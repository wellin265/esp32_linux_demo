/**
 * @file app_speech.h
 * @brief 音频编解码器（I2S + ES8311/ES7210）初始化接口
 *
 * 提供 I2S 双工通道创建、编解码器初始化和音频设备配置功能。
 * 硬件使用 ES8311（DAC 输出）+ ES7210（ADC 输入）编解码器。
 *
 * @copyright SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Unlicense OR CC0-1.0
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
#include "driver/gpio.h"
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 音频输入设备句柄（ES7210 ADC） */
extern esp_codec_dev_handle_t input_dev_;

/** @brief I2S 发送通道句柄（DAC 输出） */
extern i2s_chan_handle_t tx_handle_;

/** @brief I2S 接收通道句柄（ADC 输入） */
extern i2s_chan_handle_t rx_handle_;

/**
 * @brief 初始化音频编解码器 I2S 接口
 *
 * 配置 I2S 双工通道（TX: 16kHz STD 模式, RX: 48kHz TDM 模式），
 * 初始化 ES8311（DAC）和 ES7210（ADC）编解码器，
 * 使能功放 PA。
 */
void Codec_I2S_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SPEECH_H */
