/**
 * @file display.h
 * @brief FFT spectrum display with switchable visual styles.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "lvgl.h"

#define DISPLAY_BAND_COUNT 40

typedef enum {
    DISPLAY_STYLE_CLASSIC = 0,
    DISPLAY_STYLE_MIRROR,
    DISPLAY_STYLE_AURORA,
    DISPLAY_STYLE_CONSTELLATION,
    DISPLAY_STYLE_DREAM_WAVE,
    DISPLAY_STYLE_RADIAL_BLOOM,
    DISPLAY_STYLE_RADAR,
    DISPLAY_STYLE_EMBER,
    DISPLAY_STYLE_NEON_CITY,
    DISPLAY_STYLE_MAX,
} display_style_t;

void display_set_canvas(lv_obj_t *canvas);

esp_err_t display_draw(float *data);

esp_err_t display_buffer_init(void);

void display_set_style(display_style_t style);

display_style_t display_get_style(void);

void display_next_style(void);

#ifdef __cplusplus
}
#endif
