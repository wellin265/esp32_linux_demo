#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KSDIY_LCD_H_RES 280
#define KSDIY_LCD_V_RES 240

esp_err_t ksdiy_display_lcd_init(void);
esp_err_t ksdiy_display_draw_bitmap(const void *data, uint16_t width, uint16_t height);

esp_err_t ksdiy_display_touch_init(void);
bool ksdiy_display_touch_ready(void);
esp_err_t ksdiy_display_touch_read(
    uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_count, uint8_t max_points);

#ifdef __cplusplus
}
#endif
