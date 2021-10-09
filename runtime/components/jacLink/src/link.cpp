#include "link.hpp"
#include "muxCodec.hpp"

#include <iostream>
#include <utility>
#include <vector>
#include <algorithm>
#include <optional>

namespace {

struct ChannelDesc {
    StreamBufferHandle_t sb;
    uint8_t cid;
};

std::vector<ChannelDesc> sinkDescs;
std::vector<ChannelDesc> sourceDescs;

TaskHandle_t sinkMuxTask;
TaskHandle_t sourceDemuxTask;

EventGroupHandle_t sinkEvents;
// EventGroupHandle_t sourceEvents;
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
        if ( channelDesc == std::nullopt ) {
            // warning channel id unassigned
        }
        xEventGroupClearBits( sinkEvents, evBit );

        while ( size_t dataBytes = xStreamBufferReceive( channelDesc->sb, packetBuf + 1, sizeof( packetBuf ) - 3, 0) ) {
            packetBuf[0] = channelDesc->cid;
            size_t packetBytes = jac::link::appendCrc( packetBuf, dataBytes + 1, sizeof( packetBuf ) );
            size_t frameBytes = jac::link::encodeFrame( packetBuf, packetBytes, frameBuf, sizeof( frameBuf ) );
            std::cout.write( (const char*)frameBuf, frameBytes );
        }

        // Flush if that's all we're sending now
        if ( xEventGroupGetBits( sinkEvents ) == 0 ) {
            std::cout.flush();
        }
    }
}

void sourceDemuxRoutine( void * taskParam ) {
    static char inputBuf[300];
    while (true) {
        vTaskDelay( 1000 / portTICK_RATE_MS );
        // std::cin.readsome( inputBuf, sizeof( inputBuf ) );
    }
}

} // namespace

void jac::link::initializeLink() {
    sinkEvents = xEventGroupCreate();
    xTaskCreate( sinkMuxRoutine, "sinkMux", 3584, 0, 2, &sinkMuxTask );
    xTaskCreate( sourceDemuxRoutine, "srcDemux", 3584, 0, 2, &sourceDemuxTask );
}

void jac::link::bindSinkStreamBuffer( StreamBufferHandle_t sb, uint8_t sinkCid ) {
    assert( sinkCid <= maxCid );
    auto channelDesc = channelDescByCid( sinkDescs, sinkCid );

    if ( channelDesc.has_value() ) {
        channelDesc->sb = sb;
    } else {
        sinkDescs.push_back( {sb, sinkCid} );
    }
}

void jac::link::bindSourceStreamBuffer( StreamBufferHandle_t sb, uint8_t sourceCid ) {
    assert( sourceCid <= maxCid );
    auto channelDesc = channelDescByCid( sourceDescs, sourceCid );

    if ( channelDesc.has_value() ) {
        channelDesc->sb = sb;
    } else {
        sourceDescs.push_back( {sb, sourceCid} );
    }
}

void jac::link::notifySink( StreamBufferHandle_t sb ) {
    auto channelDesc = channelDescByStream( sinkDescs, sb );

    if ( channelDesc.has_value() ) {
        xEventGroupSetBits( sinkEvents, encodeBitPosition( channelDesc->cid ) );
    }
}
