#include "muxCodec.hpp"
#include "cobs.h"
#include "rom/crc.h"

#include <algorithm>


size_t jac::link::encode( const uint8_t *src, size_t srcLen, uint8_t *dest, size_t destLen ) {
    assert( destLen >= jac::link::frameMaxSize );
    assert( srcLen <= jac::link::inputMaxSize );
    dest[0] = 0;

    uint8_t *cobsPtr = dest + 3;
    auto cobsRes = cobs_encode( cobsPtr, 255, src, srcLen );
    assert( cobsRes.status == COBS_ENCODE_OK );
    assert( cobsRes.out_len <= 255 );
    dest[1] = cobsRes.out_len;

    uint8_t *crcPtr = dest + 3 + cobsRes.out_len;
    uint16_t crc = crc16_be( 0, dest, 3 + cobsRes.out_len );
    crcPtr[0] = uint8_t( crc >> 8 );
    crcPtr[1] = uint8_t( crc );

    return 2 + cobsRes.out_len + 2;
}
