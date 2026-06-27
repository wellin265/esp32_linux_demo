/**
 * @file app_main.c
 * @brief ADC 按键示例主程序（自定义实现版本）
 *
 * 本示例演示如何使用 ESP32-S3 的 ADC 功能读取多个按键，
 * 通过电压值区分不同按键，并在 LVGL 界面上显示按键事件。
 *
 * 主要功能：
 * - 初始化 NVS 非易失性存储
 * - 初始化 LVGL 图形库
 * - 初始化 ADC 并读取按键值
 * - 创建按键扫描任务
 * - 在 LCD 上显示按键事件
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
#include "bsp_adc.h"
#include "button.h"
#include "bsp_button.h"
#include "ksdiy_lvgl_port.h"
#include "lvgl.h"
#include "lv_demos.h"

/** @brief 测试标签控件 */
lv_obj_t *label_test;

/**
 * @brief 按键扫描任务
 *
 * 持续扫描 ADC 按键状态并处理按键事件。
 *
 * @param arg 任务参数（未使用）
 */
void button_task(void *arg)
{
    /* 初始化按键 */
    Button_Init();

    while (1)
    {
        /* 处理按键状态 */
        Button_Process();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief 初始化测试标签
 *
 * 创建 LVGL 标签控件并设置初始文本。
 */
void label_init(void)
{
    /* 创建标签控件 */
    label_test = lv_label_create(lv_scr_act());

    /* 设置标签位置和大小 */
    lv_obj_center(label_test);
    lv_obj_set_size(label_test, 280, 50);

    /* 启用文本着色功能 */
    lv_label_set_recolor(label_test, true);

    /* 设置初始文本 */
    lv_label_set_text(label_test, "Hello World!\nPlease press down the button...");
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 初始化 LVGL 端口
 * 3. 创建界面标签
 * 4. 创建按键扫描任务
 * 5. 初始化 ADC
 * 6. 进入主循环，更新界面显示
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

    /* ===== 创建界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        label_init();
        ksdiy_lvgl_unlock();
    }

    /* ===== 创建按键扫描任务 ===== */
    xTaskCreatePinnedToCore(&button_task, "button_task", 1024 * 3, NULL, 8, NULL, 0);

    /* ===== 初始化 ADC ===== */
    adc_init();
    printf("adc_value: %d\n", get_adc());

    /* ===== 主循环 ===== */
    while (1)
    {
        /* 检测按键事件并更新界面 */
        if (Button_Value != BT_NONE)
        {
            lv_label_set_text(label_test, Button_Tips[Button_Value]);
            Button_Value = BT_NONE;
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}
