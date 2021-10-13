#pragma once

#include "link.hpp"

#include <sstream>

namespace jac::storage {

template < typename Self >
class ChannelReporter {
    ChannelDesc ch;

public:
    void bindReportChannel(const ChannelDesc &ch) {
        this->ch = ch;
    }

    void yieldBuffer( char *data, size_t len ) {
        jac::link::writeSink( ch, s, len );
    }

    void yieldString( const std::string& s ) {
        jac::link::writeSink( ch, s.c_str(), s.length() );
    }

    void yieldError( const std::string& s ) {
        std::stringstream ss;
        ss << "ERROR " << s << "\n";
        jac::link::writeSink( ch, s.c_str(), s.length() );
    }

    void yieldWarning( const std::string& s ) {
        std::stringstream ss;
        ss << "WARNING " << s << "\n";
        jac::link::writeSink( ch, s.c_str(), s.length() );
    }
};

} // namespace jac::storage
