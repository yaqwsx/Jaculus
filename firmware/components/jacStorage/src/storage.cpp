#include <storage.hpp>

// There are missing guards, fixed in
// https://github.com/espressif/esp-idf/commit/cbf207bfb83156ece449a10908cad0615d66ec52
extern "C" {
    #include <esp_vfs.h>
    #include <esp_vfs_fat.h>
    #include <esp_system.h>
}

namespace {

wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
const char *base_path = nullptr;

} // namespace

void jac::storage::initializeFatFs( const char* path ) {
    esp_vfs_fat_mount_config_t mountConfig{};
    mountConfig.max_files = 20;
    mountConfig.format_if_mount_failed = true,
    mountConfig.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

    base_path = path;

    ESP_ERROR_CHECK( esp_vfs_fat_spiflash_mount(path, "storage", &mountConfig, &s_wl_handle) );
}

void jac::storage::unmountPartition() {
    ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount( base_path, s_wl_handle ) );
}
