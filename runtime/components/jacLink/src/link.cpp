#include "link.hpp"
#include "muxCodec.hpp"

#include <iostream>
#include <utility>
#include <vector>
#include <algorithm>
#include <optional>

namespace {

struct StreamTuple {
    StreamBufferHandle_t sb;
    uint8_t id;
};

std::vector<StreamTuple> sinkIds;
std::vector<StreamTuple> sourceIds;

TaskHandle_t sinkMuxTask;
TaskHandle_t sourceDemuxTask;

EventGroupHandle_t sinkEvents;
EventGroupHandle_t sourceEvents;
const uint32_t eventMask = 0x00ffffff;

}

std::optional<StreamTuple> streamById( const std::vector<StreamTuple> &collection, uint8_t id ) {
    auto it = std::find_if( collection.begin(), collection.end(),
        [=]( auto a ) { return a.id == id; } );
    if ( it != collection.end() ) {
        return *it;
    }
    return std::nullopt;
}

bool decodeLowestBitPosition( uint32_t eventBits, int &id, uint8_t &bit) {
    id = 0;
    do {
        bit = 1 << id;
        if ( bit & eventBits ) {
            return true;
        }
        id++;
    } while ( bit & eventMask );
    return false;
}

void sinkMuxRoutine( void * taskParam ) {
    static uint8_t packetBuf[jac::link::inputMaxSize];
    static uint8_t frameBuf[jac::link::inputMaxSize];

    while ( true ) {
        uint32_t evBits = xEventGroupWaitBits( sinkEvents, eventMask, pdFALSE, pdFALSE, portMAX_DELAY );
        assert( evBits );
        int evId;
        uint8_t evBit;
        decodeLowestBitPosition( evBits, evId, evBit );
        auto streamTuple = streamById( sinkIds, evId );
        if ( streamTuple == std::nullopt ) {
            // warning channel id unassigned
        }
        xEventGroupClearBits( sinkEvents, evBit );

        while ( size_t packetBytes = xStreamBufferReceive( streamTuple->sb, packetBuf + 1, sizeof( packetBuf - 1), 0) ) {
            packetBuf[0] = streamTuple->id;
            size_t frameBytes = jac::link::encode( packetBuf, packetBytes + 1, frameBuf, sizeof (frameBuf) );
            std::cout.write( (const char*)frameBuf, frameBytes );
        }
    }
}

void jac::link::initializeLink() {
    xTaskCreate( sinkMuxRoutine, "sinkMux", 3584, 0, 2, &sinkMuxTask );
}

void jac::link::bindSinkStreamBuffer( StreamBufferHandle_t sb, uint8_t sinkId ) {
    auto it = std::find_if( sinkIds.begin(), sinkIds.end(),
        [=](auto a) { return a.id == sinkId; });

    if ( it != sinkIds.end() ) {
        it->sb = sb;
    } else {
        sinkIds.push_back( {sb, sinkId} );
    }
}

void jac::link::bindSourceStreamBuffer( StreamBufferHandle_t sb, uint8_t sourceId ) {
    auto it = std::find_if( sourceIds.begin(), sourceIds.end(),
        [=](auto a) { return a.id == sourceId; });

    if ( it != sourceIds.end() ) {
        it->sb = sb;
    } else {
        sourceIds.push_back( {sb, sourceId} );
    }
}
