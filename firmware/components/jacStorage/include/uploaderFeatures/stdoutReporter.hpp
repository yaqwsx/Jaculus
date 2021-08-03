#pragma once

#include <iostream>

namespace jac::storage {

template < typename Self >
class StdoutReporter {
public:
    void yieldError( const std::string& s ) {
        std::cout << "ERROR " << s << "\n";
    }

    void yieldWarning( const std::string& s ) {
        std::cout << "WARNING " << s << "\n";
    }

};

} // namespace jac::storage