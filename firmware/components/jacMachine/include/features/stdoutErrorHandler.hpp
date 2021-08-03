#pragma once

#include <iostream>
#include <string>

namespace jac {

template < typename Self >
class StdoutErrorHandler {
public:
    MACHINE_FEATURE_SELF();

    struct Configuration {};

    void initialize() {}

    void onEventLoop() {}

    static void fatalErrorHandler( void *udata, const char *msg ) {
        std::cout << "Fatal error occured: " << msg << "\n";
        throw std::runtime_error( std::string( msg ) );
    }

    void reportError( const std::string& msg ) {
        std::cout << "An error occured: " << msg << "\n";
        throw std::runtime_error( msg );
    }
};

} // namespace jac

