#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>

#include <uploader.hpp>
#include <uploaderFeatures/commandImplementation.hpp>
#include <uploaderFeatures/commandInterpreter.hpp>
#include <uploaderFeatures/channelReader.hpp>
#include <uploaderFeatures/channelReporter.hpp>

#include <jacUtility.hpp>

using namespace jac;
using namespace jac::storage;
using namespace jac::utility;

namespace {
    TaskHandle_t uploaderTask;
    const char* basePath = nullptr;
}

using UploaderInterface = Mixin<
    ChannelReader,
    ChannelReporter,
    CommandInterpreter,
    CommandImplementation >;

void uploaderRoutine( void * taskParam ) {
    // log.yieldString( "Uploader started\n" );

    while ( true ) {
        UploaderInterface interface;
        interface.bindReportStreamBuffer( StreamBufferHandle_t(taskParam) );

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
    StreamBufferHandle_t reportSB = xStreamBufferCreate( 512, 1 );
    xTaskCreate( uploaderRoutine, "uploader", 3584, reportSB, 1, &uploaderTask );
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
