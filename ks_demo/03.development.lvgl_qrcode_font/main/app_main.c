/**
 * @file app_main.c
 * @brief LVGL v8.3 二维码和中文显示示例
 *
 * 本示例演示如何在 ESP32-S3 上使用 LVGL v8.3 显示：
 * - 自定义中文字体
 * - 二维码（QR Code）
 * - 基本控件（标签、滑块、弧形）
 * - 图片显示
 *
 * @copyright SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "ksdiy_lvgl_port.h"
#include "nvs_flash.h"
#include "nvs.h"

/** @brief 日志标签 */
static const char *TAG = "example";

/** @brief 声明外部中文字体 */
LV_FONT_DECLARE(chinese_font);

/**
 * @brief UI 任务 - 创建演示界面
 *
 * 创建以下 UI 元素：
 * 1. 英文标签（蓝色，带字间距和行间距）
 * 2. 中文标签（使用自定义中文字体）
 * 3. 滑块控件
 * 4. 弧形控件
 * 5. 二维码显示
 *
 * @param arg 未使用的参数
 */
void ui_task(void *arg)
{
    static lv_obj_t *default_src;
    default_src = lv_scr_act();

    /* 设置屏幕背景色为浅绿色 */
    lv_style_t style2;
    lv_style_set_bg_color(&style2, lv_palette_main(LV_PALETTE_LIGHT_GREEN));
    lv_obj_add_style(default_src, &style2, _LV_STYLE_STATE_CMP_SAME);

    /* ===== 创建英文标签 ===== */
    lv_obj_t *label = lv_label_create(default_src);
    lv_label_set_text(label, "hello_world\n11223344");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 2, 10);

    /* 设置标签样式：蓝色文字、透明度、字间距、行间距 */
    lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_opa(&style, LV_OPA_90);
    lv_style_set_text_letter_space(&style, 5);   /* 字间距 5 像素 */
    lv_style_set_text_line_space(&style, 20);    /* 行间距 20 像素 */
    lv_obj_add_style(label, &style, 0);

    /* ===== 创建中文标签 ===== */
    label = lv_label_create(default_src);
    lv_label_set_text(label, "酷世DIY开发板测试使用");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 2, 70);
    /* 应用自定义中文字体 */
    lv_obj_set_style_text_font(label, &chinese_font, 0);

    /* ===== 创建滑块控件 ===== */
    lv_obj_t *slid = lv_slider_create(default_src);
    lv_slider_set_range(slid, 0, 100);           /* 设置范围 0-100 */
    lv_slider_set_value(slid, 50, LV_ANIM_ON);   /* 设置初始值为 50，带动画 */
    lv_obj_align(slid, LV_ALIGN_BOTTOM_MID, 0, -50);

    /* ===== 创建弧形控件 ===== */
    lv_obj_t *arc = lv_arc_create(default_src);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 50);
    lv_obj_set_pos(arc, 20, 100);

    /* ===== 创建二维码 ===== */
    static char *str = "this is test qrcode!";
    /* 创建 100x100 像素的二维码，黑底白色 */
    lv_obj_t *qrcode = lv_qrcode_create(default_src, 100, lv_color_black(), lv_color_white());
    /* 更新二维码内容 */
    lv_qrcode_update(qrcode, str, strlen(str));
    lv_obj_set_pos(qrcode, 150, 100);

    /* 任务主循环 */
    while (1)
    {
        vTaskDelay((1000) / portTICK_PERIOD_MS);
    }
}

/** @brief 声明外部图片资源 */
LV_IMG_DECLARE(img_test);

/**
 * @brief 图片显示任务
 *
 * 演示 LVGL 图片控件的显示和旋转（代码已注释）。
 *
 * @param arg 未使用的参数
 */
void img_disp_task(void *arg)
{
    /* 创建图片控件并设置图片源 */
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_test);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    uint16_t pos = 0;
    while (1)
    {
        vTaskDelay((1000) / portTICK_PERIOD_MS);
        /* 图片旋转动画（已注释）
        lv_img_set_angle(img, 0);       // 0度
        vTaskDelay((1000) / portTICK_PERIOD_MS);
        lv_img_set_angle(img, 900);     // 90度
        vTaskDelay((1000) / portTICK_PERIOD_MS);
        lv_img_set_angle(img, 1800);    // 180度
        vTaskDelay((1000) / portTICK_PERIOD_MS);
        lv_img_set_angle(img, 2700);    // 270度
        vTaskDelay((1000) / portTICK_PERIOD_MS);
        */

        /* 图片移动动画（已注释）
        lv_obj_set_pos(img, pos, 40);
        vTaskDelay((1000) / portTICK_PERIOD_MS);
        pos += 50;
        if(pos > 450) pos = 0;
        */
    }
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 初始化 LVGL 端口
 * 3. 创建 UI 任务
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    /* ===== 初始化 NVS ===== */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* ===== 初始化 LVGL 端口 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建 UI 任务 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        xTaskCreate(ui_task, "ui_task", 4096 * 4, NULL, 3, NULL);
        ksdiy_lvgl_unlock();
    }
}
