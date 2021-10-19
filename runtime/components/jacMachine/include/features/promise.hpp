#pragma once

#include <jsmachine.hpp>

extern "C" {
    extern const uint8_t regeneratorRuntimeStart[]
        asm("_binary_regeneratorRuntime_js_start");
    extern const uint8_t regeneratorRuntimeEnd[]
        asm("_binary_regeneratorRuntime_js_end");
}


namespace jac {

// Implement Promise based on
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise
//
// Note that we intentionally implement the Promise in native language to save
// some RAM although the whole implementation could be easily implemented in
// in Javascript (probably more easily).
template < typename Self >
class Promise {
    static inline constexpr const char* SLOT = "promiseSlot";
public:
    MACHINE_FEATURE_SELF();

    struct Configuration {};

    void initialize() {
        _setupSlot();
        _registerPromise();
        _registerRuntime();
    }

    void onEventLoop() {}

private:
    void _setupSlot() {
        duk_push_heap_stash( self()._context );
        duk_push_object( self()._context );
        duk_put_prop_string( self()._context, -2, SLOT );
        duk_pop( self()._context );
    }

    void _registerPromise() {
        duk_context* ctx = self()._context;

        auto stackSize = duk_get_top( ctx );

        duk_push_global_object( ctx );
        auto globalObjectOffset = duk_get_top_index( ctx );

        duk_push_c_function( ctx, dukPromiseConstructor, 1 );
        auto constructorOffset = duk_get_top_index( ctx );

        // Add pretty name to the constructor
        duk_push_string( ctx, "name" );
        duk_push_string( ctx, "Promise" );
        duk_def_prop( ctx, constructorOffset, DUK_DEFPROP_HAVE_VALUE );

        // Add constructor methods
        duk_function_list_entry constructorMethods[] = {
            { "resolve", dukPromiseResolve, 1 },
            { "reject", dukPromiseReject, 1 },
            { "all", dukPromiseAll, 1 },
            { "race", dukPromiseRace, 1 },
            { nullptr, nullptr, 0 }
        };
        duk_put_function_list( ctx, constructorOffset, constructorMethods );

        // Build prototype
        duk_function_list_entry objectMethods[] = {
            { "catch", dukPromiseCatch, 1 },
            { "finally", dukPromiseFinally, 1 },
            { "then", dukPromiseThen, DUK_VARARGS },
            { nullptr, nullptr, 0 }
        };
        duk_push_bare_object( ctx );
        duk_put_function_list( ctx, -1, objectMethods );
        duk_put_prop_string( ctx, constructorOffset, "prototype" );

        // Register the promise to the global namespace
        duk_put_prop_string( ctx, globalObjectOffset, "Promise" );

        while ( duk_get_top( ctx ) != stackSize )
            duk_pop( ctx );
    }

    void _registerRuntime() {
        duk_context* ctx = self()._context;
        duk_push_lstring( ctx, reinterpret_cast< const char * >( regeneratorRuntimeStart ),
            regeneratorRuntimeEnd - regeneratorRuntimeStart );
        duk_push_string( ctx, "/builtin/regeneratorRuntime.js" );
        duk_compile( ctx, DUK_COMPILE_EVAL );
	    duk_call( ctx, 0 );
        duk_pop( ctx );
    }

    static duk_ret_t dukPromiseConstructor( duk_context *ctx ) {
        // Constructor takes a single argument - a function (executor) taking
        // two callbacks - resolve and reject. Therefore the executor has offset
        // 0

        if ( !duk_is_constructor_call( ctx ) ) {
            return DUK_RET_TYPE_ERROR;
        }

        duk_push_this( ctx );
        auto thisOffset = duk_get_top_index( ctx );

        // We use int to represent promise status: pending (0), resolved (1),
        // rejected (-1). We also need an arrays to store consumers of the
        // promise.  We are verbose about _promiseStatus name as we use it to
        // recognize if object is a promise or not
        duk_push_int( ctx, 0 );
        duk_put_prop_string( ctx, thisOffset, "_promiseStatus" );
        duk_push_bare_array( ctx );
        duk_put_prop_string( ctx, thisOffset, "_onResolved" );
        duk_push_bare_array( ctx );
        duk_put_prop_string( ctx, thisOffset, "_onRejected" );

        duk_pop( ctx ); // Pop this

        // Now we have to create binded reject and resolve handlers
        dukBindAsLightfuncToThis( ctx, dukResolveInternal, 1 );
        dukBindAsLightfuncToThis( ctx, dukRejectInternal, 1 );

        // Invoke executor (-3) on dukResolveInternal (-2) and dukRejectInternal (-1)
        int status = duk_pcall( ctx, 2 );
        if ( status != DUK_EXEC_SUCCESS ) {
            dukDumpLast( ctx, "Cannot evaluate executor" );
            // If executor fails, the error is on stack, just reject the promise
            dukBindAsLightfuncToThis( ctx, dukRejectInternal, 1 );
            duk_dup( ctx, -2 );
            duk_call( ctx, 1 );
        }

        return 0;
    }

