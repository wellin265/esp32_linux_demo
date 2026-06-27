/**
 * @file app_main.c
 * @brief WiFi 下载 JPG 图片并显示示例主程序
 *
 * 本示例演示如何使用 ESP32-S3 实现以下功能：
 * - 连接 WiFi 网络
 * - 从 HTTP 服务器下载 JPG 图片
 * - 使用 ESP JPEG 解码库解码图片
 * - 使用 LVGL v8 在 LCD 上显示图片
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"
#include "ksdiy_lvgl_port.h"
#include "app_wifi.h"
#include "esp_jpeg_dec.h"
#include "lvgl.h"
#include "bili_http.h"
#include "esp_heap_caps.h"

/** @brief 日志标签 */
#define TAG "APP_MAIN"

/** @brief RGB565 图像缓冲区指针 - 定义在 bili_http.c */
extern uint8_t *img_rgb565;

/* face_buffer 和 Jpgsize 定义在 bili_http.c 中 */
extern char *face_buffer;
extern int Jpgsize;

/** @brief RGB565 数据大小 - 定义在 bili_http.c */
extern int Rgbsize;

/** @brief 解码后的图像宽度 - 定义在 bili_http.c */
extern int rgb_width;

/** @brief 解码后的图像高度 - 定义在 bili_http.c */
extern int rgb_height;

/** @brief LVGL 图像控件 */
lv_obj_t *img_jpg;

/**
 * @brief LVGL 图像描述符
 */
lv_img_dsc_t jpg_data = {
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .data_size = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 连接 WiFi 网络
 * 3. 分配图像缓冲区内存
 * 4. 下载 JPG 图片
 * 5. 初始化 LVGL 端口
 * 6. 解码 JPG 并显示
 */
void app_main(void)
{
    /* ===== 初始化 NVS ===== */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ===== 初始化 WiFi 并连接网络 ===== */
    wifi_init_sta();

    /* ===== 分配图像缓冲区（使用 PSRAM） ===== */
    /* RGB565 图像缓冲区：最大支持 800x480 分辨率 */
    img_rgb565 = heap_caps_malloc(800 * 480 * 2, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    /* JPEG 文件缓冲区：最大 500KB */
    face_buffer = heap_caps_malloc(500 * 1024, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    /* ===== 下载 JPG 图片 ===== */
    http_download_jpg("https://i2.hdslb.com/bfs/face/bce14f5e3af4bca480fc7de227986ba304507078.jpg");

    /* ===== 初始化 LVGL 端口 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建图像控件并显示 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        /* 创建图像控件 */
        img_jpg = lv_img_create(lv_scr_act());
        lv_obj_set_pos(img_jpg, 20, 20);

        if (Jpgsize > 0)
        {
            /* 解码 JPG 图片 - 使用 esp_new_jpeg 组件 API */
            jpeg_dec_handle_t jpeg_dec = NULL;
            jpeg_dec_config_t jpeg_cfg = DEFAULT_JPEG_DEC_CONFIG();
            jpeg_cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

            jpeg_error_t jret = jpeg_dec_open(&jpeg_cfg, &jpeg_dec);
            if (jret == JPEG_ERR_OK) {
                jpeg_dec_io_t io = {
                    .inbuf = (uint8_t *)face_buffer,
                    .inbuf_len = Jpgsize,
                };
                jpeg_dec_header_info_t out_info = {0};
                jret = jpeg_dec_parse_header(jpeg_dec, &io, &out_info);
                if (jret == JPEG_ERR_OK) {
                    rgb_width = out_info.width;
                    rgb_height = out_info.height;
                    io.outbuf = img_rgb565;
                    io.out_size = rgb_width * rgb_height * 2;
                    jret = jpeg_dec_process(jpeg_dec, &io);
                }
                jpeg_dec_close(jpeg_dec);
            }

            /* 设置图像描述符 */
            jpg_data.header.w = rgb_width;
            jpg_data.header.h = rgb_height;
            jpg_data.data_size = rgb_width * rgb_height * 2;
            jpg_data.data = (uint8_t *)img_rgb565;

            /* 显示图像 */
            lv_img_set_src(img_jpg, &jpg_data);
            lv_obj_align(img_jpg, LV_ALIGN_CENTER, 60, 0);
        }

        ksdiy_lvgl_unlock();
    }
}
