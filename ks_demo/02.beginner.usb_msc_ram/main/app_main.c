/**
 * @file app_main.c
 * @brief CherryUSB MSC RAM 盘示例主程序
 *
 * 本示例演示如何使用 ESP32-S3 的 USB OTG 功能实现 USB MSC 设备，
 * 使用 RAM 作为存储介质，连接 PC 后会识别为可移动磁盘。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "usbd_core.h"
#include "usbh_core.h"
#include "usbx_msc.h"

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 USB MSC 设备（使用 RAM 作为存储介质）
 * 2. 进入主循环
 *
 * @note 数据存储在 RAM 中，断电后会丢失
 */
void app_main(void)
{
    printf("Hello world!\n");

    /* 初始化 USB MSC 设备（RAM 盘） */
    /* 参数：busid=0, USB 寄存器基地址=0x60080000 */
    msc_ram_init1(0, 0x60080000);

    /* 主循环 */
    while(1)
    {
        vTaskDelay(10);
    }
}
