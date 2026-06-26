/**
 * @file ksdiy_lcd_port.h
 * @brief SPI LCD 显示屏驱动移植层头文件
 *
 * 本头文件定义 SPI LCD 驱动的函数接口和外部变量。
 * 适用于 ST7789 控制器的 LCD 显示屏。
 *
 * @author Kevincoooool
 * @date 2021-10-13
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
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief LCD 面板句柄
 *
 * 全局变量，用于其他模块直接操作 LCD 显示。
 * 可通过 esp_lcd_panel_draw_bitmap() 函数直接绘制图像。
 */
extern esp_lcd_panel_handle_t panel_handle;

/**
 * @brief 初始化 SPI LCD 显示屏
 *
 * 初始化 SPI 总线和 ST7789 LCD 控制器：
 * - 配置 SPI 引脚和时钟
 * - 安装 LCD Panel IO
 * - 初始化 ST7789 驱动
 * - 配置显示方向和颜色
 * - 清屏
 */
void ksdiy_lvgl_lcd_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* _KSDIY_LVGL_PORT_ */
