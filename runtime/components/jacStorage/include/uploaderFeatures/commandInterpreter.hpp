#pragma once

#include <jacLog.hpp>

#include <memory>
#include <string>
#include <mbedtls/base64.h>

namespace jac::storage {

template < typename Self >
class CommandInterpreter {
public:
    friend Self;
    Self& self() {
        return *static_cast< Self* >( this );
    }

    const Self& self() const {
        return *static_cast< const Self* >( this );
    }

    /**
     * Reads a command from input and interprets it on-the-fly.
     * Always ensures that a whole command was read. **/
    void interpretCommand() {
        JAC_LOGI( "uploader", "interpreting" );
        discardWhitespace();
        std::string command = readWord( 256 );
        if ( command.length() == 256 ) {
            self().yieldError(
                "Unknown command, with prefix '" + command +
                "' and length " + std::to_string(command.length() ) +
                ". Ignoring rest" );
            discardRest();
            return;
        }
        discardWhitespace();
        if ( command == "LIST" )
            return interpretList();
        if ( command == "PULL" )
            return interpretPull();
        if ( command == "PUSH" )
            return interpretPush();
        if ( command == "REMOVE" )
            return interpretRemove();
        if ( command == "STATS" )
            return interpretStats();
        if ( command == "EXIT" )
            return interpretExit();
        if ( !command.empty() )
            self().yieldError( "Unknown command '" + command + "'" );
        discardRest();
    }

    void interpretList() {
        JAC_LOGI( "uploader", "LIST" );
        std::string prefix = readWord(); // It is OK if it is empty!
        if ( prefix.front() != '/' )
            prefix.insert( 0, "/" );
        self().doList( prefix );
        discardRest();
    }

    void interpretPull() {
        std::string filename = readWord();
        if ( filename.empty() ) {
            self().yieldError( "Missing name of the file to pull" );
            discardRest();
            return;
        }
        self().doPull( filename );
        discardRest();
    }

    void interpretPush() {
        std::string filename = readWord();
        if ( filename.empty() ) {
            self().yieldError( "Missing name of the file to push" );
            discardRest();
            return;
        }
        self().startFilePush();

        discardWhitespace();
        const int BLOCK_SIZE = 63;
        std::string chunk;
        std::unique_ptr< unsigned char[] > chunkBuffer( new unsigned char[ BLOCK_SIZE ] );
        do {
            const int base64BlockSize =  4 * BLOCK_SIZE / 3;
            static_assert( base64BlockSize % 4 == 0 );
            chunk = readWord( base64BlockSize );
            size_t chunklength;
            int retcode = mbedtls_base64_decode(
                chunkBuffer.get(), BLOCK_SIZE, &chunklength,
                reinterpret_cast< unsigned char * >( chunk.data() ), chunk.length() );
            assert( retcode != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL );
            if ( retcode == MBEDTLS_ERR_BASE64_INVALID_CHARACTER ) {
                self().yieldError( "Invalid characted in base64 encoding specified" );
                discardRest();
                return;
            }
            self().addFileChunk( chunkBuffer.get(), chunklength );
        } while ( !chunk.empty() );

        discardWhitespace();
        if ( !shift('\n') ) {
            self().yieldError(" Nothing was expected" );
            discardRest();
            return;
        }
        self().commitFilePush( filename );
    }

    void interpretRemove() {
        std::string filename = readWord();
         if ( filename.empty() ) {
            self().yieldError( "Missing name of the file to delete" );
            discardRest();
            return;
        }
        self().doRemove( filename );
        discardRest();
    }

    void interpretExit() {
        self().performExit();
        discardRest();
    }

    void interpretStats() {
        self().doStats();
        discardRest();
    }

    // Consume rest of the command
    void discardRest() {
        while ( self().read() != '\n' );
    }

    void discardWord() {
        while ( true ) {
            char c = self().peek();
            if ( std::isspace( c ) )
                break;
            self().read();
        }
    }

    void discardWhitespace() {
        while ( true ) {
            char c = self().peek();
            if ( !std::isspace( c ) || c == '\n' )
                return;
            self().read();
        }
    }

    std::string readWord( int limit = 256 ) {
        std::string word;
        word.reserve( limit );
        while ( !std::isspace( self().peek() ) && word.length() < limit ) {
            word.push_back( self().read() );
        }
        return word;
    }

    bool shift( char c ) {
        char input = self().peek();
        if ( input == c ) {
            self().read();
            return true;
        }
        self().yieldError( "Expected '"s + c + "' got '" + input + "'" );
        discardRest();
        return false;
    }
};

} // namespace jac::storage