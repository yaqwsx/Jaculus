#include "link.hpp"
#include "encoding.hpp"
#include <jacLog.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

#include <driver/uart.h>
#include <sys_arch.h>

#include <utility>
#include <vector>
#include <algorithm>
#include <optional>
#include <array>
#include <memory>
#include <sstream>

namespace {

using namespace jac::link;

std::vector<ChannelDesc> sinkDescs;
std::vector<ChannelDesc> sourceDescs;

TaskHandle_t sinkMuxTask;
TaskHandle_t sourceDemuxTask;

EventGroupHandle_t sinkEvents;
const uint32_t eventMask = 0x00ffffff;
const uint32_t sinkChannelNotifyMask = 0x007fffff;
const uint32_t rxWindowUpdateEvent = 0x00800000;
const uint8_t maxCid = 22;


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

uint8_t getRxWindow() {
    static const size_t rxBufferCapacity = 4096;
    size_t rxBufferBytes; 
    uart_get_buffered_data_len( UART_NUM_0, &rxBufferBytes );

    size_t rxBufferFree = rxBufferCapacity - rxBufferBytes;
    // 0 -> buffer full, 15 -> buffer empty
    return rxBufferFree / (rxBufferCapacity / 15);
}

void sinkMuxRoutine( void * taskParam ) {
    auto frameBuf = std::make_unique< uint8_t[] >( frameMaxSize );
    auto packetBuf = std::make_unique< uint8_t[] >( packetMaxSize );

    auto transmitPacket = [&]( uint8_t *packet,  size_t packetLen ) {
        size_t packetBytes = jac::link::appendCrc( packet, packetLen, packetMaxSize );

        size_t frameBytes = jac::link::encodeFrame(
            packet, packetBytes, frameBuf.get(), frameMaxSize );

        uart_write_bytes( UART_NUM_0, frameBuf.get(), frameBytes );
    };

    while ( true ) {
        uint32_t evBits = xEventGroupWaitBits( sinkEvents, eventMask, pdFALSE, pdFALSE, portMAX_DELAY );
        assert( evBits );
        uint8_t serviceByte = getRxWindow();

        if ( evBits & sinkChannelNotifyMask ) {
            uint32_t evCid = decodeLowestBitPosition( evBits );
            uint32_t evBit = encodeBitPosition( evCid );

            auto channelDesc = channelDescByCid( sinkDescs, evCid );
            xEventGroupClearBits( sinkEvents, evBit );

            if ( !channelDesc.has_value() ) {
                JAC_LOGW( "link", "Sink channel %d unassigned", evCid );
            } else if ( size_t packetDataBytes = xStreamBufferReceive(
                    channelDesc.value()->sb,
                    packetBuf.get() + packetHeaderSize,
                    packetDataMaxSize,
                    0 ) ) {

                packetBuf.get()[0] = serviceByte;
                packetBuf.get()[1] = channelDesc.value()->cid;

                // RxWindow will be piggy backed onto data frame
                xEventGroupClearBits( sinkEvents, rxWindowUpdateEvent );
                transmitPacket( packetBuf.get(), packetHeaderSize + packetDataBytes );
                continue;
            }
        }

        if ( evBits & rxWindowUpdateEvent ) {
            xEventGroupClearBits( sinkEvents, rxWindowUpdateEvent );
            packetBuf.get()[0] = serviceByte;
            packetBuf.get()[1] = 0;
            transmitPacket( packetBuf.get(), packetHeaderSize );
        }
    }
}

void processSourceFrame( uint8_t *data, size_t len ) {
    static std::array<uint8_t, jac::link::packetMaxSize> packetBuf;
    // JAC_LOGI( "link", "Dec" );
    size_t packetBytes = jac::link::decodeFrame( data, len, packetBuf.data(), packetBuf.size() );
    // JAC_LOGI( "link", "DecE %d", int(packetBytes) );

    if (packetBytes >= packetHeaderSize) {
        uint8_t serviceByte = packetBuf[0];
        uint8_t cid = packetBuf[1];
        
        auto channelDesc = channelDescByCid( sourceDescs, cid );
        if ( !channelDesc.has_value() ) {
            JAC_LOGW( "link", "Src channel %d unassigned", cid );
            return;
        }
        JAC_LOGI( "link", "RxValid %d: %d", int(cid), int(packetBytes) );
        size_t packetDatabytes = packetBytes - packetHeaderSize;
        if ( xStreamBufferSpacesAvailable( channelDesc.value()->sb ) < packetDatabytes ) {
            JAC_LOGW( "link", "Src channel %d buffer full", cid );
        }
        xStreamBufferSend(
            channelDesc.value()->sb, packetBuf.data() + packetHeaderSize, packetDatabytes, portMAX_DELAY );
        JAC_LOGI( "link", "RxForw %d", xStreamBufferSpacesAvailable( channelDesc.value()->sb ) );
    }
}

void processSourceChunk( uint8_t *data, size_t len ) {
    static std::array<uint8_t, jac::link::frameMaxSize> frameBuf;
    static size_t frameInd = 0;
    static size_t frameRem = 0;
    static bool awaitLen = false;
    JAC_LOGI( "link", "RxChunk %d", len );

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
    static const size_t inputBufLen = 300;
    auto inputBuf = std::make_unique< uint8_t[] >( inputBufLen );
    while (true) {
        int bytesRead = uart_read_bytes( UART_NUM_0, inputBuf.get(), inputBufLen, 1 / portTICK_RATE_MS );
        if (bytesRead > 0) {
            processSourceChunk( inputBuf.get(), bytesRead );
            xEventGroupSetBits( sinkEvents, rxWindowUpdateEvent );
            size_t buffered = 0;
            uart_get_buffered_data_len( UART_NUM_0, &buffered );
            JAC_LOGI( "link", "RxBuf %u", (4096 - buffered) );
        }
    }
}

} // namespace

