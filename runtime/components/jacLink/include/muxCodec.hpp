#pragma once

#include <stdint.h>
#include <stddef.h>

namespace jac::link {

inline const size_t inputMaxSize = 254;
inline const size_t frameMaxSize = 1 + 1 + 1 + 256 + 2;

size_t encode( uint8_t channelId, const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen );

}