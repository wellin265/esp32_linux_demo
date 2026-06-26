/**
 * @file app_speech.h
 * @brief 音频编解码器初始化模块头文件
 *
 * 本头文件定义了音频编解码器初始化相关的函数声明和外部变量。
 * 支持 ES8311 DAC 编解码器的配置和控制。
 *
 * @author Kevincoooool
 * @date 2021-05-25
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

#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 输出音频设备句柄
 *
 * 用于通过 esp_codec_dev 接口控制 ES8311 DAC 输出音频。
 */
extern esp_codec_dev_handle_t output_dev_;

/**
 * @brief I2S 发送通道句柄
 *
 * 用于直接通过 I2S 接口发送音频数据到 DAC。
 */
extern i2s_chan_handle_t tx_handle_;

/**
 * @brief I2S 接收通道句柄
 *
 * 用于通过 I2S 接口接收音频数据（本示例未使用）。
 */
extern i2s_chan_handle_t rx_handle_;

/**
 * @brief 初始化音频编解码器和 I2S
 *
 * 完整的音频系统初始化，包括：
 * - I2C 总线初始化
 * - I2S 通道创建
 * - ES8311 编解码器配置
 * - 功放使能
 */
void Codec_I2S_init(void);

/**
 * @brief 初始化音频编解码器
 *
 * 仅初始化 ES8311 编解码器，不包括 I2C 和 I2S 初始化。
 * 适用于已经完成 I2C/I2S 初始化的场景。
 */
void Init_codec(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SPEECH_H */
