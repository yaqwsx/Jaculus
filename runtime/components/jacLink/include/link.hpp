#pragma once

#include <stdint.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

namespace jac::link {

struct ChannelDesc {
    StreamBufferHandle_t sb;
    uint8_t cid;
};

void initializeLink();
ChannelDesc createSinkChannel( uint8_t channelId, size_t bufferSize = 2048 );
ChannelDesc createSourceChannel( uint8_t channelId, size_t bufferSize = 2048 );

void writeSink( const ChannelDesc &sinkDesc, const uint8_t *data, size_t len, TickType_t timeout = portMAX_DELAY );
size_t readSource( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, TickType_t timeout = portMAX_DELAY );
size_t readSourceAtLeast( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, size_t atLeast, TickType_t timeout = portMAX_DELAY );
void discardSourceContent( const ChannelDesc &sourceDesc );


void notifySink( const ChannelDesc &sinkDesc );

}
