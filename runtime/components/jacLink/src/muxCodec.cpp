#include "muxCodec.hpp"
#include "cobs.h"
#include "rom/crc.h"

#include <algorithm>

size_t jac::link::appendCrc( uint8_t *data, size_t dataLen, size_t bufferLen ) {
    assert( bufferLen >= dataLen + 2);

    uint16_t crc = ~crc16_be( ~0, data, dataLen );
    uint8_t *crcPtr = data + dataLen;
    crcPtr[0] = uint8_t( crc >> 8 );
    crcPtr[1] = uint8_t( crc );
    return dataLen + 2;
}

size_t jac::link::encodeFrame( const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen ) {
    assert( destLen >= jac::link::frameMaxSize );
    assert( srcLen <= jac::link::packetMaxSize );
    dest[0] = 0;

    uint8_t *cobsPtr = dest + 2;
    auto cobsRes = cobs_encode( cobsPtr, 255, src, srcLen );
    assert( cobsRes.status == COBS_ENCODE_OK );
    assert( cobsRes.out_len <= 255 );
    dest[1] = cobsRes.out_len;

    return 2 + cobsRes.out_len;
}
