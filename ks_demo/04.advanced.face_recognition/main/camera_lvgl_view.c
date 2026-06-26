#include "camera_lvgl_view.h"

#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "ksdiy_lvgl_port.h"

static const char *TAG = "camera_lvgl_view";

static lv_obj_t *s_camera_img_obj;
static lv_image_dsc_t s_camera_img_dsc;
static lv_color_t *s_camera_pixels;
static uint16_t s_width;
static uint16_t s_height;

static inline uint16_t rgb565_swap_bytes(uint16_t pixel)
{
    return (uint16_t)((pixel << 8) | (pixel >> 8));
}

esp_err_t camera_lvgl_view_init(uint16_t width, uint16_t height)
{
    s_width = width;
    s_height = height;
    s_camera_pixels = heap_caps_calloc(width * height, sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_camera_pixels != NULL, ESP_ERR_NO_MEM, TAG, "alloc image buffer failed");

    s_camera_img_dsc.header.w = width;
    s_camera_img_dsc.header.h = height;
    s_camera_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_camera_img_dsc.data_size = width * height * sizeof(lv_color_t);
    s_camera_img_dsc.data = (const uint8_t *)s_camera_pixels;

    ESP_RETURN_ON_FALSE(ksdiy_lvgl_lock(1000), ESP_ERR_TIMEOUT, TAG, "lvgl lock timeout");
    s_camera_img_obj = lv_image_create(lv_screen_active());
    lv_image_set_src(s_camera_img_obj, &s_camera_img_dsc);
    lv_obj_set_pos(s_camera_img_obj, (lv_disp_get_hor_res(NULL) - width) / 2, (lv_disp_get_ver_res(NULL) - height) / 2);
    lv_obj_clear_flag(s_camera_img_obj, LV_OBJ_FLAG_SCROLLABLE);
    ksdiy_lvgl_unlock();

    return ESP_OK;
}

esp_err_t camera_lvgl_view_update(const void *pixels, uint16_t width, uint16_t height)
{
    ESP_RETURN_ON_FALSE(s_camera_pixels != NULL, ESP_ERR_INVALID_STATE, TAG, "view not initialized");
    ESP_RETURN_ON_FALSE(width == s_width && height == s_height, ESP_ERR_INVALID_SIZE, TAG, "frame size mismatch");

    const uint16_t *src = (const uint16_t *)pixels;
    uint16_t *dst = (uint16_t *)s_camera_pixels;
    size_t pixel_count = (size_t)width * height;

    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i] = rgb565_swap_bytes(src[i]);
    }

    ESP_RETURN_ON_FALSE(ksdiy_lvgl_lock(100), ESP_ERR_TIMEOUT, TAG, "lvgl lock timeout");
    lv_obj_invalidate(s_camera_img_obj);
    ksdiy_lvgl_unlock();

    return ESP_OK;
}
