#include <filesystem.hpp>
#include <cassert>
#include <stdexcept>

bool jac::fs::ensurePath( const std::string& path ) {
    assert( path.length() > 0 );
    size_t pos = 1;
    while ( ( pos = path.find( '/', pos ) ) != std::string::npos ) {
        std::string dirPath = path.substr( 0, pos );
        pos++;
        int res = mkdir( dirPath.c_str(), 0777 );
        if ( res == 0 || errno == EEXIST )
            continue;
        return false;
    }
    return true;
}

std::string jac::fs::concatPath( std::string a, const std::string& b ) {
    if ( a.back() != '/' && b.front() != '/' )
        a.push_back( '/' );
    a += b;
    return a;
}

std::string jac::fs::readFile( const std::string& path ) {
    std::string fContent;
    readFile(path, fContent);
    return fContent;
}

void jac::fs::readFile( const std::string& path, std::string& output ) {
    int fileFd = open( path.c_str(), O_RDONLY );
    if ( fileFd < 0 )
        throw std::runtime_error( "Cannot open " + path + ": " + std::strerror( errno ) );
    int bytesRead;
    do {
        const int CHUNK_SIZE = 64;
        char buffer[ CHUNK_SIZE ];
        bytesRead = read( fileFd, buffer, CHUNK_SIZE );
        if ( bytesRead < 0 ) {
            close( fileFd );
            throw std::runtime_error( "Cannot read " + path + ": " + std::strerror( errno ) );
        }
        std::copy_n( buffer, bytesRead, std::back_inserter( output ) );
    } while ( bytesRead != 0 );
    close( fileFd );
}

bool jac::fs::fileExists( const std::string& path ) {
    int fileFd = open( path.c_str(), O_RDONLY );
    bool exists = fileFd >= 0;
    close( fileFd );
    return exists;
}
