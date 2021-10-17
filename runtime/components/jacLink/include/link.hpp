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
void bindSinkChannel( const ChannelDesc &sinkDesc );
void bindSourceChannel( const ChannelDesc &sourceDesc );

void writeSink( const ChannelDesc &sinkDesc, const uint8_t *data, size_t len, TickType_t timeout = portMAX_DELAY );
size_t readSource( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, TickType_t timeout = portMAX_DELAY );
size_t readSourceAtLeast( const ChannelDesc &sourceDesc, uint8_t *data, size_t len, size_t atLeast, TickType_t timeout = portMAX_DELAY );

void notifySink( const ChannelDesc &sinkDesc );

}