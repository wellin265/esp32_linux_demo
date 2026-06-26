/**
 * @file app_jpeg_dec.h
 * @brief JPEG 解码器模块（旧版 API 兼容）
 *
 * 提供 JPEG 图像解码功能，将 JPEG 数据解码为 RGB565 格式。
 * 使用旧版 ESP JPEG 解码 API（jpeg_error_t / 两参数 jpeg_dec_open）。
 *
 * @note 此模块使用旧版 API，分配输出缓冲区后由调用方负责释放。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef APP_JPEG_DEC_H
#define APP_JPEG_DEC_H

#include <stdint.h>
#include "esp_jpeg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 解码后图像宽度（全局，由解码函数设置） */
extern size_t rgb_width;

/** @brief 解码后图像高度（全局，由解码函数设置） */
extern size_t rgb_height;

/**
 * @brief 解码单帧 JPEG 图像
 *
 * 使用旧版 ESP JPEG 解码器将 JPEG 数据解码为指定像素格式。
 * 输出缓冲区由函数内部分配，调用方需负责释放。
 *
 * @param[in]  input_buf   输入 JPEG 数据
 * @param[in]  len          输入数据长度
 * @param[out] output_buf   输出 RGB 数据（函数内分配，需调用方 free）
 * @param[out] out_len      输出数据长度
 * @return jpeg_error_t
 *   - JPEG_ERR_OK: 解码成功
 *   - JPEG_ERR_NO_MEM: 内存不足
 *   - 其他: 解码失败
 */
jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len,
                                          uint8_t **output_buf, int *out_len);

#ifdef __cplusplus
}
#endif

#endif /* APP_JPEG_DEC_H */
