#pragma once

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/event_groups.h>

namespace jac::link {

void initializeLink();
void bindSinkStreamBuffer(StreamBufferHandle_t sb, uint8_t sinkId);
void bindSourceStreamBuffer(StreamBufferHandle_t sb, uint8_t sinkId);

}
