#pragma once

#include <jsmachine.hpp>
#include <duk_module_node.h>

#include <cstring>
#include <map>
#include <filesystem.hpp>

namespace jac {

// Implement node modules loader (i.e., resolve syntax).
//
// Note that currently, this implementation is no compliant with
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
            std::cout << "Main eval failed: " << error << "\n";
            throw std::runtime_error( error );
        }
        // Clean up the return value
        duk_pop( self()._context );
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
            else
                // TBA: Resolve built-in modules
                duk_error( ctx, DUK_ERR_TYPE_ERROR, "Cannot resolve module %s from %s",
                    requestedId.str().c_str(), parentId.str().c_str() );

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
        const char *requestedId = duk_get_string( ctx, 0 );
        try {
            // TBA Implement built-in modules
            std::string source = fs::readFile( self.resolvePath( requestedId ) );
            return dukReturn( ctx, source );
        }
        catch ( std::runtime_error& e ) {
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

    std::map< std::string, std::string > _availableNativeModules;
};

} // namespace jac