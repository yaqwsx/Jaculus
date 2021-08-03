/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_spi_flash.h>
#include "esp_vfs_dev.h"
#include "esp_vfs.h"
#include "esp_log.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <iostream>

int ets_printf(const char *fmt, ...);

#include <duk_console.h>
#include <jsmachine.hpp>
#include <features/cMemoryAllocator.hpp>
#include <features/nodeModules.hpp>
#include <features/socketDebugger.hpp>
#include <features/stdoutErrorHandler.hpp>
#include <features/rtosTimers.hpp>
#include <features/promise.hpp>

#include <storage.hpp>
#include <uploader.hpp>

#include "wifi.h"
#include "credentials.hpp"


const char* TAG = "MAIN";

void testFs() {
    ESP_LOGI(TAG, "Reading file");
    FILE *f = fopen("/spiflash/hello.txt", "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[10];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);
}

void gpioIntr(void *arg) {
    jac::storage::enterUploader();
}

void setupGpio() {
    gpio_install_isr_service(0);
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_0;
    gpio_config(&io_conf);

    gpio_isr_handler_add(GPIO_NUM_0, gpioIntr, (void*) GPIO_NUM_0);
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
    uart_driver_install(UART_NUM_0, 4096, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);

    esp_vfs_dev_uart_use_driver(0);
}

extern "C" void app_main(void)
{
    using namespace jac;

    using JsMachine = JsMachineBase<
            StdoutErrorHandler,
            CMemoryAllocator,
            RtosTimers,
            NodeModuleLoader,
            SocketDebugger,
            Promise
        >;


    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf( "Just after boot: %d bytes/ %d bytes\n", esp_get_free_heap_size(), heap_caps_get_total_size( MALLOC_CAP_INTERNAL ) );
    setupUartDriver();
    setupGpio();
    storage::initializeFatFs( "/spiflash" );
    storage::initializeUploader( "/spiflash" );
    initNvs();

    printf( "Just before connecting to WiFI: %d bytes\n", esp_get_free_heap_size() );
    // WiFiConnector connector;
    // if ( !connector.sync().connect( SSID, password ) ) {
    //     std::cout << "WiFi connection failed\n";
    // } else {
    //     connector.waitForIp();
    //     std::cout << "IP address: " << connector.ipAddrStr() << "\n";
    // }


    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size() );

    try {
        printf( "Before machine: %d bytes\n", esp_get_free_heap_size() );

        JsMachine::Configuration cfg;
        cfg.basePath = "/spiflash";
        JsMachine machine( cfg );

        machine.extend( []( JsMachine* machine, duk_context* ctx) {
            duk_console_init( ctx, 0 );
        });

        printf("After machine: %d bytes\n", esp_get_free_heap_size());

        // machine.waitForDebugger();

        machine.evalString(R"(
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
        )");

        machine.evaluateMain( "index.js" );

        // machine.evalString(R"(
        //         createTimer(1000, false, function() {
        //             console.log("Hello world!");
        //         });
        //         console.log("Starting!");
        //     )");
        machine.runEventLoop();
    }
    catch( const std::runtime_error& e ) {
        std::cerr << "FAILED with runtime error: " << e.what() << "\n";
    }
    catch( const std::exception& e ) {
        std::cerr << "FAILED: " << e.what() << "\n";
    }

    // duk_context *ctx = duk_create_heap_default();
    // duk_eval_string(ctx, "1+2");
    // printf("Javascript interpreter says: 1+2=%d\n", (int) duk_get_int(ctx, -1));
    // duk_destroy_heap(ctx);

    // for (int i = 60; i >= 0; i--) {
    //     printf("Restarting in %d seconds...\n", i);
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
    // printf("Restarting now.\n");
    // fflush(stdout);
    // esp_restart();
}
