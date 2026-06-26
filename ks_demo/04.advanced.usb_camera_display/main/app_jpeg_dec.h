/**
 * @file app_jpeg_dec.h
 * @brief JPEG 解码器模块
 *
 * 提供 JPEG 图像解码功能，将 JPEG 数据解码为 RGB565 格式。
 * 用于 USB 摄像头 MJPEG 视频流的帧解码。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef APP_JPEG_DEC_H
#define APP_JPEG_DEC_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 解码单帧 JPEG 图像为 RGB565 格式
 *
 * 使用 ESP JPEG 硬件加速解码器将 JPEG 数据解码为 RGB565 大端序格式。
 * 输出缓冲区由调用方提供，需确保足够大（width * height * 2 字节）。
 *
 * @param[in]  input_buf   输入的 JPEG 数据缓冲区
 * @param[in]  len          输入数据长度（字节）
 * @param[out] output_buf   输出的 RGB565 数据缓冲区（需预分配）
 * @return esp_err_t
 *   - ESP_OK: 解码成功
 *   - ESP_FAIL: 解码失败（数据损坏或内存不足）
 */
esp_err_t app_jpeg_decode_frame(uint8_t *input_buf, int len, uint8_t *output_buf);

#ifdef __cplusplus
}
#endif

#endif /* APP_JPEG_DEC_H */
