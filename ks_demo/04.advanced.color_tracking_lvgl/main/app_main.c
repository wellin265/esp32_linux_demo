/**
 * @file app_main.c
 * @brief 颜色追踪示例主程序
 *
 * 本示例演示如何使用 ESP32-S3 配合摄像头和 LCD 显示屏
 * 实现实时颜色追踪功能。使用 EasyTracer 算法识别并追踪
 * 指定颜色的物体（绿色、红色、蓝色）。
 *
 * 主要功能：
 * - 初始化 NVS 非易失性存储
 * - 初始化 LVGL 图形库
 * - 加载颜色追踪显示页面
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
 * 3. 创建颜色追踪显示界面
 */
void app_main(void)
{
    /* ===== 初始化 NVS ===== */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        /* NVS 分区已满或版本不兼容，擦除后重新初始化 */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ===== 初始化 LVGL 端口 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建颜色追踪显示界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        /* 加载摄像头颜色追踪页面 */
        page_cam_load();
        ksdiy_lvgl_unlock();
    }
}
