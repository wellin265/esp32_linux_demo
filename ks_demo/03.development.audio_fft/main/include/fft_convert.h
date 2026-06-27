/**
 * @file fft_convert.h
 * @brief FFT 频谱转换模块接口
 *
 * 提供环形缓冲区写入和 FFT 转换初始化功能。
 * 音频数据通过 rb_write() 写入环形缓冲区，
 * FFT 转换任务自动从缓冲区读取数据进行处理。
 *
 * @copyright SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

/** @brief FFT 采样点数（2 的幂次） */
#define N_SAMPLES 1024

/**
 * @brief 向环形缓冲区写入音频采样数据
 *
 * 将 16 位音频采样数据写入内部环形缓冲区。
 * FFT 转换任务会自动从缓冲区中读取数据进行处理。
 *
 * @param buf  指向 16 位音频采样数据的指针
 * @param size 数据长度（字节）
 */
void rb_write(int16_t *buf, size_t size);

/**
 * @brief 初始化 FFT 转换模块
 *
 * 创建环形缓冲区，初始化 ESP-DSP FFT 引擎，
 * 启动 FFT 转换任务（持续从缓冲区读取数据并计算频谱）。
 *
 * @return ESP_OK 成功, ESP_FAIL FFT 初始化失败
 */
esp_err_t fft_convert_init(void);

#ifdef __cplusplus
}
#endif
