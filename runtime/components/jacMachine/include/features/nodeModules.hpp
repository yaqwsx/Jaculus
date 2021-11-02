#pragma once

#include <jsmachine.hpp>
#include <jacLog.hpp>
#include <duk_module_node.h>

#include <cstring>
#include <map>
#include <functional>
#include <filesystem.hpp>

namespace jac {

// Implement node modules loader (i.e., resolve syntax).
//
// Note that currently, this implementation is not compliant with
// https://nodejs.org/api/modules.html
template < typename Self >
class NodeModuleLoader {
public:
    MACHINE_FEATURE_SELF();

    struct Configuration {
        std::string basePath = "/";
    };

    void initialize() {
        duk_push_object( self()._context );
        duk_push_c_function( self()._context, dukResolveModuleCb, DUK_VARARGS );
        duk_put_prop_string( self()._context, -2, "resolve" );
        duk_push_c_function( self()._context, dukLoadModule, DUK_VARARGS );
        duk_put_prop_string( self()._context, -2, "load" );
        duk_module_node_init( self()._context );
    }

    void onEventLoop() {}

    void evaluateMain( const std::string& path ) {
        std::string source = fs::readFile( resolvePath( path ) );
        duk_push_string( self()._context, source.c_str() );
        auto ret = duk_module_node_peval_main( self()._context, path.c_str() );
        if ( ret != 0 ) {
            std::string error = duk_safe_to_stacktrace( self()._context, -1 );
            duk_pop( self()._context );
            JAC_LOGE( "node", "Main eval failed: %s ", error.c_str() );
            throw std::runtime_error( error );
        }
        // Clean up the return value
        duk_pop( self()._context );
    }

    // Type of native module init function. The init function obtains a context
    // where:
    // - offset 0 contains the requested id (it can be usually ignored)
    // - offset 1 contains the exports object
    // - offset 2 contains the module object
    // The function shall do one of the following:
    // - return a string source of the module via the stack (return value 1)
    // - populate exports object and return undefined via stack (return value 0)
    // - replace exports object and return undefined via stack (return value 0)
    using NativeModuleInit = std::function< duk_ret_t( duk_context * ) >;

    void registerNativeModule( const std::string& name, NativeModuleInit init ) {
        auto it = _availableNativeModules.find( name );
        if ( it != _availableNativeModules.end() )
            throw std::runtime_error( "Module " + name + " is already registered" );
        _availableNativeModules.emplace( name, init );
    }

private:
    static duk_ret_t dukResolveModuleCb( duk_context *ctx ) {
        /*
         *  Entry stack: [ requestedId parentId ]
         */
        fs::Path requestedId = duk_get_string( ctx, 0 );
        fs::Path parentId = duk_get_string( ctx, 1 );  /* calling module */

        try {
            Self& self = Self::fromContext( ctx );

            fs::Path resolvedPath;

            if ( requestedId.absolute() )
                resolvedPath = requestedId;
            else if ( requestedId[ 0 ] == "." || requestedId[ 0 ] == ".." )
                resolvedPath = parentId.dirname() / requestedId;
            else {
                std::string id = duk_get_string( ctx, 0 );
                auto registerIt = self._availableNativeModules.find( id );
                if ( registerIt != self._availableNativeModules.end() )
                    return dukReturn( ctx, id );
                duk_error( ctx, DUK_ERR_TYPE_ERROR, "Cannot resolve module %s from %s",
                    requestedId.str().c_str(), parentId.str().c_str() );
                __builtin_unreachable();
            }

            fs::Path basePath( self._cfg.basePath );
            resolvedPath = basePath / resolvedPath;
            try {
                resolvedPath = resolvedPath.weakNormalForm();
            } catch ( const std::runtime_error& e) {
                duk_error( ctx, DUK_ERR_TYPE_ERROR, "Cannot normalize path %s: %s",
                    resolvedPath.str().c_str(), e.what() );
            }

            resolvedPath.cutFront( basePath.size() );
            std::string id;
            if ( resolvedPath.absolute() )
                id = resolvedPath.str();
            else
                id = "/" + resolvedPath.str();

            return dukReturn( ctx, id );
        } catch ( std::runtime_error& e ) {
            duk_error( ctx, DUK_ERR_TYPE_ERROR, "Cannot resolve module %s from %s: ",
                    requestedId.str().c_str(), parentId.str().c_str(), e.what() );
        }
        __builtin_unreachable();
    }

    static duk_ret_t dukLoadModule( duk_context *ctx ) {
        Self& self = Self::fromContext( ctx );
        /*
         *  Entry stack: [ resolved_id exports module ]
         */
        const std::string requestedId = duk_get_string( ctx, 0 );
        try {
            auto nativeModuleIt = self._availableNativeModules.find( requestedId );
            if ( nativeModuleIt != self._availableNativeModules.end() ) {
                return nativeModuleIt->second( ctx );
            }
            // The module is not a native one, try reading it from FS
            std::string source = fs::readFile( self.resolvePath( requestedId ) );
            return dukReturn( ctx, source );
        }
        catch ( std::exception& e ) {
            duk_error( ctx, DUK_ERR_TYPE_ERROR, "Cannot load module %s: %s",
                    requestedId, e.what() );
        }
        __builtin_unreachable(); // as duk_error never returns
    }

    std::string resolvePath( const std::string& id ) {
        assert( !id.empty() );
        auto path = fs::concatPath( self()._cfg.basePath, id );
        return path;
    }

    std::map< std::string, NativeModuleInit > _availableNativeModules;
};

} // namespace jac