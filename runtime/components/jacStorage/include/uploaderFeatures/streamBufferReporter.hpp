#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

#include <sstream>

namespace jac::storage {

template < typename Self >
class StreamBufferReporter {
    StreamBufferHandle_t sb;

public:
    void bindReportStreamBuffer(StreamBufferHandle_t sb) {
        this->sb = sb;
    }

    void yieldBuffer( char *s, size_t len ) {
        xStreamBufferSend(sb, s, len, portMAX_DELAY);
    }

    void yieldString( const std::string& s ) {
        xStreamBufferSend(sb, s.c_str(), s.length(), portMAX_DELAY);
    }

    void yieldError( const std::string& s ) {
        std::stringstream ss;
        ss << "ERROR " << s << "\n";
        xStreamBufferSend(sb, ss.str().c_str(), ss.str().length(), portMAX_DELAY);
    }

    void yieldWarning( const std::string& s ) {
        std::stringstream ss;
        ss << "WARNING " << s << "\n";
        xStreamBufferSend(sb, ss.str().c_str(), ss.str().length(), portMAX_DELAY);
    }
};

} // namespace jac::storage
