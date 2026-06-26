/**
 * @file app_main.c
 * @brief LVGL v8.3 SD卡 JPEG 图片浏览器
 *
 * 本示例演示如何在 ESP32-S3 上使用 LVGL v8.3 显示存储在 SD 卡中的 JPEG 图片。
 * 使用乐鑫官方 JPEG 解码库，速度快。
 *
 * 功能：
 * - 刌载并挂载 SD 卡文件系统
 * - 扫描并列出所有 JPG 图片文件
 * - 使用乐鑫 JPEG 解码器解码图片
 * - 在 LCD 上显示解码后的图片
 *
 * @copyright SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "ksdiy_lvgl_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_jpeg_enc.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

/** @brief 日志标签 */
static const char *TAG = "example";

/**
 * @brief LVGL 图片描述符
 *
 * 用于存储解码后的图片数据，供 LVGL 图片控件显示。
 */
lv_img_dsc_t img_dsc = {
    .header.always_zero = 0,
    .header.w = 240,
    .header.h = 240,
    .data_size = 240 * 240 * 2,
    .header.cf = LV_IMG_CF_TRUE_COLOR,  /**< 真彩色格式 */
    .data = NULL,
};

/** @brief RGB565 图片数据缓冲区（分配在 PSRAM） */
uint8_t *img_rgb565 = NULL;

/** @brief JPEG 文件读取缓冲区（分配在 PSRAM） */
uint8_t *pbuffer = NULL;

/** @brief 解码后图片宽度 */
size_t rgb_width = 0;

/** @brief 解码后图片高度 */
size_t rgb_height = 0;

/** @brief JPEG 解码输出格式：RGB565 大端 */
static jpeg_pixel_format_t j_type = JPEG_PIXEL_FORMAT_RGB565_BE;

/** @brief JPEG 解码旋转角度 */
static jpeg_rotate_t j_rotation = JPEG_ROTATE_0D;

/**
 * @brief 解码单张 JPEG 图片
 *
 * 使用乐鑫 JPEG 解码库将 JPEG 数据解码为 RGB565 格式。
 *
 * @param input_buf 输入 JPEG 数据缓冲区
 * @param len 输入数据长度
 * @param output_buf 输出 RGB 数据缓冲区指针
 * @param out_len 输出数据长度
 * @return jpeg_error_t 解码结果
 */
jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len, uint8_t **output_buf, int *out_len)
{
    uint8_t *out_buf = NULL;
    jpeg_error_t ret = JPEG_ERR_OK;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    /* 创建 JPEG 解码器句柄 */
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_config_t config = {
        .output_type = j_type,
        .rotate = j_rotation,
    };
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK)
    {
        return ret;
    }

    /* 分配 IO 回调结构体 */
    jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL)
    {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    /* 分配输出信息结构体 */
    out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL)
    {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    /* 设置输入缓冲区和长度 */
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    /* 解析 JPEG 图片头信息 */
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK)
    {
        goto jpeg_dec_failed;
    }

    /* 获取图片尺寸 */
    rgb_width = out_info->width;
    rgb_height = out_info->height;
    ESP_LOGI(TAG, "img width:%d height:%d ", rgb_width, rgb_height);

    /* 计算输出缓冲区大小 */
    *out_len = out_info->width * out_info->height * 3;
    if (config.output_type == JPEG_PIXEL_FORMAT_RGB565_LE || config.output_type == JPEG_PIXEL_FORMAT_RGB565_BE || config.output_type == JPEG_PIXEL_FORMAT_CbYCrY)
    {
        *out_len = out_info->width * out_info->height * 2;
    }
    else if (config.output_type == JPEG_PIXEL_FORMAT_RGB888)
    {
        *out_len = out_info->width * out_info->height * 3;
    }
    else
    {
        ret = JPEG_ERR_INVALID_PARAM;
        goto jpeg_dec_failed;
    }

    /* 分配输出缓冲区（16字节对齐） */
    out_buf = jpeg_calloc_align(*out_len, 16);
    if (out_buf == NULL)
    {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    /* 设置输出缓冲区 */
    jpeg_io->outbuf = out_buf;
    *output_buf = out_buf;

    /* 开始解码 JPEG */
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK)
    {
        goto jpeg_dec_failed;
    }

    /* 解码器反初始化 */
jpeg_dec_failed:
    jpeg_dec_close(jpeg_dec);
    if (jpeg_io)
    {
        free(jpeg_io);
    }
    if (out_info)
    {
        free(out_info);
    }
    return ret;
}

/**
 * @brief 应用程序主入口
 *
 * 初始化 NVS、SD 卡、LVGL，然后从 SD 卡读取 JPEG 图片，
 * 解码后在 LCD 上显示。
 */
void app_main(void)
{
    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* 初始化 SD 卡 */
    sdmmc_card_t *card = NULL;
    const char *mount_point = "/sdcard";
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    const sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SD card mounted");

    /* 初始化 LVGL */
    ksdiy_lvgl_port_init();

    /* 读取 JPEG 文件并解码 */
    const char *jpeg_path = "/sdcard/test.jpg";
    FILE *f = fopen(jpeg_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", jpeg_path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    pbuffer = malloc(fsize);
    if (pbuffer == NULL) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return;
    }
    fread(pbuffer, 1, fsize, f);
    fclose(f);

    /* 解码 JPEG */
    uint8_t *output_buf = NULL;
    int out_len = 0;
    jpeg_error_t jpeg_ret = esp_jpeg_decode_one_picture(pbuffer, fsize, &output_buf, &out_len);
    if (jpeg_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %d", jpeg_ret);
        free(pbuffer);
        return;
    }

    /* 使用 canvas 显示解码后的图片 */
    lv_obj_t *canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(canvas, output_buf, 320, 240, LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(canvas);

    ESP_LOGI(TAG, "JPEG decoded and displayed successfully");
}

