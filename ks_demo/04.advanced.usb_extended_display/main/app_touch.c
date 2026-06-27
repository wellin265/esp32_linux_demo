/**
 * @file app_touch.c
 * @brief app_touch module
 *
 * USB Extended Display app_touch.c(app_touch.c.BaseName).
 *
 * @copyright Copyright (c) 2024 KSDIY
 * @license MIT
 */
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_log.h"
#include "app_usb.h"
#include "app_touch.h"
#include "usb_descriptors.h"
#include "ksdiy_display.h"

static const char *TAG = "app_touch";

static void app_touch_task(void *arg)
{
    uint16_t x[KSDIY_TOUCH_MAX_POINTS];
    uint16_t y[KSDIY_TOUCH_MAX_POINTS];
    uint16_t strength[KSDIY_TOUCH_MAX_POINTS];
    uint8_t track_id[KSDIY_TOUCH_MAX_POINTS];
    uint8_t touchpad_cnt = 0;
    bool send_press = false;

    while (1) {
        bool touchpad_pressed = false;
        if (ksdiy_display_touch_read(x, y, strength, track_id, &touchpad_cnt, KSDIY_TOUCH_MAX_POINTS) == ESP_OK) {
            touchpad_pressed = touchpad_cnt > 0;
        }
        hid_report_t report = {0};
        if (touchpad_pressed && touchpad_cnt > 0) {
            report.report_id = REPORT_ID_TOUCH;
            for (int i = 0; i < touchpad_cnt; i++) {
                report.touch_report.data[i].index = track_id[i];
                report.touch_report.data[i].press_down = 1;
                report.touch_report.data[i].x = x[i];
                report.touch_report.data[i].y = y[i];
                report.touch_report.data[i].width = strength[i];
                report.touch_report.data[i].height = strength[i];
            }
            report.touch_report.cnt = touchpad_cnt;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            send_press = true;
        } else if (send_press) {
            send_press = false;
            report.report_id = REPORT_ID_TOUCH;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t app_touch_init(void)
{
    ESP_RETURN_ON_ERROR(ksdiy_display_touch_init(), TAG, "touch init failed");
    xTaskCreate(app_touch_task, "app_touch_task", 4096, NULL, CONFIG_TOUCH_TASK_PRIORITY, NULL);
    return ESP_OK;
}
