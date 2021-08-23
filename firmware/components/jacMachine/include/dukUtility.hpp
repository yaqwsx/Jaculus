#pragma once

#include <duktape.h>
#include <string>
#include <string_view>

using DukCFunction = duk_ret_t (*)( duk_context * );

inline void dukDumpStack( duk_context* ctx, const std::string& message = {} ) {
    duk_push_context_dump( ctx );
    std::cout << message << ": " << duk_safe_to_string( ctx, -1 ) << "\n";
    duk_pop( ctx );
}

inline void dukDumpLast( duk_context* ctx, const std::string& message = {} ) {
    duk_dup( ctx, -1 );
    std::cout << message << ": " << duk_safe_to_stacktrace( ctx, -1 ) << "\n";
    duk_pop( ctx );
}

inline int dukReturn( duk_context* ctx, int value ) {
    duk_push_int( ctx, value );
    return 1;
}

inline int dukReturn( duk_context* ctx, double value ) {
    duk_push_number( ctx, value );
    return 1;
}

inline int dukReturn( duk_context* ctx, const std::string& s ) {
    duk_push_string( ctx, s.c_str() );
    return 1;
}

inline int dukReturn( duk_context* ctx, const std::string_view& s ) {
    duk_push_lstring( ctx, s.data(), s.length() );
    return 1;
}

inline int dukReturn( duk_context* ) {
    return 0;
}

// Bind an object as this of a Duktape/C function. The object should be on top
// of the stack. The bound function is returned via stack.
inline void dukBindLightFunction( duk_context* ctx, duk_ret_t ( *fun )( duk_context * ), int nargs ) {
    duk_push_c_lightfunc( ctx, fun, nargs, nargs, 0 );
    duk_push_string( ctx, "bind" );
    duk_dup( ctx, 0 ); // Duplicate the object
    duk_call_prop( ctx, -3, 1 ); // Invoke bind on fun
    duk_remove( ctx, -2 ); // Delete the unboud function
    duk_remove( ctx, -2 ); // Delete the original object
}

// Bind Duktape/C function to this as a lightfunction, leave the bound function
// on stack
inline void dukBindAsLightfuncToThis( duk_context* ctx, duk_ret_t ( *fun )( duk_context * ), int nargs ) {
    duk_push_c_lightfunc( ctx, fun, nargs, nargs, 0 );
    duk_push_string( ctx, "bind" );
    duk_push_this( ctx );
    duk_call_prop( ctx, -3, 1 ); // Invoke bind on fun
    duk_remove(ctx, -2); // Delete the unbouded function
}


// Given an array on top of the stack, perform action for each element. The
// function is invoked with index as argument and the element is on top of the
// stack.
template < typename F >
void dukForEach( duk_context* ctx, F f ) {
    int count = duk_get_length( ctx, -1 );
    auto stackSize = duk_get_top( ctx );
    for( int i = 0; i < count; i++ ) {
        duk_get_prop_index( ctx, -1, i );
        f( i );
        while ( duk_get_top( ctx ) != stackSize )
            duk_pop( ctx );
    };
}

// Push top-most value into an array on given offset
inline void dukAppendArray( duk_context* ctx, int arrayOffset ) {
    int count = duk_get_length( ctx, arrayOffset );
    duk_put_prop_index( ctx, arrayOffset, count );
}


// Pushes class with a given name and methods to the stack
template < int MethodCount, int StaticMethodCount >
inline void dukPushClass( duk_context* ctx, const std::string& name,
    DukCFunction constructor, int constructorArgCount,
    const duk_function_list_entry ( &methods )[ MethodCount ],
    const duk_function_list_entry ( &staticMethods )[ StaticMethodCount ] )
{
    duk_push_c_function( ctx, constructor, constructorArgCount );
    auto constructorOffset = duk_get_top_index( ctx );

    // Add pretty name to the constructor
    duk_push_string( ctx, "name" );
    duk_push_string( ctx, name.c_str() );
    duk_def_prop( ctx, constructorOffset, DUK_DEFPROP_HAVE_VALUE );

    duk_put_function_list( ctx, constructorOffset, staticMethods );

    duk_push_bare_object( ctx );
    duk_put_function_list( ctx, -1, methods );
    duk_put_prop_string( ctx, constructorOffset, "prototype" );
}

inline void dukRaiseError( duk_context* ctx, const std::string& error ) {
    duk_error( ctx, DUK_ERR_TYPE_ERROR, error.c_str() );
    __builtin_unreachable(); // duk_error never returns
}
