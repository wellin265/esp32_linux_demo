/**
 * @file app_main.c
 * @brief ADC 按键示例主程序（ESP 组件版本）
 *
 * 本示例演示如何使用 ESP32-S3 的 ADC 功能配合 ESP-IoT-Solution 的 button 组件
 * 读取多个按键，并在 LVGL 界面上显示按键事件。
 *
 * 主要功能：
 * - 初始化 NVS 非易失性存储
 * - 初始化 LVGL 图形库
 * - 使用 ESP button 组件初始化 ADC 按键
 * - 在 LCD 上显示按键事件
 *
 * 与 11.adc_button_lvgl_v8 的区别：
 * - 本示例使用 ESP 官方的 button 组件（iot_button）
 * - 支持更多按键事件类型
 * - 代码更简洁，无需手动扫描按键
 *
 * @author Kevincoooool
 * @date 2021-09-04
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_freertos_hooks.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "app_main.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_timer.h"
#endif
#include "ksdiy_lvgl_port.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "app_adc.h"

/** @brief 按键标签控件 */
lv_obj_t *label_btn;

/**
 * @brief 初始化测试标签
 *
 * 创建 LVGL 标签控件并设置初始文本。
 */
void label_init(void)
{
    /* 创建标签控件 */
    label_btn = lv_label_create(lv_scr_act());

    /* 设置标签位置和大小 */
    lv_obj_center(label_btn);
    lv_obj_set_size(label_btn, 280, 50);

    /* 启用文本着色功能 */
    lv_label_set_recolor(label_btn, true);

    /* 设置初始文本 */
    lv_label_set_text(label_btn, "Hello World!\nPlease press down the button...");
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 初始化 LVGL 端口
 * 3. 初始化 ADC 按键（使用 ESP button 组件）
 * 4. 创建界面标签
 * 5. 进入主循环，更新界面显示
 */
void app_main(void)
{
    /* ===== 初始化 NVS ===== */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ===== 初始化 LVGL 端口 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 初始化 ADC 按键 ===== */
    ADC_BUTTON_Init();

    /* ===== 创建界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        label_init();
        ksdiy_lvgl_unlock();
    }

    /* ===== 主循环 ===== */
    while (1)
    {
        /* 检测按键事件并更新界面 */
        if (Button_Value == 1)
        {
            lv_label_set_text(label_btn, button_str);
            Button_Value = 0;
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}
