#pragma once

#include <jsmachine.hpp>

namespace jac {

// Implement timers functionality for the JsMachine.
//
// Internally keeps a list of timer structures. The corresponding callback for
// the timer is stored in <stash>.timerSlot[String(id)].
template < typename Self >
class Timers {
    static inline constexpr const char* SLOT = "timerSlot";

    struct Timer {
        int id;
        unsigned long long targetTime;
        int period;
        bool oneShot;
        bool deleted; // Note that we cannot
    };
public:
    MACHINE_FEATURE_SELF();

    struct Configuration {};

    void initialize() {
        setupSlot();
        registerFunctions();
    }

    void onEventLoop() {
        deleteDeadTimers();

        auto now = pdTICKS_TO_MS( xTaskGetTickCount() );
        for ( Timer& t : _timers ) {
            if ( t.targetTime > now )
                continue;
            self().schedule( { SLOT, t.id } );
            t.deleted = t.oneShot;
            t.targetTime = now + t.period;
        }
    }
private:
    void setupSlot() {
        duk_push_heap_stash( self()._context );
        duk_push_object( self()._context );
        duk_put_prop_string( self()._context, -2, SLOT );
        duk_pop( self()._context );
    }

    void registerFunctions() {
        duk_push_c_function( self()._context, dukCreateTimer, 3 );
        duk_put_global_string( self()._context, "createTimer" );
    }

    const Timer& createTimer( int period, bool oneShot ) {
        _timers.push_back({
            allocateId(),
            pdTICKS_TO_MS( xTaskGetTickCount() ) + period,
            period,
            oneShot,
            false });
        return _timers.back();
    }

    void deleteTimer( const Timer& t ) {
        releaseId( t.id );

        duk_push_heap_stash( self()._context );
        duk_get_prop_string( self()._context, -1, SLOT );
        duk_del_prop_index( self()._context, -1, t.id );
        duk_pop_2( self()._context );
    }

    void deleteDeadTimers() {
        auto toDelete = std::remove_if( _timers.begin(), _timers.end(),
            []( auto t ) { return t.deleted; } );
        for ( auto i = toDelete; i != _timers.end(); i++ )
            deleteTimer( *i );
        _timers.erase( toDelete, _timers.end() );
    }

    int allocateId() {
        if ( !_freeIds.empty() ) {
            int id = _freeIds.back();
            _freeIds.pop_back();
            return id;
        }
        return _timers.size() + 1;
    }

    void releaseId( int id ) {
        _freeIds.push_back( id );
    }

    // Accepts the following duk arguments:
    // - period: number - period in milliseconds
    // - oneShot: bool  - declare if the timer is one shot or not
    // - callback: fun  - timer callback
    static duk_ret_t dukCreateTimer( duk_context* ctx ) {
        Self& self = Self::fromContext( ctx );
        // Validate arguments
        int period = duk_require_number( ctx, 0 );
        bool oneShot = duk_require_boolean( ctx, 1 );
        duk_require_function( ctx, 2 );

        const Timer& t = self.createTimer( period, oneShot );

        auto callbackOffset = duk_get_top_index( ctx );
        duk_push_heap_stash( ctx );
        duk_get_prop_string( ctx, -1, SLOT );
        auto slotOffset = duk_get_top_index( ctx );
        duk_dup( ctx, callbackOffset );
        duk_put_prop_index( ctx, slotOffset, t.id );

        return dukReturn( ctx, t.id );
    }

    std::vector< Timer > _timers;
    std::vector< int > _freeIds;
};

} // namespace jac