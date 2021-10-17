#include "link.hpp"
#include "encoding.hpp"
#include <jacLog.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/event_groups.h>

#include <driver/uart.h>

#include <iostream>
#include <utility>
#include <vector>
#include <algorithm>
#include <optional>
#include <array>
#include <sstream>

namespace {

using namespace jac::link;

std::vector<ChannelDesc> sinkDescs;
std::vector<ChannelDesc> sourceDescs;

TaskHandle_t sinkMuxTask;
TaskHandle_t sourceDemuxTask;

EventGroupHandle_t sinkEvents;
const uint32_t eventMask = 0x00ffffff;
const uint8_t maxCid = 23;

std::optional<ChannelDesc*> channelDescByCid( std::vector<ChannelDesc> &collection, uint8_t cid ) {
    auto it = std::find_if( collection.begin(), collection.end(),
        [=]( auto a ) { return a.cid == cid; } );
    if ( it != collection.end() ) {
        return &(*it);
    }
    return std::nullopt;
}

std::optional<ChannelDesc*> channelDescByStream( std::vector<ChannelDesc> &collection, StreamBufferHandle_t sb ) {
    auto it = std::find_if( collection.begin(), collection.end(),
        [=]( auto a ) { return a.sb == sb; } );
    if ( it != collection.end() ) {
        return &(*it);
    }
    return std::nullopt;
}

uint32_t encodeBitPosition( uint8_t cid ) {
    return 1 << cid;
}

uint32_t decodeLowestBitPosition( uint32_t bits ) {
    return __builtin_ctz( bits );
}

void sinkMuxRoutine( void * taskParam ) {
    static uint8_t packetBuf[jac::link::packetMaxSize];
    static uint8_t frameBuf[jac::link::frameMaxSize];

    while ( true ) {
        uint32_t evBits = xEventGroupWaitBits( sinkEvents, eventMask, pdFALSE, pdFALSE, portMAX_DELAY );
        assert( evBits );
        uint32_t evCid = decodeLowestBitPosition( evBits );
        uint32_t evBit = 1 << evCid;

        auto channelDesc = channelDescByCid( sinkDescs, evCid );
        xEventGroupClearBits( sinkEvents, evBit );
        if ( !channelDesc.has_value() ) {
            JAC_LOGW( "link", "Sink channel %d unassigned", evCid );
            continue;
        }

        while ( size_t dataBytes = xStreamBufferReceive( channelDesc.value()->sb, packetBuf + 1, sizeof( packetBuf ) - 3, 0) ) {
            packetBuf[0] = channelDesc.value()->cid;
            size_t packetBytes = jac::link::appendCrc( packetBuf, dataBytes + 1, sizeof( packetBuf ) );
            size_t frameBytes = jac::link::encodeFrame( packetBuf, packetBytes, frameBuf, sizeof( frameBuf ) );
            // JAC_LOGI( "link", "Tx %d", int(frameBytes) );
            uart_write_bytes( UART_NUM_0, frameBuf, frameBytes );
        }

        // Flush if that's all we're sending now
        // if ( xEventGroupGetBits( sinkEvents ) == 0 ) {
        // }
    }
}

void processSourceFrame( uint8_t *data, size_t len ) {
    static std::array<uint8_t, jac::link::packetMaxSize> packetBuf;
    // JAC_LOGI( "link", "Dec" );
    size_t packetBytes = jac::link::decodeFrame( data, len, packetBuf.data(), packetBuf.size() );
    // JAC_LOGI( "link", "DecE %d", int(packetBytes) );

    if (packetBytes > 0) {
        uint8_t cid = packetBuf[0];
        auto channelDesc = channelDescByCid( sourceDescs, cid );
        if ( !channelDesc.has_value() ) {
            JAC_LOGW( "link", "Src channel %d unassigned", cid );
            return;
        }
        // JAC_LOGI( "link", "RxValid %d: %d", int(cid), int(packetBytes) );
        size_t packetDatabytes = packetBytes - 1;
        if ( xStreamBufferSpacesAvailable( channelDesc.value()->sb ) < packetDatabytes ) {
            JAC_LOGW( "link", "Src channel %d buffer full", cid );
        }
        xStreamBufferSend( channelDesc.value()->sb, packetBuf.data() + 1, packetDatabytes, portMAX_DELAY );
        // JAC_LOGI( "link", "RxForwarded" );
    }
}

void processSourceChunk( uint8_t *data, size_t len ) {
    static std::array<uint8_t, jac::link::frameMaxSize> frameBuf;
    static size_t frameInd = 0;
    static size_t frameRem = 0;
    static bool awaitLen = false;
    // JAC_LOGI( "link", "RxChunk %d", len );

    size_t position = 0;
    while ( position < len ) {
        if (data[position] == 0) {
            awaitLen = true;
            position++;
            continue;
        }
        if (awaitLen) {
            frameRem = data[position];
            awaitLen = frameRem == 0;
            frameInd = 0;
            position++;
            continue;
        }
        if (frameRem > 0) {
            frameBuf[frameInd++] = data[position];
            if (--frameRem == 0) {
                processSourceFrame( frameBuf.data(), frameInd );
                frameInd = 0;
            }
        }
        position++;
    }
    // JAC_LOGI( "link", "RxChunkE" );
}

void sourceDemuxRoutine( void * taskParam ) {
    static std::array<uint8_t, 300> inputBuf;
    while (true) {
        int bytesRead;
        bytesRead = uart_read_bytes( UART_NUM_0, inputBuf.data(), inputBuf.size(), 1 / portTICK_RATE_MS );
        if (bytesRead > 0) {
            processSourceChunk( inputBuf.data(), bytesRead );
        }
    }
}

} // namespace

