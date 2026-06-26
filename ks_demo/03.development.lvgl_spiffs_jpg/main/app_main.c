/**
 * @file app_main.c
 * @brief LVGL v8.3 SPIFFS JPEG 图片浏览器
 *
 * 本示例演示如何在 ESP32-S3 上使用 LVGL v8.3 显示存储在 SPIFFS 中的 JPEG 图片。
 * 使用乐鑫官方 JPEG 解码库，解码速度快。
 *
 * 功能：
 * - 挂载 SPIFFS 文件系统
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
    .header.cf = LV_IMG_CF_TRUE_COLOR,  /* 真彩色格式 */
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

    /* 生成默认配置 */
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = j_type;
    config.rotate = j_rotation;

    /* 创建 JPEG 解码器句柄 */
    jpeg_dec_handle_t jpeg_dec = NULL;
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
        *out_len = out_info->width * out_info->height * 2;  /* RGB565: 2 bytes/pixel */
    }
    else if (config.output_type == JPEG_PIXEL_FORMAT_RGB888)
    {
        *out_len = out_info->width * out_info->height * 3;  /* RGB888: 3 bytes/pixel */
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
    jpeg_io->outbuf = out_buf;
    *output_buf = out_buf;

    /* 执行 JPEG 解码 */
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK)
    {
        goto jpeg_dec_failed;
    }

/* 错误处理和资源释放 */
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
 * @brief 列表按钮点击事件回调
 *
 * 当用户点击文件列表中的 JPG 文件名时，读取、解码并显示该图片。
 *
 * @param event 事件对象指针
 */
static void btn_event_cb(lv_event_t *event)
{
    lv_obj_t *img = (lv_obj_t *)event->user_data;
    const char *file_name = lv_list_get_btn_text(lv_obj_get_parent(event->target), event->target);

    /* 分配带路径的文件名缓冲区 */
    char *file_name_with_path = (char *)heap_caps_malloc(48, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    uint32_t Jpgsize = 0;
    int Rgbsize = 0;

    if (NULL != file_name_with_path)
    {
        /* 构建完整文件路径 */
        strcpy(file_name_with_path, "/spiffs/");
        strcat(file_name_with_path, file_name);

        /* 打开 JPG 文件 */
        FILE *jpg_file = fopen(file_name_with_path, "rb");
        if (jpg_file == NULL)
        {
            ESP_LOGE(TAG, "Cannot open %s", file_name_with_path);
            return;
        }

        /* 获取文件大小 */
        fseek(jpg_file, 0, SEEK_END);
        Jpgsize = ftell(jpg_file);
        rewind(jpg_file);
        ESP_LOGI("GetFileSize", "Lsize %d", Jpgsize);

        /* 读取整个文件到缓冲区 */
        fread(pbuffer, Jpgsize, 1, jpg_file);

        /* 使用乐鑫 JPEG 解码器解码（速度快） */
        esp_jpeg_decode_one_picture(pbuffer, Jpgsize, &img_rgb565, &Rgbsize);

        /* 更新图片描述符 */
        img_dsc.header.w = rgb_width;
        img_dsc.header.h = rgb_height;
        img_dsc.data_size = rgb_width * rgb_height * 2;
        img_dsc.data = (uint8_t *)img_rgb565;

        /* 设置图片控件源 */
        lv_img_set_src(img, &img_dsc);

        /* 居中对齐图片 */
        lv_obj_align(img, LV_ALIGN_CENTER, 60, 0);

        ESP_LOGI(TAG, "Display image file : %s", file_name_with_path);

        /* 释放资源 */
        free(file_name_with_path);
        fclose(jpg_file);
    }
}

/**
 * @brief 创建图片浏览器界面
 *
 * 创建左侧文件列表和右侧图片显示区域。
 * 扫描 SPIFFS 中的所有 JPG 文件并添加到列表。
 */
static void image_display(void)
{
    /* 创建文件列表控件 */
    lv_obj_t *list = lv_list_create(lv_scr_act());
    lv_obj_set_size(list, 100, 240);
    lv_obj_set_style_border_width(list, 0, LV_STATE_DEFAULT);
    lv_obj_align(list, LV_ALIGN_LEFT_MID, 0, 0);

    /* 创建图片显示控件 */
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 100, 0);

    /* 打开 SPIFFS 目录 */
    struct dirent *p_dirent = NULL;
    DIR *p_dir_stream = opendir("/spiffs");

    /* 扫描目录中的 JPG 文件 */
    while (true)
    {
        p_dirent = readdir(p_dir_stream);
        if (NULL != p_dirent)
        {
            if (p_dirent->d_type == DT_REG)  /* 普通文件 */
            {
                /* 检查是否为 JPG 文件 */
                if (strstr(p_dirent->d_name, ".jpg") || strstr(p_dirent->d_name, ".JPG"))
                {
                    /* 添加到列表 */
                    lv_obj_t *btn = lv_list_add_btn(list, NULL, p_dirent->d_name);
                    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)img);
                }
            }
        }
        else
        {
            closedir(p_dir_stream);
            break;
        }
    }
}

/**
 * @brief 打印 SPIFFS 目录中的所有文件信息
 *
 * @param path 要扫描的目录路径
 */
static void SPIFFS_Directory(char *path)
{
    DIR *dir = opendir(path);
    assert(dir != NULL);
    while (true)
    {
        struct dirent *pe = readdir(dir);
        if (!pe)
            break;
        ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x", pe->d_name, pe->d_ino, pe->d_type);
    }
    closedir(dir);
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS
 * 2. 挂载 SPIFFS 文件系统
 * 3. 分配图片缓冲区（PSRAM）
 * 4. 初始化 LVGL
 * 5. 创建图片浏览器界面
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    /* ===== 初始化 NVS ===== */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* ===== 初始化 SPIFFS 文件系统 ===== */
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    /* 打印 SPIFFS 文件列表 */
    SPIFFS_Directory("/spiffs/");

    /* ===== 分配图片缓冲区（使用 PSRAM） ===== */
    img_rgb565 = heap_caps_malloc(800 * 480 * 2, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    pbuffer = heap_caps_malloc(500 * 1024, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    /* ===== 初始化 LVGL ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建图片浏览器界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        image_display();
        ksdiy_lvgl_unlock();
    }
}
