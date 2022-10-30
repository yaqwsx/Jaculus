#pragma once

#include <link.hpp>

namespace jac::storage {

void initializeUploader(
    const char *storagePrefix,
    jac::link::ChannelDesc *readerChannel,
    jac::link::ChannelDesc *reporterChannel );
const char *getStoragePrefix();

} // namespace jac::storage