#pragma once

namespace jac::storage {

void initializeUploader( const char *storagePrefix );
void enterUploader();
const char *getStoragePrefix();

} // namespace jac::storage