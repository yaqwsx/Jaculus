#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace jac {

using namespace std::string_literals;

// Implement a simple debugger feature over sockets. This is more-less only a
// proof of concept implementation, it should be rewritten for serious usage
template < typename Self >
class SocketDebugger {
public:
    MACHINE_FEATURE_SELF();

    ~SocketDebugger() {
        if ( _clientSocket >= 0 )
            close( _clientSocket );
        if ( _serverSocket >= 0 )
            close( _serverSocket );
    }

    struct Configuration {
        int debuggerPort = 3333;
    };

    void initialize() {}

    void onEventLoop() {
        if ( _debuggingEnabled )
            duk_debugger_cooperate( self()._context );
    }

    void waitForDebugger() {
        _initializeServerSocket();
        _waitForClient();
        _attachDebugger();
    }


private:
    void _attachDebugger() {
        duk_debugger_attach( self()._context,
                    dukReadCb,         // read callback
                    dukWriteCb,        // write callback
                    dukPeekCb,         // peek callback
                    dukReadFlushCb,    // read flush callback
                    dukWriteFlushCb,   // write flush callback
                    nullptr,           // app request callback
                    dukDeatachCb,      // debugger detached callback
                    &self() );         // debug udata
        _debuggingEnabled = true;
        std::cout << "Debugger attached!\n";
    }

    void _initializeServerSocket() {
        _serverSocket = socket( AF_INET, SOCK_STREAM, 0 );
        if ( _serverSocket < 0 ) {
            throw std::runtime_error( "Cannot open server socket: "s + std::strerror( errno ) );
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons( self()._cfg.debuggerPort );
        if ( bind( _serverSocket, reinterpret_cast< sockaddr *>( &addr ), sizeof( addr ) ) < 0 ) {
            throw std::runtime_error( "Cannot bind server socket: "s + std::strerror( errno ) );
        }

        if ( listen( _serverSocket, 1 )  < 0 ) {
            throw std::runtime_error( "Cannot listen server socket: "s + std::strerror( errno ) );
        }
    }

    void _waitForClient() {
        assert( _serverSocket >= 0 );
        if ( _clientSocket >= 0 )
		    close( _clientSocket );

        sockaddr_in addr;
	    socklen_t sz;
        sz = (socklen_t) sizeof( addr );
	    _clientSocket = accept( _serverSocket, (sockaddr *) &addr, &sz );
	    if ( _clientSocket < 0 ) {
            throw std::runtime_error( "Cannot accept client: "s + std::strerror( errno ) );
        }
    }

    static duk_size_t dukReadCb( void *udata, char *buffer, duk_size_t length ) {
        auto& self = Self::fromUdata( udata );
        auto bytesRead = read( self._clientSocket, buffer, length );
        if ( bytesRead < 0 ) {
            throw std::runtime_error( "Cannot read from client: "s + std::strerror( errno ) );
        }
        assert( bytesRead > 0 );
        return bytesRead;
    }

    static duk_size_t dukWriteCb( void *udata, const char *buffer, duk_size_t length ) {
        auto& self = Self::fromUdata( udata );
        auto bytesWritten = write( self._clientSocket, buffer, length );
        if ( bytesWritten < 0 ) {
            throw std::runtime_error( "Cannot write to client: "s + std::strerror( errno ) );
        }
        assert( bytesWritten > 0 );
        return bytesWritten;
    }

    static duk_size_t dukPeekCb( void *udata ) {
        auto& self = Self::fromUdata( udata );
        pollfd fds[ 1 ];

        fds[ 0 ].fd = self._clientSocket;
	    fds[ 0 ].events = POLLIN;
	    fds[ 0 ].revents = 0;

        int pollRc = poll( fds, 1, 0 );
        if ( pollRc < 0 ) {
            throw std::runtime_error( "Cannot poll client: "s + std::strerror( errno ) );
        }
        return pollRc;
    }

    static void dukDeatachCb( duk_hthread*, void *udata ) {
        auto& self = Self::fromUdata( udata );
        close( self._clientSocket );
        self._clientSocket = -1;
    }

    static void dukReadFlushCb( void * /*udata*/ ) {}
    static void dukWriteFlushCb( void * /*udata*/ ) {}

    bool _debuggingEnabled;
    int _serverSocket = -1;
    int _clientSocket = -1;
};

} // namespace jac