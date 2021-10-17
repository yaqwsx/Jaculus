#pragma once

#include <link.hpp>

#include <sstream>
#include <string_view>

namespace jac::storage {

template < typename Self >
class ChannelReporter {
    jac::link::ChannelDesc ch;

public:
    void bindReporterChannel(const jac::link::ChannelDesc &ch) {
        this->ch = ch;
    }

    void yieldBuffer( const uint8_t *data, size_t len ) {
        jac::link::writeSink( ch, data, len );
    }

    void yieldString( const std::string_view& sv ) {
        jac::link::writeSink( ch, reinterpret_cast< const uint8_t *>( sv.data() ), sv.length() );
    }

    void yieldError( const std::string_view& sv ) {
        std::stringstream ss;
        ss << "ERROR " << sv << "\n";
        jac::link::writeSink( ch, reinterpret_cast< const uint8_t *>( sv.data() ), sv.length() );
    }

    void yieldWarning( const std::string_view& sv ) {
        std::stringstream ss;
        ss << "WARNING " << sv << "\n";
        jac::link::writeSink( ch, reinterpret_cast< const uint8_t *>( ss.str().c_str() ), ss.str().length() );
    }
};

} // namespace jac::storage
