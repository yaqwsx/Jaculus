#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>

#include <uploader.hpp>
#include <uploaderFeatures/commandImplementation.hpp>
#include <uploaderFeatures/commandInterpreter.hpp>
#include <uploaderFeatures/channelReader.hpp>
#include <uploaderFeatures/channelReporter.hpp>

#include <jacUtility.hpp>
#include <jacLog.hpp>

using namespace jac;
using namespace jac::storage;
using namespace jac::utility;
using namespace jac::link;

namespace {
    TaskHandle_t uploaderTask;
    struct TaskParams {
        ChannelDesc *readerChannel;
        ChannelDesc *reporterChannel;
    } taskParams;
    const char* basePath = nullptr;
}

using UploaderInterface = Mixin<
    ChannelReader,
    ChannelReporter,
    CommandInterpreter,
    CommandImplementation >;


void uploaderRoutine( void * taskParam ) {
    JAC_LOGI( "uploader", "Uploader started" );

    while ( true ) {
        UploaderInterface interface;
        interface.bindReporterChannel( *reinterpret_cast< TaskParams* >( taskParam )->reporterChannel );
        interface.bindReaderChannel( *reinterpret_cast< TaskParams* >( taskParam )->readerChannel );

        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        interface.discardBufferedInput();

        do {
            interface.interpretCommand();
        } while ( !interface.finished() );
    }
}

void jac::storage::initializeUploader(
        const char *path,
        ChannelDesc *readerChannel,
        ChannelDesc *reporterChannel ) {
    assert( uploaderTask == nullptr );
    basePath = path;
    taskParams.readerChannel = readerChannel;
    taskParams.reporterChannel = reporterChannel;
    xTaskCreate( uploaderRoutine, "uploader", 3584, &taskParams, 1, &uploaderTask );
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
