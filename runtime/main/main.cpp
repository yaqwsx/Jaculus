#include "sdkconfig.h"

#include <esp_system.h>
#include <esp_log.h>
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
        .rx_flow_ctrl_thresh = 63,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install( UART_NUM_0, 4096, 0, 0, NULL, ESP_INTR_FLAG_IRAM );
    uart_param_config( UART_NUM_0, &uart_config );

    esp_vfs_dev_uart_use_driver( 0 );
}

void setupLogToChannel( uint8_t channel ) {
    static jac::link::ChannelDesc runtimeLogChannel;
    static std::mutex runtimeLogMutex;

    runtimeLogChannel = jac::link::ChannelDesc{ xStreamBufferCreate( 512, 0 ), channel };
    jac::link::bindSinkChannel( runtimeLogChannel );
    esp_log_set_vprintf( []( const char * fmt, va_list args ) -> int {
        static std::array<uint8_t, 128> buffer;
        std::unique_lock lock{runtimeLogMutex};
        int bytes = vsnprintf( reinterpret_cast< char *>( buffer.data() ), buffer.size(), fmt, args );
        if (bytes > 0) {
            jac::link::writeSink( runtimeLogChannel, buffer.data(), bytes, 100 );
        }
        return bytes;
    });
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
    setupLogToChannel( 3 );
    link::initializeLink();
    auto stdoutDesc = jac::link::ChannelDesc{ xStreamBufferCreate( 512, 0 ), 1 };
    link::bindSourceChannel( stdoutDesc );
    link::bindSinkChannel( stdoutDesc );

    auto transferReaderChannel = jac::link::ChannelDesc{ xStreamBufferCreate( 512, 0 ), 2 };
    auto transferReporterChannel = jac::link::ChannelDesc{ xStreamBufferCreate( 512, 0 ), 2 };
    link::bindSourceChannel( transferReaderChannel );
    link::bindSinkChannel( transferReporterChannel );

    storage::initializeFatFs( "/spiflash" );
    storage::initializeUploader( "/spiflash", &transferReaderChannel, &transferReporterChannel );
    initNvs();
    
    while ( true ) {
        sys_delay_ms( 10 );
        link::notifySink( stdoutDesc );
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
