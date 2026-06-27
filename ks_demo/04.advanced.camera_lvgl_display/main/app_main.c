/**
 * @file app_main.c
 * @brief LCD 摄像头显示示例主程序
 *
 * 本示例演示如何使用 ESP32-S3 配合摄像头模块和 LCD 显示屏
 * 实现实时摄像头图像显示。
 *
 * 主要功能：
 * - 初始化 NVS 非易失性存储
 * - 初始化 LVGL 图形库
 * - 加载摄像头显示页面
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"
#include "ksdiy_lvgl_port.h"
#include "app_camera.h"
#include "page_cam.h"
#include "lv_demos.h"

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 初始化 LVGL 端口
 * 3. 加载摄像头显示页面
 */
void app_main(void)
{
    /* ===== 初始化 NVS ===== */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ===== 初始化 LVGL 端口 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建摄像头显示界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        /* 加载摄像头显示页面 */
        page_cam_load();
        ksdiy_lvgl_unlock();
    }
}