void jac::link::initializeLink() {
    sinkEvents = xEventGroupCreate();
    xTaskCreate( sinkMuxRoutine, "sinkMux", 3584, 0, 2, &sinkMuxTask );
    xTaskCreate( sourceDemuxRoutine, "srcDemux", 3584, 0, 2, &sourceDemuxTask );
}

ChannelDesc jac::link::createSinkChannel( uint8_t channelId, size_t bufferSize ) {
    assert( channelId <= maxCid );
    auto existingChannel = channelDescByCid( sinkDescs, channelId );

    if ( existingChannel.has_value() ) {
        JAC_LOGE( "link", "Sink channel %d already exists", channelId );
    }
    
    auto channel = ChannelDesc { xStreamBufferCreate( bufferSize, 0 ), channelId } ;
    sinkDescs.push_back( channel );
    return channel;
}

ChannelDesc jac::link::createSourceChannel( uint8_t channelId, size_t bufferSize ) {
    assert( channelId <= maxCid );
    auto existingChannel = channelDescByCid( sourceDescs, channelId );

    if ( existingChannel.has_value() ) {
        JAC_LOGE( "link", "Source channel %d already exists", channelId );
    }
    
    auto channel = ChannelDesc { xStreamBufferCreate( bufferSize, 0 ), channelId } ;
    sourceDescs.push_back( channel );
    return channel;
}

void jac::link::discardSourceContent( const ChannelDesc &sourceDesc ) {
    static std::array<uint8_t, 128> foo;
    while ( xStreamBufferReceive( sourceDesc.sb, foo.data(), foo.size(), 0 ) > 0 );
    // xStreamBufferReset crashes and burns
}

void jac::link::writeSink( const ChannelDesc &sinkDesc, const uint8_t *data, size_t len, TickType_t timeout ) {
    // Make sure to notifySink even if we exceed the buffer
    while (len > 0) {
        size_t avail = xStreamBufferSpacesAvailable( sinkDesc.sb );
        size_t chunkBytes = avail > 0 ? std::min( len, avail ) : len;
        if (xStreamBufferSend( sinkDesc.sb, data, chunkBytes, timeout ) > 0) {
            notifySink( sinkDesc );
        }
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
    data += bytes;
    len -= bytes;
    // Read more bytes that may be in buffer
    bytes += readSource( sourceDesc, data, len, 0 );
    return bytes;
}

void jac::link::notifySink( const ChannelDesc &sinkDesc ) {
    xEventGroupSetBits( sinkEvents, encodeBitPosition( sinkDesc.cid ) );
}