    static duk_ret_t dukPromiseResolve( duk_context *ctx ) {
        bool isArgAPromise = duk_is_object( ctx, 0 ) && duk_has_prop_string( ctx, 0, "_promiseStatus" );
        if ( isArgAPromise )
            return 1;

        duk_push_global_object( ctx );
        duk_get_prop_string( ctx, -1, "Promise" );
        duk_get_prop_string( ctx, -1, "prototype" );

        duk_push_bare_object( ctx );
        auto promiseOffset = duk_get_top_index( ctx );
        duk_dup( ctx, -2 );
        duk_set_prototype( ctx, -2 );

        duk_push_int( ctx, 1 );
        duk_put_prop_string( ctx, promiseOffset, "_promiseStatus" );
        duk_push_bare_array( ctx );
        duk_put_prop_string( ctx, promiseOffset, "_onResolved" );
        duk_push_bare_array( ctx );
        duk_put_prop_string( ctx, promiseOffset, "_onRejected" );
        duk_dup( ctx, 0 );
        duk_put_prop_string( ctx, promiseOffset, "_value" );

        return 1;
    }

    static duk_ret_t dukPromiseReject( duk_context *ctx ) {
        duk_push_global_object( ctx );
        duk_get_prop_string( ctx, -1, "Promise" );
        duk_get_prop_string( ctx, -1, "prototype" );

        duk_push_bare_object( ctx );
        auto promiseOffset = duk_get_top_index( ctx );
        duk_dup( ctx, -2 );
        duk_set_prototype( ctx, -2 );

        duk_push_int( ctx, -1 );
        duk_put_prop_string( ctx, promiseOffset, "_promiseStatus" );
        duk_push_bare_array( ctx );
        duk_put_prop_string( ctx, promiseOffset, "_onResolved" );
        duk_push_bare_array( ctx );
        duk_put_prop_string( ctx, promiseOffset, "_onRejected" );
        duk_dup( ctx, 0 );
        duk_put_prop_string( ctx, promiseOffset, "_value" );

        return 1;
    }

    static duk_ret_t dukPromiseAll( duk_context *ctx ) {
        // TBA: Implement
        return DUK_RET_TYPE_ERROR;
    }

    static duk_ret_t dukPromiseRace( duk_context *ctx ) {
        // TBA: Implement
        return DUK_RET_TYPE_ERROR;
    }

