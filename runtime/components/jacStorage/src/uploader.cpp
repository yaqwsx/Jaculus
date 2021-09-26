#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <uploader.hpp>
#include <uploaderFeatures/commandImplementation.hpp>
#include <uploaderFeatures/commandInterpreter.hpp>
#include <uploaderFeatures/stdinReader.hpp>
#include <uploaderFeatures/stdoutReporter.hpp>

#include <jacUtility.hpp>

using namespace jac;
using namespace jac::storage;
using namespace jac::utility;

namespace {
    TaskHandle_t uploaderTask;
    const char* basePath = nullptr;
}

using UploaderInterface = Mixin<
    StdinReader,
    StdoutReporter,
    CommandInterpreter,
    CommandImplementation >;

void uploaderRoutine( void * ) {
    UploaderInterface interface;
    interface.yieldString( "Uploader started\n" );

    while ( true ) {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        interface.discardBufferedStdin();

        do {
            interface.interpretCommand();
        } while ( !interface.finished() );

    }
}

void jac::storage::initializeUploader( const char *path ) {
    assert( uploaderTask == nullptr );
    basePath = path;
    xTaskCreate( uploaderRoutine, "uploader", 3584, nullptr, 1, &uploaderTask );
}

void jac::storage::enterUploader() {
    if ( xPortInIsrContext() )
        vTaskNotifyGiveFromISR( uploaderTask, nullptr );
    else
        xTaskNotifyGive( uploaderTask );
}

const char* jac::storage::getStoragePrefix() {
    return basePath;
}
