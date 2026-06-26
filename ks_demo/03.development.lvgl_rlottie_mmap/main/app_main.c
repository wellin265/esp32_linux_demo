#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "ksdiy_lvgl_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "esp_lv_fs.h"
#include "lv_lottie.h"

#include "mmap_generate_lottie_assets.h"

mmap_assets_handle_t asset_lottie;

static const char *TAG = "example";

static lv_obj_t *ui_face_canvas = NULL;
static uint8_t *fb = NULL;

static bool load_lottie_from_asset(enum MMAP_LOTTIE_ASSETS_LISTS asset_id, const char *name)
{
    const void *data = mmap_assets_get_mem(asset_lottie, asset_id);
    size_t size = mmap_assets_get_size(asset_lottie, asset_id);

    if (data == NULL || size == 0) {
        ESP_LOGE(TAG, "Failed to get mmap lottie asset: %s", name);
        return false;
    }

    ESP_LOGI(TAG, "Loading mmap lottie: %s, size=%u", name, (unsigned)size);
    lv_lottie_set_src_data(ui_face_canvas, data, size);

    lv_anim_t *anim = lv_lottie_get_anim(ui_face_canvas);
    if (anim == NULL) {
        ESP_LOGE(TAG, "Failed to create lottie animation from asset: %s", name);
        return false;
    }

    ESP_LOGI(TAG, "Lottie anim ready: start=%d, end=%d, time=%d",
             anim->start_value, anim->end_value, (int)anim->time);
    return true;
}

static void initial_lottie_task(void *arg)
{
    LV_UNUSED(arg);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (ui_face_canvas != NULL && ksdiy_lvgl_lock(-1)) {
        load_lottie_from_asset(MMAP_LOTTIE_ASSETS_SPEAK_JSON, "speak.json");
        ksdiy_lvgl_unlock();
    }

    vTaskDelete(NULL);
}

static void app_mount_mmap_fs(void)
{
    const mmap_assets_config_t config_lottie = {
        .partition_label = "animation",
        .max_files = MMAP_LOTTIE_ASSETS_FILES,
        .checksum = MMAP_LOTTIE_ASSETS_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    mmap_assets_new(&config_lottie, &asset_lottie);
    ESP_LOGI(TAG, "[%s]stored_files:%d",
             config_lottie.partition_label,
             mmap_assets_get_stored_files(asset_lottie));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    fb = heap_caps_malloc(280 * 240 * 4, MALLOC_CAP_SPIRAM);
    assert(fb);

    app_mount_mmap_fs();
    ksdiy_lvgl_port_init();

    if (ksdiy_lvgl_lock(0)) {
        ui_face_canvas = lv_lottie_create(lv_scr_act());
        lv_lottie_set_buffer(ui_face_canvas, 280, 240, fb);
        lv_obj_align(ui_face_canvas, LV_ALIGN_CENTER, 0, 0);
        ksdiy_lvgl_unlock();
    }

    xTaskCreatePinnedToCore(initial_lottie_task, "lottie_init", 1024 * 8, NULL, 5, NULL, 1);
}
