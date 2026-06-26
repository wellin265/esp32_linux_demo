/**
 * @file app_main.cpp
 * @brief Korvo board test application entry
 */

#include <dirent.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_adc.h"
#include "app_camera.h"
#include "app_speech.h"
#include "ksdiy_lvgl_port.h"
#include "lvgl.h"
#include "page_about.h"
#include "page_button.h"
#include "page_camera.h"
#include "page_main.h"
#include "page_manager.h"
#include "page_speech.h"

#define TAG "KSDIY_TEST"

LV_FONT_DECLARE(myFont);

typedef struct {
    uint8_t addr;
    const char *name;
} i2c_required_dev_t;

static const i2c_required_dev_t s_audio_required_devices[] = {
    {0x18, "ES8311 audio DAC"},
    {0x40, "ES7210 audio ADC"},
};

static void SPIFFS_Directory(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGW(TAG, "Failed to open directory: %s", path);
        return;
    }

    while (true) {
        struct dirent *pe = readdir(dir);
        if (!pe) {
            break;
        }
        ESP_LOGI(TAG, "SPIFFS file: %s (type=%x)", pe->d_name, pe->d_type);
    }

    closedir(dir);
}

static bool detect_io_expander(void)
{
    return (i2c_master_probe(touch_i2c_bus_, 0x20, -1) == ESP_OK) ||
           (i2c_master_probe(touch_i2c_bus_, 0x38, -1) == ESP_OK);
}

static void show_hardware_error_screen(const bool *missing, bool io_expander_found)
{
    if (!ksdiy_lvgl_lock(-1)) {
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xCC0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Audio hardware missing");
    lv_obj_set_style_text_font(title, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Voice wakeup will not be available");
    lv_obj_set_style_text_font(hint, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFCCCC), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 40);

    int y_offset = 70;
    for (size_t i = 0; i < sizeof(s_audio_required_devices) / sizeof(s_audio_required_devices[0]); ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %s",
                 missing[i] ? "X" : "OK",
                 s_audio_required_devices[i].name);
        lv_obj_t *label = lv_label_create(scr);
        lv_label_set_text(label, buf);
        lv_obj_set_style_text_font(label, &myFont, LV_PART_MAIN);
        lv_obj_set_style_text_color(label,
                                    missing[i] ? lv_color_hex(0xFFFF00) : lv_color_hex(0x00FF00),
                                    LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 20, y_offset);
        y_offset += 24;
    }

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s IO expander",
                 io_expander_found ? "OK" : "WARN");
        lv_obj_t *label = lv_label_create(scr);
        lv_label_set_text(label, buf);
        lv_obj_set_style_text_font(label, &myFont, LV_PART_MAIN);
        lv_obj_set_style_text_color(label,
                                    io_expander_found ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFFF00),
                                    LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 20, y_offset);
    }

    ksdiy_lvgl_unlock();
}

static bool check_required_hardware(void)
{
    bool missing[sizeof(s_audio_required_devices) / sizeof(s_audio_required_devices[0])] = {false};
    bool any_missing = false;

    for (size_t i = 0; i < sizeof(s_audio_required_devices) / sizeof(s_audio_required_devices[0]); ++i) {
        esp_err_t ret = i2c_master_probe(touch_i2c_bus_, s_audio_required_devices[i].addr, -1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Missing required audio device: 0x%02X - %s",
                     s_audio_required_devices[i].addr, s_audio_required_devices[i].name);
            missing[i] = true;
            any_missing = true;
        }
    }

    if (i2c_master_probe(touch_i2c_bus_, 0x15, -1) != ESP_OK) {
        ESP_LOGW(TAG, "Touch controller not found, continue without screen interaction");
    }

    if (!detect_io_expander()) {
        ESP_LOGW(TAG, "IO expander not found, screen related features may be unavailable");
    }

    if (any_missing) {
        if (ksdiy_lvgl_is_ready()) {
            show_hardware_error_screen(missing, detect_io_expander());
        }
        ESP_LOGW(TAG, "Audio path is incomplete, skip voice wakeup init");
        return false;
    }

    ESP_LOGI(TAG, "Required audio hardware detected");
    return true;
}

static void scan_i2c_devices(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus");
    for (uint8_t i = 0x01; i < 0x7F; ++i) {
        if (i2c_master_probe(touch_i2c_bus_, i, -1) == ESP_OK) {
            const char *dev_name = "unknown";
            switch (i) {
                case 0x15: dev_name = "CST816S touch"; break;
                case 0x18: dev_name = "ES8311 DAC"; break;
                case 0x20: dev_name = "TCA9554 IO expander"; break;
                case 0x38: dev_name = "TCA9554A IO expander"; break;
                case 0x40: dev_name = "ES7210 ADC"; break;
                default: break;
            }
            ESP_LOGI(TAG, "I2C device: 0x%02X - %s", i, dev_name);
        }
    }
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing and reinitializing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
        return;
    }

    SPIFFS_Directory("/spiffs/");

    ESP_LOGI(TAG, "Initializing LVGL/display");
    ksdiy_lvgl_port_init();

    if (ksdiy_lvgl_is_ready()) {
        ESP_LOGI(TAG, "Registering UI pages");
        page_manager_register(&page_camera);
        page_manager_register(&page_button);
        page_manager_register(&page_speech);
        page_manager_register(&page_about);
        page_manager_init();
    } else {
        ESP_LOGW(TAG, "Display not available, run in headless voice mode");
    }

    scan_i2c_devices();

    bool audio_ready = check_required_hardware();

    ESP_LOGI(TAG, "Initializing camera");
    if (app_camera_init() == ESP_OK) {
        ESP_LOGI(TAG, "Camera initialized");
    } else {
        ESP_LOGW(TAG, "Camera init failed");
    }

    ESP_LOGI(TAG, "Initializing ADC buttons");
    ADC_BUTTON_Init();

    if (audio_ready) {
        ESP_LOGI(TAG, "Initializing voice wakeup");
        Wakenet_init();
        ESP_LOGI(TAG, "Voice wakeup initialized");
    } else {
        ESP_LOGW(TAG, "Skip voice wakeup init because audio hardware is incomplete");
    }

    if (ksdiy_lvgl_lock(-1)) {
        page_main_create();
        ksdiy_lvgl_unlock();
    } else {
        ESP_LOGI(TAG, "Headless mode active");
    }

    ESP_LOGI(TAG, "Korvo board test started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
