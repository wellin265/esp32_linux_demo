/**
 * @file app_main.c
 * @brief LVGL v9 示例程序入口
 *
 * 本示例演示如何在 ESP32-S3 上使用 LVGL v9 图形库。
 * 使用 esp_lvgl_port 组件简化 LVGL 的移植工作。
 *
 * 主要功能：
 * - 初始化 LVGL 端口（包含 LCD 和触摸驱动）
 * - 运行 LVGL 音乐播放器演示
 *
 * @copyright SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "ksdiy_lvgl_port.h"

/** @brief 日志标签 */
static const char *TAG = "example";

/**
 * @brief 应用程序主入口
 *
 * 执行流程：
 * 1. 初始化 LVGL 端口（包括 SPI LCD、I2C 触摸、LVGL 库）
 * 2. 获取 LVGL 互斥锁（确保线程安全）
 * 3. 启动 LVGL 音乐播放器演示
 * 4. 释放互斥锁
 */
void app_main(void)
{
    /* 初始化 LVGL 端口（LCD + 触摸 + LVGL） */
    ksdiy_lvgl_port_init();

    /* 获取 LVGL 互斥锁，-1 表示无限等待 */
    ksdiy_lvgl_lock(-1);

    /* 启动 LVGL 音乐播放器演示 */
    lv_demo_music();

    /* 释放 LVGL 互斥锁 */
    ksdiy_lvgl_unlock();
}
