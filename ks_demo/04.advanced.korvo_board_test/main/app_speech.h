/**
 * @file app_speech.h
 * @brief 语音识别唤醒模块头文件
 *
 * 提供语音唤醒（WakeNet）初始化接口。
 * 基于 ESP-SR 框架，支持 WakeNet 唤醒词和 MultiNet 命令词。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef APP_SPEECH_H
#define APP_SPEECH_H
#include "esp_log.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <model_path.h>
#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>

#include <driver/i2s_std.h>
#include <driver/gpio.h>
#ifdef __cplusplus
extern "C"
{
#endif

    void Wakenet_init();//初始化摄像头
#ifdef __cplusplus
}
#endif
#endif