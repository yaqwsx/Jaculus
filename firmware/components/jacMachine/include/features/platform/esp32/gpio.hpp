#pragma once

#include <jsmachine.hpp>
#include <driver/gpio.h>

namespace jac {

// Implement GPIO driver
//
// The driver exposes a function pin(identifier) that returns an object of type
// Gpio.
//
// The object has the following methods (for now):
// - setMode(mode): change the pin mode
// - digitalRead(): read the value of the GPIO
// - digitalWrite(): set the GPIO status
// - onChange(cb): attach a function on a callback, return handle
// - clearInterrupt(handle): given a handle, unregister interrupt handler
template < typename Self >
class GpioDriver {
    static inline constexpr const char* SLOT = "gpioDriverSlot";
public:
    MACHINE_FEATURE_SELF();

    struct Configuration {};

    void initialize() {
        setupSlot();

        gpio_install_isr_service( 0 );

        self().registerNativeModule( "gpio", [this]( duk_context *ctx ) {
            return self().initializeModule( ctx );
        });
    }

    void onEventLoop() {}
private:
    struct IsrId {
        Self *machine;
        gpio_num_t pin;
        int cbIndex;
        bool lastLevel;
    };

    void setupSlot() {
        duk_push_heap_stash( self()._context );
        duk_push_object( self()._context );
        duk_put_prop_string( self()._context, -2, SLOT );
        duk_pop( self()._context );
    }

    // Initializes the module; there are the following arguments on the
    // Duktape stack:
    // - 0 requested module ID
    // - 1 exports object
    // - 2 module object
    duk_ret_t initializeModule( duk_context *ctx ) {
        const int exportOffset = 1;

        // Register the Gpio class to exports
        dukPushClass( ctx, "Gpio", gpioConstructor, 1,
            {
                { "setMode", gpioSetMode, 1 },
                { "digitalRead", digitalRead, 0 },
                { "digitalWrite", digitalWrite, 1 },
                { "onChange", onChange, 1},
                { nullptr, nullptr, 0 }
            },
            {
                { nullptr, nullptr, 0 }
            } );
        duk_put_prop_string( ctx, exportOffset, "Gpio" );

        // Register getPin function
        duk_push_c_function( ctx, getPin, 1 );
        duk_put_prop_string( ctx, exportOffset, "getPin" );

        return dukReturn( ctx );
    }

    // The constructor takes pin number
    static duk_ret_t gpioConstructor( duk_context *ctx ) {
        if ( !duk_is_constructor_call( ctx ) ) {
            return DUK_RET_TYPE_ERROR;
        }

        int gpioIndex = duk_require_int( ctx, 0 );
        // TBA: Validate the index

        duk_push_this( ctx );
        auto thisOffset = duk_get_top_index( ctx );

        duk_push_int( ctx, gpioIndex );
        duk_put_prop_string( ctx, thisOffset, "_pin" );

        duk_pop( ctx ); // Pop this

        return dukReturn( ctx );
    }

    static duk_ret_t gpioSetMode( duk_context *ctx ) {
        const std::string modeString = duk_require_string( ctx, 0 );
        gpio_num_t pinNumber = getPinNumberFromThis( ctx );

        // TBA: Implement proper modes, think about the API
        gpio_config_t ioConf{};
        ioConf.pin_bit_mask = 1ULL << pinNumber;
        ioConf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        ioConf.pull_up_en = GPIO_PULLUP_DISABLE;
        if ( modeString == "output" ) {
            ioConf.mode = GPIO_MODE_OUTPUT;
            gpio_intr_disable( pinNumber );
        }
        else if ( modeString == "input" ) {
            ioConf.mode = GPIO_MODE_INPUT;
            gpio_intr_enable( pinNumber );
            ioConf.pull_up_en = GPIO_PULLUP_ENABLE; // TBA: Remove, temporary for testing
        }
        else {
            dukRaiseError( ctx, "Unknown pin mode: " + modeString );
        }

        // Check if there is an interrupt handler attached:
        duk_push_heap_stash( ctx );
        duk_get_prop_string( ctx, -1, SLOT );
        if ( duk_has_prop_index( ctx, -1, pinNumber ) ) {
            duk_get_prop_index( ctx, -1,  pinNumber );
            if ( duk_get_length( ctx, -1 ) ) {
                ioConf.intr_type = GPIO_INTR_ANYEDGE;
            }
        }

        gpio_config( &ioConf );

        return dukReturn( ctx );
    }

