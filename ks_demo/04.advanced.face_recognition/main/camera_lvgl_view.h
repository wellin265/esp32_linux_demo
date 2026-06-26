#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t camera_lvgl_view_init(uint16_t width, uint16_t height);
esp_err_t camera_lvgl_view_update(const void *pixels, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif
