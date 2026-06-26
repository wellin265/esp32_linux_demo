/**
 * @file app_jpeg_dec.c
 * @brief JPEG 解码器模块实现（旧版 API）
 *
 * 封装旧版 ESP-IDF JPEG 解码器 API，提供单帧解码功能。
 * 使用 DEFAULT_JPEG_DEC_CONFIG + 两参数 jpeg_dec_open。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_jpeg_dec.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"

static const char *TAG = "jpeg_dec";

/* 全局图像尺寸 */
size_t rgb_width  = 0;
size_t rgb_height = 0;

/** @brief 输出像素格式（RGB565 大端序） */
static jpeg_pixel_format_t s_j_type = JPEG_PIXEL_FORMAT_RGB565_BE;

/** @brief 旋转角度 */
static jpeg_rotate_t s_j_rotation = JPEG_ROTATE_0D;

jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len,
                                          uint8_t **output_buf, int *out_len)
{
    if (input_buf == NULL || output_buf == NULL || out_len == NULL) {
        ESP_LOGE(TAG, "无效参数");
        return JPEG_ERR_INVALID_PARAM;
    }

    uint8_t *out_buf = NULL;
    jpeg_error_t ret = JPEG_ERR_OK;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    rgb_width  = 0;
    rgb_height = 0;

    /* 生成默认配置 */
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = s_j_type;
    config.rotate = s_j_rotation;

    /* 创建 JPEG 解码器句柄 */
    jpeg_dec_handle_t jpeg_dec = NULL;
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_open 失败: %d", ret);
        return ret;
    }

    /* 创建 IO 回调句柄 */
    jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto cleanup;
    }

    /* 创建输出信息句柄 */
    out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto cleanup;
    }

    /* 设置输入缓冲区 */
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    /* 解析 JPEG 头部 */
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG 头部解析失败: %d", ret);
        goto cleanup;
    }

    /* 获取图像尺寸 */
    rgb_width  = out_info->width;
    rgb_height = out_info->height;

    /* 根据输出格式计算缓冲区大小 */
    if (config.output_type == JPEG_PIXEL_FORMAT_RGB565_LE ||
        config.output_type == JPEG_PIXEL_FORMAT_RGB565_BE ||
        config.output_type == JPEG_PIXEL_FORMAT_CbYCrY) {
        *out_len = out_info->width * out_info->height * 2;
    } else if (config.output_type == JPEG_PIXEL_FORMAT_RGB888) {
        *out_len = out_info->width * out_info->height * 3;
    } else {
        ret = JPEG_ERR_INVALID_PARAM;
        goto cleanup;
    }

    /* 分配输出缓冲区（16 字节对齐） */
    out_buf = jpeg_calloc_align(*out_len, 16);
    if (out_buf == NULL) {
        ESP_LOGE(TAG, "输出缓冲区分配失败: %d 字节", *out_len);
        ret = JPEG_ERR_NO_MEM;
        goto cleanup;
    }

    jpeg_io->outbuf = out_buf;
    *output_buf = out_buf;

    /* 执行 JPEG 解码 */
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG 解码失败: %d", ret);
        free(out_buf);
        *output_buf = NULL;
    }

cleanup:
    if (jpeg_dec) {
        jpeg_dec_close(jpeg_dec);
    }
    free(jpeg_io);
    free(out_info);
    return ret;
}
