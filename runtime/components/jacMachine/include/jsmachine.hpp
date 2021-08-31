#pragma once

#include <duktape.h>
#include <stdexcept>
#include <mutex>
#include <vector>

#include <freertos/semphr.h>

#include <dukUtility.hpp>
#include <freeRtos.hpp>

// Define this macro to avoid tedious writing of a repetitive code
// Note that macro is much easire solution than any other "proper C++" solution
#define MACHINE_FEATURE_SELF() \
    Self& self() { return *static_cast< Self* >( this ); } \
    const Self& self() const { return *static_cast< const Self* >( this ); }

namespace jac {

template< template < typename > typename... Features >
class JsMachineBase: public Features< JsMachineBase < Features... > >... {
public:
    using Self = JsMachineBase< Features... >;

    struct Configuration:
        public Features< Self >::Configuration...
    {
        int eventLoopLimit = 128;
        int interruptQueueSize = 32;
    };

    JsMachineBase( Configuration cfg = Configuration() )
        : _cfg( cfg ), _isrService( cfg.interruptQueueSize )
    {
        _context = duk_create_heap(
            Self::allocateMemory,
            Self::reallocateMemory,
            Self::freeMemory,
            this,
            Self::fatalErrorHandler
        );
        if ( !_context )
            throw std::runtime_error( "Cannot initialize Duktape context" );
        // Create another thread for jobs and keep a reference
        duk_push_heap_stash( _context );
        _nextJobs = duk_get_context( _context, duk_push_thread( _context ) );
        duk_put_prop_string( _context, -2, "_nextJobsContext" );
        duk_pop( _context );

        _eventsPending = xSemaphoreCreateCounting( _cfg.eventLoopLimit, 0 );

        ( Features< Self >::initialize(), ... );
    }

    JsMachineBase( const JsMachineBase& ) = delete;

    ~JsMachineBase() {
        duk_destroy_heap( _context );
    }

    template < typename Extension >
    void extend( const Extension& e ) {
        e( this, _context );
    }

    static Self& fromContext( duk_context* ctx ) {
        duk_memory_functions funs;
        duk_get_memory_functions( ctx, &funs );
        return *reinterpret_cast< Self * >( funs.udata );
    }

    static Self& fromUdata( void* udata ) {
        return *reinterpret_cast< Self * >( udata );
    }

    void evalString( const std::string& s ) {
        duk_eval_string( _context, s.c_str() );
        // For type simplicity we ignore the return value
        duk_pop( _context );
    }

    void finishEvent() {
        bool ret = xSemaphoreTake( _eventsPending, 0 );
        assert( ret && "There were no pending events." );
    }

    void addEvent() {
        xSemaphoreGive( _eventsPending );
    }

    void IRAM_ATTR handleInterrupt( freertos::IsrDeferrer::Handler h,
                                    freertos::IsrDeferrer::Arg a )
    {
        _isrService.isr( h, a );
    }

    // Schedule a new job. The function f will be invoked with a _nextJobs
    // context and it should push function and arguments to it. Just like if
    // duk_call should be called.
    template < typename Fn >
    void schedule( Fn f ) {
        std::scoped_lock _( _globalLock );
        auto stackSize = duk_get_top( _nextJobs );
        f( _nextJobs );
        auto pushedCount = duk_get_top( _nextJobs ) - stackSize;

        // Push number of arguments
        duk_push_int( _nextJobs, pushedCount - 1 );

        // Mark the event
        _jobsPending++;
        addEvent();
    }

    void runEventLoop() {
        while ( !_shouldExit ) {
            // Wait for some events
            xSemaphoreTake( _eventsPending, portMAX_DELAY );
            xSemaphoreGive( _eventsPending );

            // Process the events
            (Features< Self >::onEventLoop(), ...);

            // Run scheduled jobs
            std::scoped_lock _( _globalLock );
            while ( _jobsPending != 0 ) {
                auto argCount = duk_require_int( _nextJobs, -1 );
                duk_pop( _nextJobs );

                duk_require_stack( _context, argCount + 1 );
                duk_xmove_top( _context, _nextJobs, argCount + 1 );
                if ( duk_pcall( _context, argCount ) != 0 ) {
                    this->reportError( duk_safe_to_stacktrace( _context, -1) );
                }
                duk_pop( _context );

                _jobsPending--;
                finishEvent();
            }
        }
    }

    duk_context *_context = nullptr;
    duk_context *_nextJobs = nullptr;
    Configuration _cfg;
protected:
    bool _shouldExit = false;
    SemaphoreHandle_t _eventsPending;
    int _jobsPending = 0;
    std::recursive_mutex _globalLock; // The mutex has to be recursive to properly implement scheduleJob

    freertos::IsrDeferrer _isrService;
};

} // namespace jac