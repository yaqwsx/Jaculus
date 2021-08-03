#pragma once

#include <string>

namespace jac::utility {

template< template < typename > typename... Features >
class Mixin: public Features< Mixin < Features... > >... {};


inline bool startswith( const std::string& what, const std::string& with ) {
    if ( with.length() > what.length() )
        return false;
    for ( int i = 0; i != with.length(); i++ ) {
        if ( what[ i ] != with[ i ] )
            return false;
    }
    return true;
}

} // namespace jac::utility