/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ksdiy_example_display.h"

static const char *TAG = "nvs_example";

static void update_display(const char *line1, const char *line2, const char *line3)
{
    ksdiy_example_display_set_lines(line1, line2, line3);
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.nvs", "non-volatile storage");
    update_display("init nvs...", "", "");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        update_display("open failed", esp_err_to_name(err), "");
        return;
    }

    int32_t counter = 42;
    ESP_ERROR_CHECK(nvs_set_i32(my_handle, "counter", counter));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, "message", "Hello from NVS!"));

    int32_t read_counter = 0;
    err = nvs_get_i32(my_handle, "counter", &read_counter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read counter: %s", esp_err_to_name(err));
        update_display("read counter failed", esp_err_to_name(err), "");
        nvs_close(my_handle);
        return;
    }

    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_get_str(my_handle, "message", NULL, &required_size));
    char *message = malloc(required_size);
    if (message == NULL) {
        update_display("malloc failed", "", "");
        nvs_close(my_handle);
        return;
    }
    ESP_ERROR_CHECK(nvs_get_str(my_handle, "message", message, &required_size));

    err = nvs_erase_key(my_handle, "counter");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase key: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    char line1[64];
    char line2[64];
    snprintf(line1, sizeof(line1), "counter=%" PRIi32, read_counter);
    snprintf(line2, sizeof(line2), "msg=%s", message);
    update_display(line1, line2, "key 'counter' erased");

    ESP_LOGI(TAG, "Read counter=%" PRIi32 ", message=%s", read_counter, message);
    free(message);
}
