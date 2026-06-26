/**
 * @file ui_record.h
 * @brief 音频录制播放器 LVGL 界面接口
 *
 * 定义录制/播放工作模式枚举和 UI 初始化函数。
 *
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 * @license SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "lvgl.h"
#include <driver/i2s_std.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 工作模式枚举
 *
 * 定义音频录制播放器的三种工作状态。
 */
typedef enum {
    idle = 0,   /*!< 空闲状态：等待用户操作 */
    record,     /*!< 录制模式：从麦克风录制音频到内存 */
    play,       /*!< 播放模式：从内存播放录制的音频 */
} record_mode_t;

/** @brief 当前工作模式（全局共享） */
extern record_mode_t mode;

/**
 * @brief 设置当前工作模式
 * @param new_mode 新的工作模式
 */
void set_mode(record_mode_t new_mode);

/**
 * @brief 获取当前工作模式
 * @return 当前工作模式
 */
record_mode_t get_mode(void);

/**
 * @brief 创建音频录制播放界面
 */
void ui_record(void);

#ifdef __cplusplus
}
#endif
