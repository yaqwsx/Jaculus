#pragma once

#include "link.hpp"

// #include <freertos/FreeRTOS.h>
// #include <freertos/stream_buffer.h>

namespace jac::storage {

template < typename Self >
class ChannelReader {
    ChannelDesc ch;

public:
    char read() {
        // return getc( stdin );
    }

    char peek() {
        // int c;
        // c = fgetc( stdin );
        // ungetc( c, stdin );
        // return c;
    }

    void discardBufferedStdin() {
    }
};

} // namespace jac::storage