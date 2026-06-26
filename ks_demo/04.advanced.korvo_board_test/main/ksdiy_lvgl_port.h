/**
 * @file ksdiy_lvgl_port.h
 * @brief KSDIY Korvo 开发板 LVGL 端口驱动头文件
 *
 * 提供 LVGL 初始化和线程安全的互斥锁接口。
 * 包含 SPI LCD、I2C 触摸屏和 IO 扩展器的初始化。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license CC0-1.0
 */


#ifndef _KSDIY_LVGL_PORT_
#define _KSDIY_LVGL_PORT_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <driver/i2c_master.h>

#ifdef __cplusplus
extern "C" {
#endif
extern i2c_master_bus_handle_t touch_i2c_bus_;

void ksdiy_lvgl_port_init(void);
bool ksdiy_lvgl_lock(int timeout_ms);
bool ksdiy_lvgl_is_ready(void);

void ksdiy_lvgl_unlock(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
