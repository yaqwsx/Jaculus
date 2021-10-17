#pragma once

#include <esp_log.h>

#define JAC_LOGI( tag, format, ... ) ESP_LOGI( tag, format, ##__VA_ARGS__ )
#define JAC_LOGW( tag, format, ... ) ESP_LOGW( tag, format, ##__VA_ARGS__ )