    // Take a value, transform it via transform. If the transformed value is a
    // value, directly pass it settle, otherwise, if it is a promise, resolve it
    // or reject it. Takes a single argument: the resolved value. The function
    // also expects that the following properties are set:
    // - transform: the function that tranforms the value
    // - resolve: a callback called with a resolved value from a promise
    // - reject: a callback called with a rejected value from a promise
    // - settle: a callback called with a settled value (one of resolve or
    //   reject)
    static duk_ret_t dukSettleValueOrPromise( duk_context *ctx ) {
        duk_push_current_function( ctx );
        auto funcOffset = duk_get_top_index( ctx );

        // Transform value
        duk_get_prop_string( ctx, funcOffset, "transform" );
        duk_dup( ctx, 0 );
        if ( duk_pcall( ctx, 1 ) != DUK_EXEC_SUCCESS ) {
            // The transform failed, reject the error (on top of the stack)
            duk_get_prop_string( ctx, funcOffset, "reject" );
            duk_dup( ctx, -2 );
            duk_call( ctx, 1 );
            return 0;
        }

        const auto transformResOffset = duk_get_top_index(ctx);
        bool isPromise = duk_is_object( ctx, transformResOffset ) && duk_has_prop_string( ctx, transformResOffset, "_promiseStatus" );
        if ( isPromise ) {
            duk_get_prop_string( ctx, transformResOffset, "_promiseStatus" );
            int status = duk_require_int( ctx, -1 );
            if ( status == 0 ) {
                // Unresolved
                duk_get_prop_string( ctx, transformResOffset, "_onResolved" );
                duk_get_prop_string( ctx, funcOffset, "resolve" );
                dukAppendArray( ctx, -2 );
                duk_get_prop_string( ctx, transformResOffset, "_onRejected" );
                duk_get_prop_string( ctx, funcOffset, "reject" );
                dukAppendArray( ctx, -2 );
            }
            else if ( status == 1 ) {
                // Resolved, invoke callback directly
                duk_get_prop_string( ctx, funcOffset, "resolve" );
                duk_get_prop_string( ctx, transformResOffset, "_value" );
                duk_call( ctx, 1 );
            }
            else if ( status == -1 ) {
                // Rejected, invoke callback directly
                duk_get_prop_string( ctx, funcOffset, "reject" );
                duk_get_prop_string( ctx, transformResOffset, "_value" );
                duk_call( ctx, 1 );
            }
        }
        else {
            duk_get_prop_string( ctx, funcOffset, "settle" );
            duk_dup( ctx, -2 );
            duk_call( ctx, 1 );
        }
        return 0;
    }

    static duk_ret_t dukDoNothing( duk_context *ctx ) {
        // Return 1st passed argument
        return 1;
    }

    static duk_ret_t dukRethrow(duk_context *ctx) {
        int argCount = duk_get_top( ctx );
        if (argCount != 1)
            return DUK_RET_ERROR;
        return duk_throw(ctx);
    }

    // Takes two arguments: resolve handler and reject handler
    // We follow roughly the following guide: https://medium.com/swlh/implement-a-simple-promise-in-javascript-20c9705f197a
    static duk_ret_t dukPromiseThen( duk_context *ctx ) {
        int argCount = duk_get_top( ctx );
        if ( argCount == 0 || argCount > 2 )
            return DUK_RET_ERROR;

        const int resolveArgOffset = 0;
        const int rejectArgOffset = 1;
        if ( argCount == 1 ) {
            // Push dummy instead of the second argument
            duk_push_c_lightfunc( ctx, dukRethrow, 1, 1, 0 );
        }

        duk_push_global_object( ctx );
        duk_get_prop_string( ctx, -1, "Promise" );

        // Prepare new promise executor
        duk_push_c_function( ctx, dukPromiseThenImpl, 2 );
        duk_push_this( ctx );
        duk_put_prop_string( ctx, -2, "sourcePromise" );
        duk_dup( ctx, resolveArgOffset );
        duk_put_prop_string( ctx, -2, "onResolved" );
        duk_dup( ctx, rejectArgOffset );
        duk_put_prop_string( ctx, -2, "onRejected" );

        // Invoke Promise contructor
        duk_new( ctx, 1 );
        return 1;
    }

    // Takes one argument: reject handler
    static duk_ret_t dukPromiseCatch( duk_context *ctx ) {
        int argCount = duk_get_top( ctx );
        if ( argCount != 1)
            return DUK_RET_ERROR;

        const int rejectArgOffset = 0;
        const int resolveArgOffset = 1;

        // Push dummy instead of the resolve function
        duk_push_c_lightfunc( ctx, dukDoNothing, 1, 1, 0 );

        duk_push_global_object( ctx );
        duk_get_prop_string( ctx, -1, "Promise" );

        // Prepare new promise executor
        duk_push_c_function( ctx, dukPromiseThenImpl, 2 );
        duk_push_this( ctx );
        duk_put_prop_string( ctx, -2, "sourcePromise" );
        duk_dup( ctx, resolveArgOffset );
        duk_put_prop_string( ctx, -2, "onResolved" );
        duk_dup( ctx, rejectArgOffset );
        duk_put_prop_string( ctx, -2, "onRejected" );

        // Invoke Promise contructor
        duk_new( ctx, 1 );

        duk_dup( ctx, rejectArgOffset );
        duk_put_prop_string( ctx, -2, "_iam_catch" );

        return 1;
    }