void jac::link::initializeLink() {
    sinkEvents = xEventGroupCreate();
    xTaskCreate( sinkMuxRoutine, "sinkMux", 3584, 0, 2, &sinkMuxTask );
    xTaskCreate( sourceDemuxRoutine, "srcDemux", 3584, 0, 2, &sourceDemuxTask );
}

void jac::link::bindSinkChannel( const ChannelDesc &sinkDesc ) {
    assert( sinkDesc.cid <= maxCid );
    auto channelDesc = channelDescByCid( sinkDescs, sinkDesc.cid );

    if ( channelDesc.has_value() ) {
        channelDesc.value()->sb = sinkDesc.sb;
    } else {
        sinkDescs.push_back( sinkDesc );
    }
}

void jac::link::bindSourceChannel( const ChannelDesc &sourceDesc ) {
    assert( sourceDesc.cid <= maxCid );
    auto channelDesc = channelDescByCid( sourceDescs, sourceDesc.cid );

    if ( channelDesc.has_value() ) {
        channelDesc.value()->sb = sourceDesc.sb;
    } else {
        sourceDescs.push_back( sourceDesc );
    }
}

void jac::link::writeSink( const ChannelDesc &sinkDesc, const uint8_t *data, size_t len, TickType_t timeout ) {
    // Make sure to notifySink even if we exceed the buffer
    while (len > 0) {
        size_t avail = xStreamBufferSpacesAvailable( sinkDesc.sb );
        size_t chunkBytes = std::min( len, avail );
        xStreamBufferSend( sinkDesc.sb, data, chunkBytes, timeout );
        notifySink( sinkDesc );
        len -= chunkBytes;
    }
}

size_t jac::link::readSource( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, TickType_t timeout ) {
    size_t bytes = xStreamBufferReceive( sourceDesc.sb, data, len, timeout );
    return bytes;
}

size_t jac::link::readSourceAtLeast( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, size_t atLeast, TickType_t timeout ) {
    assert( atLeast < len );
    size_t bytes = readSource( sourceDesc, data, atLeast, timeout );
    // JAC_LOGI( "link", "%d: ReadA %d", int(sourceDesc.cid), bytes );
    data += bytes;
    len -= bytes;
    // Read more bytes that may be in buffer
    bytes += readSource( sourceDesc, data, len, 0 );
    return bytes;
}

void jac::link::notifySink( const ChannelDesc &sinkDesc ) {
    xEventGroupSetBits( sinkEvents, encodeBitPosition( sinkDesc.cid ) );
}
