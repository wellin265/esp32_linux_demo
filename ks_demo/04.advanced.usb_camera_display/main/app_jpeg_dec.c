/**
 * @file app_jpeg_dec.c
 * @brief JPEG 解码器模块实现
 *
 * 封装 ESP-IDF JPEG 解码器 API，提供简洁的帧解码接口。
 * 使用 JPEG 硬件加速，输出 RGB565 大端序格式。
 *
 * 依赖组件：espressif/esp_new_jpeg "^0.5.0"
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_jpeg_dec.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"

static const char *TAG = "jpeg_dec";

esp_err_t app_jpeg_decode_frame(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    if (input_buf == NULL || output_buf == NULL || len <= 0) {
        ESP_LOGE(TAG, "无效参数: buf=%p, out=%p, len=%d", input_buf, output_buf, len);
        return ESP_ERR_INVALID_ARG;
    }

    jpeg_error_t ret = JPEG_ERR_OK;

    /* 配置 JPEG 解码器：RGB565 大端序，不旋转 */
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
    config.rotate = JPEG_ROTATE_0D;

    /* 创建解码器句柄（两参数版本） */
    jpeg_dec_handle_t jpeg_dec = NULL;
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "创建 JPEG 解码器失败: %d", ret);
        return ESP_FAIL;
    }

    /* 分配 IO 回调结构体 */
    jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        ESP_LOGE(TAG, "JPEG IO 内存分配失败");
        jpeg_dec_close(jpeg_dec);
        return ESP_ERR_NO_MEM;
    }

    /* 分配输出信息结构体 */
    jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        ESP_LOGE(TAG, "JPEG info 内存分配失败");
        free(jpeg_io);
        jpeg_dec_close(jpeg_dec);
        return ESP_ERR_NO_MEM;
    }

    /* 设置输入数据 */
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    /* 解析 JPEG 头部 */
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG 头部解析失败: %d", ret);
        goto cleanup;
    }

    /* 设置输出缓冲区，跳过已消费的输入数据 */
    jpeg_io->outbuf = output_buf;
    int consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    /* 执行 JPEG 解码 */
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG 解码失败: %d", ret);
    }

cleanup:
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return (ret == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL;
}
