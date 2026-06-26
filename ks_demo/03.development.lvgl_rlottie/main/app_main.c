#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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

static const char *TAG = "example";

static char file_name_with_path[128] = {0};
static char initial_lottie_path[128] = {0};
static uint8_t *fb = NULL;
static lv_obj_t *ui_face_canvas = NULL;
static uint8_t *loaded_lottie_data = NULL;
static size_t loaded_lottie_size = 0;

static bool load_lottie_from_path(lv_obj_t *canvas, const char *path)
{
    struct stat st = {0};
    FILE *fp = NULL;
    uint8_t *data = NULL;
    bool ok = false;

    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "Lottie file not found: %s", path);
        return false;
    }

    ESP_LOGI(TAG, "Loading lottie: %s, size=%ld", path, (long)st.st_size);
    fp = fopen(path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open lottie file: %s", path);
        return false;
    }

    data = heap_caps_malloc(st.st_size, MALLOC_CAP_8BIT);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %ld bytes for %s", (long)st.st_size, path);
        fclose(fp);
        return false;
    }

    if (fread(data, 1, st.st_size, fp) != (size_t)st.st_size) {
        ESP_LOGE(TAG, "Failed to read lottie file: %s", path);
        goto cleanup;
    }

    if (loaded_lottie_data) {
        free(loaded_lottie_data);
        loaded_lottie_data = NULL;
        loaded_lottie_size = 0;
    }

    loaded_lottie_data = data;
    loaded_lottie_size = st.st_size;
    data = NULL;

    lv_lottie_set_src_data(canvas, (const void *)loaded_lottie_data, loaded_lottie_size);

    lv_anim_t *anim = lv_lottie_get_anim(canvas);
    if (anim) {
        ESP_LOGI(TAG, "Lottie anim ready: start=%d, end=%d, time=%d",
                 anim->start_value, anim->end_value, (int)anim->time);
        ok = true;
    } else {
        ESP_LOGE(TAG, "Failed to create lottie animation from: %s", path);
    }

cleanup:
    if (fp) {
        fclose(fp);
    }
    if (data) {
        free(data);
    }
    return ok;
}

static void initial_lottie_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    if (initial_lottie_path[0] != '\0' && ui_face_canvas != NULL) {
        if (ksdiy_lvgl_lock(-1)) {
            load_lottie_from_path(ui_face_canvas, initial_lottie_path);
            ksdiy_lvgl_unlock();
        }
    } else {
        ESP_LOGW(TAG, "No initial lottie path available");
    }

    vTaskDelete(NULL);
}

static void btn_event_cb(lv_event_t *event)
{
    lv_obj_t *canvas = (lv_obj_t *)lv_event_get_user_data(event);
    lv_obj_t *btn = lv_event_get_target(event);
    lv_obj_t *list = lv_obj_get_parent(btn);
    const char *file_name = lv_list_get_btn_text(list, btn);

    if (file_name == NULL) {
        ESP_LOGE(TAG, "Failed to get list button text");
        return;
    }

    ESP_LOGI(TAG, "btn_event_cb: %s", file_name);

    snprintf(file_name_with_path, sizeof(file_name_with_path), "/spiffs/%s", file_name);
    load_lottie_from_path(canvas, file_name_with_path);
}

static void image_display(void)
{
    lv_obj_t *list = lv_list_create(lv_scr_act());
    lv_obj_set_size(list, 120, 240);
    lv_obj_set_style_border_width(list, 0, LV_STATE_DEFAULT);
    lv_obj_align(list, LV_ALIGN_LEFT_MID, 0, 0);

    ui_face_canvas = lv_lottie_create(lv_scr_act());
    lv_lottie_set_buffer(ui_face_canvas, 280, 240, fb);
    lv_obj_align(ui_face_canvas, LV_ALIGN_CENTER, 0, 0);

    DIR *p_dir_stream = opendir("/spiffs");
    struct dirent *p_dirent = NULL;

    if (p_dir_stream == NULL) {
        ESP_LOGE(TAG, "Failed to open /spiffs");
        return;
    }

    while ((p_dirent = readdir(p_dir_stream)) != NULL) {
        if (p_dirent->d_type != DT_REG) {
            continue;
        }

        if (strstr(p_dirent->d_name, ".json") == NULL) {
            continue;
        }

        lv_obj_t *btn = lv_list_add_btn(list, NULL, p_dirent->d_name);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, ui_face_canvas);

        if (initial_lottie_path[0] == '\0') {
            snprintf(initial_lottie_path, sizeof(initial_lottie_path), "/spiffs/%s", p_dirent->d_name);
        }
    }

    closedir(p_dir_stream);

    if (initial_lottie_path[0] == '\0') {
        ESP_LOGW(TAG, "No playable json file found in /spiffs");
    }
}

static void SPIFFS_Directory(char *path)
{
    DIR *dir = opendir(path);
    assert(dir != NULL);

    while (true) {
        struct dirent *pe = readdir(dir);
        if (!pe) {
            break;
        }

        ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x",
                 pe->d_name, pe->d_ino, pe->d_type);
    }

    closedir(dir);
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

    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    SPIFFS_Directory("/spiffs/");

    fb = heap_caps_malloc(280 * 240 * 4, MALLOC_CAP_SPIRAM);
    assert(fb);

    ksdiy_lvgl_port_init();

    if (ksdiy_lvgl_lock(0)) {
        image_display();
        ksdiy_lvgl_unlock();
    }

    xTaskCreatePinnedToCore(initial_lottie_task, "lottie_init", 1024 * 8, NULL, 5, NULL, 1);
}
