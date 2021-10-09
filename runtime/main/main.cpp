#include "sdkconfig.h"

#include <esp_system.h>
// This shouldn't be necessary, but ESP-IDF has broken guards.
// Relevant issue: https://github.com/espressif/esp-idf/issues/7204
extern "C" {
    #include <esp_vfs_dev.h>
    #include <esp_vfs.h>
}
#include <driver/gpio.h>
#include <driver/uart.h>
#include <iostream>

#include <duk_console.h>
#include <jsmachine.hpp>
#include <features/cMemoryAllocator.hpp>
#include <features/nodeModules.hpp>
#include <features/socketDebugger.hpp>
#include <features/stdoutErrorHandler.hpp>
#include <features/rtosTimers.hpp>
#include <features/promise.hpp>
#include <features/platform/esp32/gpio.hpp>

#include <storage.hpp>
#include <uploader.hpp>
#include <link.hpp>

#include "wifi.h"

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

// Uncomment the following line to enable the proof-of-concept debugger
// #define ENABLE_TEMPORARY_DEBUGGER

#ifdef ENABLE_TEMPORARY_DEBUGGER
    #include "credentials.hpp"
#endif

void gpioIntr(void *arg) {
    jac::storage::enterUploader();
}

void setupGpio() {
    gpio_install_isr_service( 0 );
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_0;
    gpio_config( &io_conf );

    gpio_isr_handler_add( GPIO_NUM_0, gpioIntr, (void*) GPIO_NUM_0 );
}

void setupUartDriver() {
    uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 127,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install( UART_NUM_0, 4096, 0, 0, NULL, 0 );
    uart_param_config( UART_NUM_0, &uart_config );

    esp_vfs_dev_uart_use_driver( 0 );
}

extern "C" void app_main() {
    using namespace jac;

    // Define javascript machines capabilities
    using JsMachine = JsMachineBase<
            StdoutErrorHandler,
            CMemoryAllocator,
            RtosTimers,
            NodeModuleLoader,
            SocketDebugger,
            Promise,
            GpioDriver
        >;

    setupUartDriver(); // Without UART drive stdio is non-blocking
    setupGpio();
    link::initializeLink();
    auto stdoutSb = xStreamBufferCreate( 512, 0 );
    link::bindSinkStreamBuffer( stdoutSb, 1 );
    storage::initializeFatFs( "/spiflash" );
    storage::initializeUploader( "/spiflash" );
    initNvs();

    while ( true ) {
        xStreamBufferSend( stdoutSb, "ein", 3, portMAX_DELAY );
        xStreamBufferSend( stdoutSb, "zwei", 4, portMAX_DELAY );
        xStreamBufferSend( stdoutSb, "drei", 4, portMAX_DELAY );
        xStreamBufferSend( stdoutSb, "vier", 4, portMAX_DELAY );
        xStreamBufferSend( stdoutSb, "fuenf", 5, portMAX_DELAY );
        link::notifySink( stdoutSb );
        sys_delay_ms( 100 );
        xStreamBufferSend( stdoutSb, "jedna", 5, portMAX_DELAY );
        link::notifySink( stdoutSb );
        sys_delay_ms( 100 );
        xStreamBufferSend( stdoutSb, "dva", 3, portMAX_DELAY );
        link::notifySink( stdoutSb );
        sys_delay_ms( 100 );
        xStreamBufferSend( stdoutSb, "try", 3, portMAX_DELAY );
        link::notifySink( stdoutSb );
        sys_delay_ms( 100 );
        xStreamBufferSend( stdoutSb, "ctyry", 5, portMAX_DELAY );
        link::notifySink( stdoutSb );
        
        sys_delay_ms( 1000 );
    }

    #ifdef ENABLE_TEMPORARY_DEBUGGER
        WiFiConnector connector;
        if ( !connector.sync().connect( SSID, password ) ) {
            std::cout << "WiFi connection failed\n";
        } else {
            connector.waitForIp();
            std::cout << "IP address: " << connector.ipAddrStr() << "\n";
        }
    #endif

    try {
        JsMachine::Configuration cfg;
        cfg.basePath = "/spiflash";
        JsMachine machine( cfg );

        machine.extend( []( JsMachine* machine, duk_context* ctx) {
            duk_console_init( ctx, 0 );
        });

        #ifdef ENABLE_TEMPORARY_DEBUGGER
            machine.waitForDebugger();
        #endif

        // The following code makes stacktraces richer
        // TBA: Refactor into a separate machine feature
        machine.evalString( R"(
            Duktape.errCreate = function (err) {
                try {
                    if (typeof err === 'object' &&
                        typeof err.message !== 'undefined' &&
                        typeof err.lineNumber === 'number') {
                        err.message = err.message + ' (line ' + err.lineNumber + ')';
                    }
                } catch (e) {
                    // ignore; for cases such as where "message" is not writable etc
                }
                return err;
            }
        )" );

        machine.evaluateMain( "index.js" );
        machine.runEventLoop();
    }
    catch( const std::runtime_error& e ) {
        std::cerr << "FAILED with runtime error: " << e.what() << "\n";
    }
    catch( const std::exception& e ) {
        std::cerr << "FAILED: " << e.what() << "\n";
    }

    esp_restart();
}
