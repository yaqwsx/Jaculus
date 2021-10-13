#include "link.hpp"
#include "encoding.hpp"

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

using jac::link::ChannelDesc;

std::vector<ChannelDesc> sinkDescs;
std::vector<ChannelDesc> sourceDescs;

TaskHandle_t sinkMuxTask;
TaskHandle_t sourceDemuxTask;

EventGroupHandle_t sinkEvents;
const uint32_t eventMask = 0x00ffffff;
const uint8_t maxCid = 23;

std::optional<ChannelDesc> channelDescByCid( const std::vector<ChannelDesc> &collection, uint8_t cid ) {
    auto it = std::find_if( collection.begin(), collection.end(),
        [=]( auto a ) { return a.cid == cid; } );
    if ( it != collection.end() ) {
        return *it;
    }
    return std::nullopt;
}

std::optional<ChannelDesc> channelDescByStream( const std::vector<ChannelDesc> &collection, StreamBufferHandle_t sb ) {
    auto it = std::find_if( collection.begin(), collection.end(),
        [=]( auto a ) { return a.sb == sb; } );
    if ( it != collection.end() ) {
        return *it;
    }
    return std::nullopt;
}

bool decodeLowestBitPosition( uint32_t eventBits, uint8_t &cid, uint32_t &bit) {
    cid = 0;
    do {
        bit = 1 << cid;
        if ( bit & eventBits ) {
            return true;
        }
        cid++;
    } while ( bit & eventMask );
    bit = 0;
    return false;
}

uint32_t encodeBitPosition( uint8_t cid ) {
    return 1 << cid;
}

void sinkMuxRoutine( void * taskParam ) {
    static uint8_t packetBuf[jac::link::packetMaxSize + 3];
    static uint8_t frameBuf[jac::link::frameMaxSize];

    while ( true ) {
        uint32_t evBits = xEventGroupWaitBits( sinkEvents, eventMask, pdFALSE, pdFALSE, portMAX_DELAY );
        assert( evBits );
        uint8_t evCid;
        uint32_t evBit;
        decodeLowestBitPosition( evBits, evCid, evBit );
        auto channelDesc = channelDescByCid( sinkDescs, evCid );
        xEventGroupClearBits( sinkEvents, evBit );
        if ( !channelDesc.has_value() ) {
            // warning channel id unassigned
            continue;
        }

        while ( size_t dataBytes = xStreamBufferReceive( channelDesc->sb, packetBuf + 1, sizeof( packetBuf ) - 3, 0) ) {
            packetBuf[0] = channelDesc->cid;
            size_t packetBytes = jac::link::appendCrc( packetBuf, dataBytes + 1, sizeof( packetBuf ) );
            size_t frameBytes = jac::link::encodeFrame( packetBuf, packetBytes, frameBuf, sizeof( frameBuf ) );
            uart_write_bytes( UART_NUM_0, frameBuf, frameBytes );
        }

        // Flush if that's all we're sending now
        // if ( xEventGroupGetBits( sinkEvents ) == 0 ) {
        // }
    }
}

void processSourceFrame( uint8_t *data, size_t len ) {
    static std::array<uint8_t, jac::link::packetMaxSize> packetBuf;
    size_t packetBytes = jac::link::decodeFrame( data, len, packetBuf.data(), packetBuf.size() );
    if (packetBytes > 0) {
        uint8_t cid = packetBuf[0];
        auto channelDesc = channelDescByCid( sourceDescs, cid );
        if ( !channelDesc.has_value() ) {
            // warning channel id unassigned
            return;
        }
        xStreamBufferSend( channelDesc->sb, packetBuf.data() + 1, packetBytes - 1, portMAX_DELAY );
    }
}

void processSourceChunk( uint8_t *data, size_t len ) {
    static std::array<uint8_t, jac::link::frameMaxSize> frameBuf;
    static size_t frameInd = 0;
    static size_t frameRem = 0;
    static bool awaitLen = false;

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
        channelDesc->sb = sinkDesc.sb;
    } else {
        sinkDescs.push_back( sinkDesc );
    }
}

void jac::link::bindSourceChannel( const ChannelDesc &sourceDesc ) {
    assert( sourceDesc.cid <= maxCid );
    auto channelDesc = channelDescByCid( sourceDescs, sourceDesc.cid );

    if ( channelDesc.has_value() ) {
        channelDesc->sb = sourceDesc.sb;
    } else {
        sourceDescs.push_back( sourceDesc );
    }
}

void jac::link::writeSink( const ChannelDesc &sinkDesc, const uint8_t *data, size_t len ) {
    xStreamBufferSend( sinkDesc.sb, data, len, portMAX_DELAY );
    notifySink( sinkDesc );
}

size_t jac::link::readSource( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, TickType_t timeout = portMAX_DELAY ) {
    size_t bytes = xStreamBufferReceive( sourceDesc.sb, data, len, timeout );
    return bytes;
}

void jac::link::notifySink( const ChannelDesc &sinkDesc ) {
    xEventGroupSetBits( sinkEvents, encodeBitPosition( sinkDesc.cid ) );
}
