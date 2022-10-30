#pragma once

#include <stdint.h>
#include <stddef.h>

namespace jac::link {

inline const size_t packetDataMaxSize = 250;
inline const size_t packetHeaderSize = 2;
inline const size_t packetCrcSize = 2;
inline const size_t packetMaxSize = 254;
inline const size_t frameMaxSize = 257;
inline const size_t frameMinSize = 7;

uint16_t calculateCrc( uint8_t *data, size_t len );
size_t appendCrc( uint8_t *data, size_t dataLen, size_t bufferLen );
size_t encodeFrame( const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen );
size_t decodeFrame( const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen );

}