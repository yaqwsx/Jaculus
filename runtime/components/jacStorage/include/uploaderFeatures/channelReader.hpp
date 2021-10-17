#pragma once

#include <link.hpp>
#include <jacLog.hpp>

#include <array>

namespace jac::storage {

template < typename Self >
class ChannelReader {
    jac::link::ChannelDesc ch;
    std::array<uint8_t, 32> buf;
    uint8_t *bufIter = buf.begin();
    uint8_t *bufEnd = buf.begin();

public:
    void bindReaderChannel( const jac::link::ChannelDesc &ch ) {
        this->ch = ch;
    }

    char read() {
        // JAC_LOGI( "RDR", "Read" );
        if ( bufIter == bufEnd ) {
            // JAC_LOGI( "RDR", "Fetch" );
            size_t bytes = jac::link::readSourceAtLeast( ch, buf.data(), buf.size(), 1 );
            bufIter = buf.begin();
            bufEnd = buf.begin() + bytes;
        }
        return *bufIter++;
    }

    char peek() {
        // JAC_LOGI( "RDR", "Peek" );
        if ( bufIter == bufEnd ) {
            // JAC_LOGI( "RDR", "Fetch" );
            size_t bytes = jac::link::readSourceAtLeast( ch, buf.data(), buf.size(), 1 );
            bufIter = buf.begin();
            bufEnd = buf.begin() + bytes;
        }
        return *bufIter;
    }

    void discardBufferedInput() {
        bufIter = buf.begin();
        bufEnd = buf.begin();
    }
};

} // namespace jac::storage