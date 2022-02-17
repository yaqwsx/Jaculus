#pragma once

#include <link.hpp>
#include <jacLog.hpp>

#include <array>

namespace jac::storage {

template < typename Self >
class ChannelReader {
    const jac::link::ChannelDesc *ch;
    std::array<uint8_t, 32> buf;
    uint8_t *bufIter = buf.begin();
    uint8_t *bufEnd = buf.begin();

public:
    void bindReaderChannel( const jac::link::ChannelDesc *ch ) {
        this->ch = ch;
    }

    char read() {
        JAC_LOGV( "RDR", "Read" );
        if ( bufIter == bufEnd ) {
            JAC_LOGD( "RDR", "Fetch" );
            size_t bytes = jac::link::readSourceAtLeast( *ch, buf.data(), buf.size(), 1 );
            bufIter = buf.begin();
            bufEnd = buf.begin() + bytes;
        }
        return *bufIter++;
    }

    char peek() {
        JAC_LOGV( "RDR", "Peek" );
        if ( bufIter == bufEnd ) {
            JAC_LOGD( "RDR", "Fetch" );
            size_t bytes = jac::link::readSourceAtLeast( *ch, buf.data(), buf.size(), 1 );
            bufIter = buf.begin();
            bufEnd = buf.begin() + bytes;
        }
        return *bufIter;
    }

    void discardBufferedInput() {
        bufIter = buf.begin();
        bufEnd = buf.begin();
        jac::link::discardSourceContent( *ch );
    }
};

} // namespace jac::storage
