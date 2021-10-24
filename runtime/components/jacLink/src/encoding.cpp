#include "encoding.hpp"
#include "cobs.h"
#include "rom/crc.h"
#include "jacLog.hpp"

#include <algorithm>

#include "driver/uart.h"

uint16_t jac::link::calculateCrc( uint8_t *data, size_t len ) {
    uint16_t crc = ~crc16_be( ~0, data, len );
    return crc;
}

size_t jac::link::appendCrc( uint8_t *data, size_t dataLen, size_t bufferLen ) {
    assert( bufferLen >= dataLen + 2);
    uint8_t *crcPtr = data + dataLen;
    uint16_t crc = jac::link::calculateCrc( data, dataLen );
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

size_t jac::link::decodeFrame( const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen ) {
    auto cobsRes = cobs_decode( dest, destLen, src, srcLen );
    if ( cobsRes.status != COBS_DECODE_OK || cobsRes.out_len < 3 ) {
        JAC_LOGW( "link", "decerr %d", int(cobsRes.status) );
        return 0;
    }
    uint16_t crcRem = jac::link::calculateCrc( dest, cobsRes.out_len );
    if (crcRem != 0) {
        JAC_LOGW( "link", "crcerr" );
        return 0;
    }
    return cobsRes.out_len - 2;
}