    // Takes one argument (resolve value) and requires function to have .settle parameter, which
    // is the function that should be called in JS's .finally(...).
    // Any result from .settle is discarded, and resolved value is returned instead.
    static duk_ret_t dukFinallyResolveWrapper(duk_context *ctx) {
        duk_push_current_function( ctx );
        duk_get_prop_string( ctx, -1, "settle" );
        duk_call(ctx, 0);
        duk_pop_2(ctx); // drop return value and current_function
        return 1;
    }

    // Takes one argument (thrown error) and requires function to have .settle parameter, which
    // is the function that should be called in JS's .finally(...)
    // Any result from .settle is discarded, and thrown error is re-thrown instead.
    static duk_ret_t dukFinallyRejectWrapper(duk_context *ctx) {
        duk_push_current_function( ctx );
        duk_get_prop_string( ctx, -1, "settle" );
        duk_call(ctx, 0);
        duk_pop_2(ctx); // drop return value and current_function
        return duk_throw(ctx);
    }

    // Takes one argument: settle handler
    static duk_ret_t dukPromiseFinally( duk_context *ctx ) {
        int argCount = duk_get_top( ctx );
        if ( argCount != 1)
            return DUK_RET_ERROR;

        const int settleArgOffset = 0;

        duk_push_global_object( ctx );
        duk_get_prop_string( ctx, -1, "Promise" );

        // Prepare new promise executor
        duk_push_c_function( ctx, dukPromiseThenImpl, 2 );
        duk_push_this( ctx );
        duk_put_prop_string( ctx, -2, "sourcePromise" );

        duk_push_c_function(ctx, dukFinallyResolveWrapper, 1);
        duk_dup(ctx, settleArgOffset);
        duk_put_prop_string(ctx, -2, "settle");
        duk_put_prop_string( ctx, -2, "onResolved" );

        duk_push_c_function(ctx, dukFinallyRejectWrapper, 1);
        duk_dup(ctx, settleArgOffset);
        duk_put_prop_string(ctx, -2, "settle");
        duk_put_prop_string(ctx, -2, "onRejected" );

        // Invoke Promise contructor
        duk_new( ctx, 1 );

        duk_dup( ctx, settleArgOffset );
        duk_put_prop_string( ctx, -2, "_iam_finally" );
        return 1;
    }

    // A function that is passed into a Promise constructor in dukPromiseThen.
    // Therefore it takes two arguments: resolve and reject.
    // The function expects also the following properties:
    // - sourcePromise: the promise being chained
    // - onResolved: a resolve callback following the chain
    // - onRejected: a reject callback following the chain
    static duk_ret_t dukPromiseThenImpl( duk_context *ctx ) {
        duk_push_current_function( ctx );
        auto funcOffset = duk_get_top_index( ctx );

        // Construct the resolve callback
        duk_push_c_function( ctx, dukSettleValueOrPromise, 1 );
        auto resolveCbOffset = duk_get_top_index( ctx );
        duk_get_prop_string( ctx, funcOffset, "onResolved" );
        duk_put_prop_string( ctx, -2, "transform" );
        duk_dup( ctx, 0 );
        duk_put_prop_string( ctx, -2, "resolve" );
        duk_dup( ctx, 1 );
        duk_put_prop_string( ctx, -2, "reject" );
        duk_dup( ctx, 0 );
        duk_put_prop_string( ctx, -2, "settle" );

        // Construct the reject callback
        duk_push_c_function( ctx, dukSettleValueOrPromise, 1 );
        auto rejectCbOffset = duk_get_top_index( ctx );
        duk_get_prop_string( ctx, funcOffset, "onRejected" );
        duk_put_prop_string( ctx, -2, "transform" );
        duk_dup( ctx, 0 );
        duk_put_prop_string( ctx, -2, "resolve" );
        duk_dup( ctx, 1 );
        duk_put_prop_string( ctx, -2, "reject" );
        duk_dup( ctx, 0 );  // .catch or onRejected "erases" the rejected status (or explicitely rethrows), so settle must be resolve again.
        duk_put_prop_string( ctx, -2, "settle" );

        duk_get_prop_string( ctx, funcOffset, "sourcePromise" );
        auto sourcePromiseOffset = duk_get_top_index( ctx );
        duk_get_prop_string( ctx, -1, "_promiseStatus" );
        int status = duk_require_int( ctx, -1 );
        if ( status == 0 ) {
            // Unresolved
            duk_get_prop_string( ctx, sourcePromiseOffset, "_onResolved" );
            duk_dup( ctx, resolveCbOffset );
            dukAppendArray( ctx, -2 );
            duk_get_prop_string( ctx, sourcePromiseOffset, "_onRejected" );
            duk_dup( ctx, rejectCbOffset );
            dukAppendArray( ctx, -2 );
        }
        else if ( status == 1 ) {
            // Resolved, invoke callback directly
            duk_dup( ctx, resolveCbOffset );
            duk_get_prop_string( ctx, sourcePromiseOffset, "_value" );
            duk_call( ctx, 1 );
        }
        else if ( status == -1 ) {
            // Rejected, invoke callback directly
            duk_dup( ctx, rejectCbOffset );
            duk_get_prop_string( ctx, sourcePromiseOffset, "_value" );
            duk_call( ctx, 1 );
        }

        return 0;
    }

