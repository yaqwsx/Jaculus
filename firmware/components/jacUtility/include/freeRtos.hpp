#pragma once

#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <freertos/task.h>

namespace jac::freertos {

class IsrDeferrer {
public:
    using Arg = void*;
    using Handler = void (*)( Arg );

    IsrDeferrer( int size )
        : _q( xQueueCreate( size, sizeof( Handler ) + sizeof( Arg ) ) )
    {
        if ( !_q )
            throw std::runtime_error( "Cannot allocate queue" );
        auto res = xTaskCreate( _run, "IsrDeferrer", 2048, this, 15, nullptr );
        if ( res != pdPASS )
            throw std::runtime_error( "Cannot allocate task" );
    }
    IsrDeferrer( const IsrDeferrer& ) = delete;
    IsrDeferrer& operator=( const IsrDeferrer& ) = delete;
    IsrDeferrer( IsrDeferrer&& o ) : _q( o._q ) { o._q = nullptr; }
    IsrDeferrer& operator=( IsrDeferrer&& o ) { swap( o ); return *this; }

    ~IsrDeferrer() {
        if ( _q )
            vQueueDelete( _q );
    }

    void swap( IsrDeferrer& o ) {
        using std::swap;
        swap( _q, o._q );
    }

    void IRAM_ATTR isr( Handler h, Arg a ) {
        uint8_t data[ sizeof( Handler ) + sizeof( Arg ) ];
        *reinterpret_cast< Arg* >( data ) = a;
        *reinterpret_cast< Handler* >( data + sizeof( Arg ) ) = h;
        portBASE_TYPE higherPriorityTaskWoken = pdFALSE;
        xQueueSendToBackFromISR( _q, data, &higherPriorityTaskWoken );
        if( higherPriorityTaskWoken )
            portYIELD_FROM_ISR();
    }
private:
    static void _run(void* arg ) {
        auto* self = reinterpret_cast< IsrDeferrer* >( arg );
        while ( true ) {
            uint8_t data[ sizeof( Handler ) + sizeof( Arg ) ];
            xQueueReceive( self->_q, data, portMAX_DELAY );
            Arg& a = *reinterpret_cast< Arg* >( data );
            Handler& h = *reinterpret_cast< Handler* >( data + sizeof( Arg ) );
            (*h)( a );
        }
    }
    QueueHandle_t _q;
};

} // namespace jac::freertos