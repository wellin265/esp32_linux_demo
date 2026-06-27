/**
 * @file app_adc.h
 * @brief ADC 按键驱动模块头文件
 *
 * 提供 ADC 多路按键检测功能的初始化接口和全局状态变量。
 * 用于 KSDIY Korvo ESP32-S3 开发板的物理按键输入。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef _APP_ADC_H_
#define _APP_ADC_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#ifdef __cplusplus
extern "C"
{
#endif
extern char button_str[128];
extern uint8_t Button_Value;

void ADC_BUTTON_Init(void);
#ifdef __cplusplus
}
#endif
#endif