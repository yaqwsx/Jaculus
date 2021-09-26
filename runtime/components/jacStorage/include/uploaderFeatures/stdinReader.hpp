#pragma once

#include <cstdio>

namespace jac::storage {

template < typename Self >
class StdinReader {
public:
    char read() {
        return getc( stdin );
    }

    char peek() {
        int c;
        c = fgetc( stdin );
        ungetc( c, stdin );
        return c;
    }

    void discardBufferedStdin() {
        std::cin.ignore( std::cin.rdbuf()->in_avail() );
    }
};

} // namespace jac::storage