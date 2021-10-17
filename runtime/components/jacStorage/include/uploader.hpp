#pragma once

#include <link.hpp>

namespace jac::storage {

void initializeUploader(
    const char *storagePrefix,
    jac::link::ChannelDesc *readerChannel,
    jac::link::ChannelDesc *reporterChannel );
void enterUploader();
const char *getStoragePrefix();

} // namespace jac::storage