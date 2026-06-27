/**
 * @file ksdiy_lvgl_port.h
 * @brief KSDIY LVGL 移植层头文件
 *
 * 提供 LVGL v9 在 ESP32-S3 上的移植接口函数声明。
 * 封装了 LCD 显示、触摸输入和 LVGL 初始化等功能。
 *
 * @note 本模块使用 esp_lvgl_port 组件简化 LVGL 移植
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
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 端口
 *
 * 完成以下初始化工作：
 * - I2C 总线初始化（用于触摸控制器）
 * - SPI 总线初始化（用于 LCD）
 * - ST7789 LCD 驱动安装
 * - CST816S 触摸控制器初始化
 * - LVGL 库和 esp_lvgl_port 初始化
 * - 显示设备和触摸设备注册
 */
void ksdiy_lvgl_port_init(void);

/**
 * @brief 获取 LVGL 互斥锁
 *
 * LVGL API 不是线程安全的，在调用任何 LVGL 函数前需要获取锁。
 *
 * @param timeout_ms 超时时间（毫秒），-1 表示无限等待
 * @return true 成功获取锁
 * @return false 获取锁超时
 */
bool ksdiy_lvgl_lock(int timeout_ms);

/**
 * @brief 释放 LVGL 互斥锁
 *
 * 在完成 LVGL 操作后必须释放锁，以允许其他任务访问 LVGL。
 */
void ksdiy_lvgl_unlock(void);

/**
 * @brief 获取 LVGL 显示设备指针
 *
 * @return lv_disp_t* 显示设备指针
 */
lv_disp_t *ksdiy_lvgl_get_disp(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* _KSDIY_LVGL_PORT_ */
