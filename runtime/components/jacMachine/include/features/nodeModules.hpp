#pragma once

#include <jsmachine.hpp>
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
        std::string source = MODULE_HEADER;
        fs::readFile( resolvePath( path ), source );
        source += MODULE_FOOTER;

        dukPushModuleObject( self()._context, path.c_str(), true  );
        auto ret = duk_safe_call(self()._context, dukEvalNodeModuleFromString, &source, 1, 1);
        if ( ret != 0 ) {
            std::string error = duk_safe_to_stacktrace( self()._context, -1 );
            duk_pop( self()._context );
            std::cout << "Main eval failed: " << error << "\n";
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
    static constexpr const char *MODULE_HEADER = "(function(exports,require,module,__filename,__dirname){";
    static constexpr const char *MODULE_FOOTER = "\n})";

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
        if( path.size() >= 3 && path.compare( path.size()-3, 3, ".js" ) != 0 ) {
            path += ".js";
        }
        return path;
    }

    // Functions below are copied from duktape_v2.6.0-src/extras/module-node/duk_module_node.c
    // They are static there, so we can't just use them.
    static void dukPushModuleObject( duk_context *ctx, const char *moduleId, bool main ) {
        duk_push_object(ctx);

        /* Set this as the main module, if requested */
        if (main) {
            duk_push_global_stash(ctx);
            duk_dup(ctx, -2);
            duk_put_prop_string(ctx, -2, "\xff" "mainModule");
            duk_pop(ctx);
        }

        /* Node.js uses the canonicalized filename of a module for both module.id
        * and module.filename.  We have no concept of a file system here, so just
        * use the module ID for both values.
        */
        duk_push_string(ctx, moduleId);
        duk_dup(ctx, -1);
        duk_put_prop_string(ctx, -3, "filename");
        duk_put_prop_string(ctx, -2, "id");

        /* module.exports = {} */
        duk_push_object(ctx);
        duk_put_prop_string(ctx, -2, "exports");

        /* module.loaded = false */
        duk_push_false(ctx);
        duk_put_prop_string(ctx, -2, "loaded");

        /* module.require */
        dukPushRequireFunction(ctx, moduleId);
        duk_put_prop_string(ctx, -2, "require");
    }

    static void dukPushRequireFunction(duk_context *ctx, const char *id) {
        duk_push_c_function(ctx, dukHandleRequire, 1);
        duk_push_string(ctx, "name");
        duk_push_string(ctx, "require");
        duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
        duk_push_string(ctx, id);
        duk_put_prop_string(ctx, -2, "\xff" "moduleId");

        /* require.cache */
        duk_push_global_stash(ctx);
        (void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
        duk_put_prop_string(ctx, -3, "cache");
        duk_pop(ctx);

        /* require.main */
        duk_push_global_stash(ctx);
        (void) duk_get_prop_string(ctx, -1, "\xff" "mainModule");
        duk_put_prop_string(ctx, -3, "main");
        duk_pop(ctx);
    }

    static duk_ret_t dukHandleRequire(duk_context *ctx) {
        /*
        *  Value stack handling here is a bit sloppy but should be correct.
        *  Call handling will clean up any extra garbage for us.
        */

        const char *id;
        const char *parent_id;
        duk_idx_t module_idx;
        duk_idx_t stash_idx;
        duk_int_t ret;

        duk_push_global_stash(ctx);
        stash_idx = duk_normalize_index(ctx, -1);

        duk_push_current_function(ctx);
        (void) duk_get_prop_string(ctx, -1, "\xff" "moduleId");
        parent_id = duk_require_string(ctx, -1);
        (void) parent_id;  /* not used directly; suppress warning */

        /* [ id stash require parent_id ] */

        id = duk_require_string(ctx, 0);

        (void) duk_get_prop_string(ctx, stash_idx, "\xff" "modResolve");
        duk_dup(ctx, 0);   /* module ID */
        duk_dup(ctx, -3);  /* parent ID */
        duk_call(ctx, 2);

        /* [ ... stash ... resolved_id ] */

        id = duk_require_string(ctx, -1);

        if (dukGetCachedModule(ctx, id)) {
            goto have_module;  /* use the cached module */
        }

        dukPushModuleObject(ctx, id, false);
        dukPutCachedModule(ctx);  /* module remains on stack */

        /*
        *  From here on out, we have to be careful not to throw.  If it can't be
        *  avoided, the error must be caught and the module removed from the
        *  require cache before rethrowing.  This allows the application to
        *  reattempt loading the module.
        */

        module_idx = duk_normalize_index(ctx, -1);

        /* [ ... stash ... resolved_id module ] */

        (void) duk_get_prop_string(ctx, stash_idx, "\xff" "modLoad");
        duk_dup(ctx, -3);  /* resolved ID */
        (void) duk_get_prop_string(ctx, module_idx, "exports");
        duk_dup(ctx, module_idx);
        ret = duk_pcall(ctx, 3);
        if (ret != DUK_EXEC_SUCCESS) {
            dukDelCachedModule(ctx, id);
            (void) duk_throw(ctx);  /* rethrow */
        }

        if (duk_is_string(ctx, -1)) {
            duk_int_t ret;

            /* [ ... module source ] */
            ret = duk_safe_call(ctx, dukEvalModuleSource, NULL, 2, 1);
            if (ret != DUK_EXEC_SUCCESS) {
                dukDelCachedModule(ctx, id);
                (void) duk_throw(ctx);  /* rethrow */
            }
        } else if (duk_is_undefined(ctx, -1)) {
            duk_pop(ctx);
        } else {
            dukDelCachedModule(ctx, id);
            (void) duk_type_error(ctx, "invalid module load callback return value");
        }

        /* fall through */

    have_module:
        /* [ ... module ] */

        (void) duk_get_prop_string(ctx, -1, "exports");
        return 1;
    }

    /* Place a `module` object on the top of the value stack into the require cache
     * based on its `.id` property.  As a convenience to the caller, leave the
     * object on top of the value stack afterwards.
     */
    static void dukPutCachedModule(duk_context *ctx) {
        /* [ ... module ] */

        duk_push_global_stash(ctx);
        (void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
        duk_dup(ctx, -3);

        /* [ ... module stash req_cache module ] */

        (void) duk_get_prop_string(ctx, -1, "id");
        duk_dup(ctx, -2);
        duk_put_prop(ctx, -4);

        duk_pop_3(ctx);  /* [ ... module ] */
    }

    static void dukDelCachedModule(duk_context *ctx, const char *id) {
        duk_push_global_stash(ctx);
        (void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
        duk_del_prop_string(ctx, -1, id);
        duk_pop_2(ctx);
    }

    static duk_bool_t dukGetCachedModule(duk_context *ctx, const char *id) {
        duk_push_global_stash(ctx);
        (void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
        if (duk_get_prop_string(ctx, -1, id)) {
            duk_remove(ctx, -2);
            duk_remove(ctx, -2);
            return 1;
        } else {
            duk_pop_3(ctx);
            return 0;
        }
    }

    static duk_int_t dukEvalModuleSource(duk_context *ctx, void *udata) {
        const char *src;

        /*
        *  Stack: [ ... module source ]
        */

        (void) udata;

        /* Wrap the module code in a function expression.  This is the simplest
        * way to implement CommonJS closure semantics and matches the behavior of
        * e.g. Node.js.
        */
        duk_push_string(ctx, MODULE_HEADER);
        src = duk_require_string(ctx, -2);
        duk_push_string(ctx, (src[0] == '#' && src[1] == '!') ? "//" : "");  /* Shebang support. */
        duk_dup(ctx, -3);  /* source */
        duk_push_string(ctx, MODULE_FOOTER);  /* Newline allows module last line to contain a // comment. */
        duk_concat(ctx, 4);

        /* [ ... module source func_src ] */

        (void) duk_get_prop_string(ctx, -3, "filename");
        duk_compile(ctx, DUK_COMPILE_EVAL);
        duk_call(ctx, 0);

        /* [ ... module source func ] */

        /* Set name for the wrapper function. */
        duk_push_string(ctx, "name");
        duk_push_string(ctx, "main");
        duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

        /* call the function wrapper */
        (void) duk_get_prop_string(ctx, -3, "exports");   /* exports */
        (void) duk_get_prop_string(ctx, -4, "require");   /* require */
        duk_dup(ctx, -5);                                 /* module */
        (void) duk_get_prop_string(ctx, -6, "filename");  /* __filename */
        duk_push_undefined(ctx);                          /* __dirname */
        duk_call(ctx, 5);

        /* [ ... module source result(ignore) ] */

        /* module.loaded = true */
        duk_push_true(ctx);
        duk_put_prop_string(ctx, -4, "loaded");

        /* [ ... module source retval ] */

        duk_pop_2(ctx);

        /* [ ... module ] */

        return 1;
    }

    // This is dukEvalModuleSource, but without the header/footer and with duk_compile_lstring_filename
    static duk_int_t dukEvalNodeModuleFromString(duk_context *ctx, void *sourceStrVoid ) {
        std::string *source = (std::string*)sourceStrVoid;

        /* [ ... module ] */

        (void) duk_get_prop_string(ctx, -1, "filename");
        duk_compile_lstring_filename(ctx, DUK_COMPILE_EVAL, source->c_str(), source->size());
        duk_call(ctx, 0);

        /* [ ... module func ] */

        /* Set name for the wrapper function. */
        duk_push_string(ctx, "name");
        duk_push_string(ctx, "main");
        duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

        /* [ ... module func ] */

        /* call the function wrapper */
        (void) duk_get_prop_string(ctx, -2, "exports");   /* exports */
        (void) duk_get_prop_string(ctx, -3, "require");   /* require */
        duk_dup(ctx, -4);                                 /* module */
        (void) duk_get_prop_string(ctx, -5, "filename");  /* __filename */
        duk_push_undefined(ctx);                          /* __dirname */
        duk_call(ctx, 5);

        /* [ ... module result(ignore) ] */

        /* module.loaded = true */
        duk_push_true(ctx);
        duk_put_prop_string(ctx, -3, "loaded");

        /* [ ... module retval ] */
        duk_pop(ctx);

        /* [ ... module ] */
        return 1;
    }

    std::map< std::string, NativeModuleInit > _availableNativeModules;
};

} // namespace jac
