#pragma once

#include <iostream>

namespace jac::storage {

template < typename Self >
class StdoutReporter {
public:
    void yieldBuffer( uint8_t *s, size_t len ) {
        std::cout.write( s, len );
    }

    void yieldString( const std::string& s ) {
        std::cout << s;
    }

    void yieldError( const std::string& s ) {
        std::cout << "ERROR " << s << "\n";
    }

    void yieldWarning( const std::string& s ) {
        std::cout << "WARNING " << s << "\n";
    }

};

} // namespace jac::storage