/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include "ksdiy_example_display.h"

static const char *TAG = "example";
static const char *base_path = "/spiflash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

static void update_display(const char *line1, const char *line2, const char *line3)
{
    ksdiy_example_display_set_lines(line1, line2, line3);
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.fatfs", "flash FAT filesystem");
    update_display("mounting FATFS...", "", "");

    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        update_display("mount failed", esp_err_to_name(err), "");
        return;
    }

    const char *filename = "/spiflash/example.txt";
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        update_display("open write failed", filename, "");
        return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);

    f = fopen(filename, "r");
    if (f == NULL) {
        update_display("open read failed", filename, "");
        return;
    }

    char line[128];
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        update_display("read failed", filename, "");
        return;
    }
    fclose(f);

    char *pos = strchr(line, '\n');
    if (pos != NULL) {
        *pos = '\0';
    }

    ESP_LOGI(TAG, "Read from file: '%s'", line);
    update_display("mounted /spiflash", "example.txt OK", line);

    ESP_ERROR_CHECK(esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle));
    ESP_LOGI(TAG, "Done");
}
