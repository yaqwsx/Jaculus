#pragma once

#include <stdint.h>
#include <stddef.h>

namespace jac::link {

inline const size_t packetMaxSize = 254;
inline const size_t frameMaxSize = 1 + 1 + 1 + 256 + 2;

size_t appendCrc( uint8_t *data, size_t dataLen, size_t bufferLen );
size_t encodeFrame( const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen );

}