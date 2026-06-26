/*
 * Public Domain (or CC0 licensed, at your option.)
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "ksdiy_example_display.h"

static const char *TAG = "example";

static void update_status(const char *line1, const char *line2, const char *line3)
{
    ksdiy_example_display_set_lines(line1, line2, line3);
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.spiffs", "SPI flash file system");
    update_status("Mounting /spiffs...", "", "");

    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
            update_status("SPIFFS mount failed", "Mount or format error", "");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
            update_status("SPIFFS mount failed", "Partition not found", "");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            update_status("SPIFFS init failed", esp_err_to_name(ret), "");
        }
        return;
    }

#ifdef CONFIG_EXAMPLE_SPIFFS_CHECK_ON_START
    ESP_LOGI(TAG, "Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        update_status("SPIFFS check failed", esp_err_to_name(ret), "");
        return;
    }
#endif

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        update_status("SPIFFS info failed", esp_err_to_name(ret), "");
        return;
    }

    if (used > total) {
        ESP_LOGW(TAG, "Used bytes larger than total bytes, running SPIFFS_check()");
        ret = esp_spiffs_check(conf.partition_label);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            update_status("SPIFFS check failed", esp_err_to_name(ret), "");
            return;
        }
    }

    char line1[64];
    char line2[64];
    snprintf(line1, sizeof(line1), "mounted total=%uKB", (unsigned) (total / 1024));
    snprintf(line2, sizeof(line2), "used=%uKB free=%uKB",
             (unsigned) (used / 1024), (unsigned) ((total - used) / 1024));
    update_status(line1, line2, "writing hello.txt");

    FILE *f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        update_status(line1, line2, "open write failed");
        return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);
    ESP_LOGI(TAG, "File written");

    struct stat st;
    if (stat("/spiffs/foo.txt", &st) == 0) {
        unlink("/spiffs/foo.txt");
    }

    if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0) {
        ESP_LOGE(TAG, "Rename failed");
        update_status(line1, line2, "rename failed");
        return;
    }

    f = fopen("/spiffs/foo.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        update_status(line1, line2, "open read failed");
        return;
    }

    char text[64];
    if (fgets(text, sizeof(text), f) == NULL) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to read file");
        update_status(line1, line2, "read failed");
        return;
    }
    fclose(f);

    char *newline = strchr(text, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }

    ESP_LOGI(TAG, "Read from file: '%s'", text);
    update_status(line1, line2, text);

    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}