    // Takes a single argument: value
    static duk_ret_t dukResolveInternal( duk_context *ctx ) {
        auto& self = Self::fromContext( ctx );
        duk_push_this( ctx );
        auto thisOffset = duk_get_top_index( ctx );

        duk_get_prop_string( ctx, -1, "_promiseStatus" );
        if ( duk_require_int( ctx, -1 ) != 0 )
            duk_error( ctx, DUK_ERR_TYPE_ERROR,
                       "Promise was rejected/resolved multiple times" );
        // Set status to resolved
        duk_push_int( ctx, 1 );
        duk_put_prop_string( ctx, thisOffset, "_promiseStatus" );
        // Set value to the error
        duk_dup( ctx, 0 ); // resolve value (the argument of dukResolveInternal)
        duk_put_prop_string( ctx, -3, "_value" );

        // Invoke onRejected
        duk_get_prop_string( ctx, thisOffset, "_onResolved" );
        dukForEach( ctx, [&]( int idx ) {
            self.schedule([&]( duk_context *jobContext ) {
                // There is already the function
                duk_dup( ctx, 0 ); // resolve value
                duk_xmove_top( jobContext, ctx, 2 );
            } );
        });

        return 0;
    }

    // Takes a single argument: error
    static duk_ret_t dukRejectInternal( duk_context *ctx ) {
        auto& self = Self::fromContext( ctx );
        duk_push_this( ctx );
        auto thisOffset = duk_get_top_index( ctx );

        duk_get_prop_string( ctx, -1, "_promiseStatus" );
        if ( duk_require_int( ctx, -1 ) != 0 )
            duk_error( ctx, DUK_ERR_TYPE_ERROR,
                       "Promise was rejected/resolved multiple times" );
        // Set status to rejected
        duk_push_int( ctx, -1 );
        duk_put_prop_string( ctx, thisOffset, "_promiseStatus" );
        // Set value to the error
        duk_dup( ctx, 0 ); // Error value (the argument of dukRejectInternal)
        duk_put_prop_string( ctx, -3, "_value" );

        // Invoke onRejected
        duk_get_prop_string( ctx, thisOffset, "_onRejected" );
        if(duk_get_length(ctx, -1) == 0) {
            dukDumpStack( ctx, "Uncaught exception in Promise" );
            dukRaiseError( ctx, "Uncaught exception in Promise");
        }

        dukForEach( ctx, [&]( int idx ) {
            self.schedule([&]( duk_context *jobContext ) {
                // There is already the function
                duk_dup( ctx, 0 ); // Error value
                duk_xmove_top( jobContext, ctx, 2 );
            } );
        });

        return 0;
    }
};

} // namespace jac
