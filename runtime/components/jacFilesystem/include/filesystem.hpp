#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cassert>
#include <cstring> // strerror

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// There are missing guards, fixed in
// https://github.com/espressif/esp-idf/commit/cbf207bfb83156ece449a10908cad0615d66ec52
extern "C" {
    #include <sys/types.h>
    #include <sys/dirent.h>
    #include <dirent.h>
}

namespace jac::fs {

class Path {
public:
    Path() = default;
    Path( const std::string& s ): Path( s.c_str() ) {}
    Path( const char* s ) {
        _split( s );
    }
    Path( const Path& p ) = default;
    Path( Path&& p ) = default;
    Path& operator=( const Path& p ) = default;
    Path& operator=( Path&& p ) = default;

    Path& operator/=( const Path& o ) {
        std::copy( o._path.begin(), o._path.end(), std::back_inserter( _path ) );
        return *this;
    }

    Path operator/( const Path& o ) const {
        Path copy( *this );
        copy /= o;
        return copy;
    }

    std::string& operator[]( int idx ) {
        if ( idx < 0 )
            idx = _path.size() - idx;
        return _path[ idx ];
    }

    void cutFront( int len ) {
        _path.erase( _path.begin(), _path.begin() + len );
    }

    bool absolute() const {
        assert( !_path.empty() );
        return _path[ 0 ].empty();
    }

    size_t size() const {
        return _path.size();
    }

    std::string str() const {
        std::string ret;
        const char *delimiter = "";
        for ( const auto& chunk : _path ) {
            ret += delimiter;
            ret += chunk;
            delimiter = "/";
        }
        return ret;
    }

    // It would be nice to have a version without copy - i.e., PathView
    Path dirname() const {
        if ( _path.empty() )
            return {};
        return { std::vector< std::string >( _path.begin(), _path.end() - 1 ) };
    }

    std::string basename() const {
        assert( !_path.empty() );
        return _path.back();
    }

    // Remove ./ and ../ from the path. If the path is invalid, throw an exception
    Path weakNormalForm() const {
        std::vector< std::string > newPath;
        for ( const auto& chunk : _path ) {
            if ( chunk == "." )
                continue;
            else if ( chunk == ".." ) {
                if ( newPath.empty() )
                    throw std::runtime_error( "Invalid path" );
                newPath.pop_back();
                continue;
            }
            newPath.push_back( chunk );
        }
        return { std::move( newPath ) };
    }

private:
    Path( std::vector< std::string >&& chunks ): _path( std::move( chunks ) ) {}

    void _split( const char *path ) {
        std::string chunk;
        const char *c = path;
        while ( *c ) {
            if ( *c == '/' )
                _path.push_back( std::move( chunk ) );
            else
                chunk.push_back( *c );
            c++;
        }
        _path.push_back( chunk );
    }

    std::vector< std::string > _path;
};

enum class FileType { File, Directory };

// Recursively walk given path and yield all directories and files.
// The yield function receives two arguments:
// - FileType type
// - std::string path
// - std::string entity name
template < typename Yield, typename Error >
void listDirectory( const std::string& path, Yield yield, Error yieldError ) {
    DIR *dir;
    struct dirent *entry;

    if ( ! ( dir = ::opendir( path.c_str() ) ) ) {
        yieldError( std::strerror( errno ) );
        return;
    }

    while ( ( entry = ::readdir( dir ) ) != nullptr ) {
        std::string entityName( entry->d_name );
        if ( entityName == "." || entityName == ".." )
            continue;
        if ( entry->d_type == DT_DIR ) {
            listDirectory( path + "/" + entityName, yield, yieldError );
            yield( FileType::Directory, path, entityName );
        }
        else if ( entry->d_type == DT_REG ) {
            yield( FileType::File, path, entityName );
        }
    }
    ::closedir( dir );
}

// Ensure that directory path to a file exists. Return true if successfull.
bool ensurePath( const std::string& path );
std::string concatPath( std::string a, const std::string& b );
std::string readFile( const std::string& path );
void readFile( const std::string& path, std::string& output );
bool fileExists( const std::string& path );

} // namespace jac::fs
