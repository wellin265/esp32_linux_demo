/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "sdkconfig.h"

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.hello_world", "chip info and restart");
    ksdiy_example_display_set_lines("reading chip info...", "", "");

    printf("Hello world!\n");

    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    esp_chip_info(&chip_info);

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ksdiy_example_display_set_lines("flash read failed", "", "");
        return;
    }

    printf("This is %s chip with %d CPU core(s)\n", CONFIG_IDF_TARGET, chip_info.cores);
    printf("silicon revision v%d.%d, %" PRIu32 "MB flash\n",
           major_rev, minor_rev, flash_size / (uint32_t)(1024 * 1024));
    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    char line1[64];
    char line2[64];
    char line3[64];
    snprintf(line1, sizeof(line1), "%s rev v%d.%d %d core",
             CONFIG_IDF_TARGET, major_rev, minor_rev, chip_info.cores);
    snprintf(line2, sizeof(line2), "flash=%" PRIu32 "MB heap=%" PRIu32,
             flash_size / (uint32_t)(1024 * 1024), esp_get_minimum_free_heap_size());

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        snprintf(line3, sizeof(line3), "restart in %d sec", i);
        ksdiy_example_display_set_lines(line1, line2, line3);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