    static duk_ret_t digitalRead( duk_context *ctx ) {
        gpio_num_t pinNumber = getPinNumberFromThis( ctx );
        bool level = gpio_get_level( pinNumber );

        return dukReturn( ctx, level );
    }

    static duk_ret_t digitalWrite( duk_context *ctx ) {
        duk_to_boolean( ctx, 0 );
        bool level = duk_require_boolean( ctx, -1 );
        gpio_num_t pinNumber = getPinNumberFromThis( ctx );

        gpio_set_level( pinNumber, level );
        return dukReturn( ctx );
    }

    // First argument is the callback
    static duk_ret_t onChange( duk_context *ctx ) {
        duk_require_function( ctx, 0 );

        gpio_num_t pinNumber = getPinNumberFromThis( ctx );

        // Isr id will be freed when the handler is removed
        IsrId *isrId = new IsrId;
        isrId->machine = &Self::fromContext( ctx );
        isrId->pin = pinNumber;
        isrId->lastLevel = gpio_get_level( pinNumber );

        // Ensure there is an array with callbacks and ensure the slot array is
        // on top of the stack
        duk_push_heap_stash( ctx );
        duk_get_prop_string( ctx, -1, SLOT );
        if ( !duk_has_prop_index( ctx, -1, pinNumber ) ) {
            duk_push_array( ctx );
            duk_put_prop_index( ctx, -2, pinNumber );
        }
        duk_get_prop_index( ctx, -1, pinNumber );
        auto pinArray = duk_get_top_index( ctx );
        int cbArrayLength = duk_get_length( ctx, pinArray );

        // Prepare object
        duk_push_bare_object( ctx );
        duk_dup( ctx, 0 );
        duk_put_prop_string( ctx, -2, "cb" );
        isrId->cbIndex = cbArrayLength;
        duk_push_pointer( ctx, isrId );
        duk_put_prop_string( ctx, -2, "id" );

        // Push callback info
        duk_put_prop_index( ctx, pinArray, cbArrayLength );

        gpio_isr_handler_add( pinNumber, isrHandler, isrId );
        gpio_set_intr_type( pinNumber, GPIO_INTR_ANYEDGE );
        gpio_intr_enable( pinNumber );

        return dukReturn( ctx, pinNumber << 16 | cbArrayLength );
    }

    static void isrHandler( void *arg ) {
        IsrId *isrId = reinterpret_cast< IsrId* >( arg );
        bool level = gpio_get_level( isrId->pin );
        if ( level == isrId->lastLevel )
            return;
        isrId->lastLevel = level;
        isrId->machine->handleInterrupt([]( void *arg ) {
            IsrId *isrId = reinterpret_cast< IsrId* >( arg );
            isrId->machine->schedule([&isrId]( duk_context* ctx ) {
                duk_require_stack( ctx, 4 );
                duk_push_c_function( ctx, isrHandlerJs, 3 );
                duk_push_int( ctx, isrId->cbIndex );
                duk_push_int( ctx, isrId->pin );
                duk_push_boolean( ctx, isrId->lastLevel );
            });
        }, arg );
    }

    static duk_ret_t isrHandlerJs( duk_context *ctx ) {
        int cbIndex = duk_require_int( ctx, 0 );
        int pinNumber = duk_require_int( ctx, 1 );
        // Simply obtain callback handler...
        duk_push_heap_stash( ctx );
        duk_get_prop_string( ctx, -1, SLOT );
        duk_get_prop_index( ctx, -1, pinNumber );
        duk_get_prop_index( ctx, -1, cbIndex );
        duk_get_prop_string( ctx, -1, "cb" );
        // ...and invoke it with pin number (0) and pin level (1)
        duk_push_int( ctx, pinNumber );
        duk_dup( ctx, 2 );
        duk_call( ctx, 2 );

        return dukReturn( ctx );
    }

    static duk_ret_t getPin( duk_context *ctx ) {
        // TBA: implement
        return DUK_RET_TYPE_ERROR;
    }

    static gpio_num_t getPinNumberFromThis( duk_context *ctx ) {
        duk_push_this( ctx );
        duk_get_prop_string( ctx, -1, "_pin" );
        int pinNumber = duk_require_int( ctx, -1 );
        duk_pop_2( ctx );
        return static_cast< gpio_num_t >( pinNumber );
    }
};

} // namespace jac