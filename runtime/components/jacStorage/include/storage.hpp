#pragma once

namespace jac::storage {

void initializeFatFs( const char* path );
void unmountPartition();

} // namespace jac::storage