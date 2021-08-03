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
};

} // namespace jac::storage