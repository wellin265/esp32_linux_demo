/**
 * @file bili_http.c
 * @brief HTTP 下载 JPG 图片模块
 *
 * 本文件实现通过 HTTP GET 请求从网络下载 JPG 图片数据。
 * 支持：
 * - 断点续传（未启用）
 * - 超时设置
 * - 进度显示
 *
 * @author Kevincoooool
 * @date 2021-10-13
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_jpeg_dec.h"
#include "esp_heap_caps.h"
#include "lvgl.h"

/** @brief 日志标签 */
#define TAG "HTTP_JPG"

/** @brief HTTP 输出缓冲区指针 */
char *face_buffer = NULL;

/** @brief 已下载的数据长度 */
int Jpgsize = 0;

/** @brief RGB565 图像数据指针 */
uint8_t *img_rgb565 = NULL;

/** @brief RGB565 数据大小 */
int Rgbsize = 0;

/** @brief 解码后的图像宽度 */
int rgb_width = 0;

/** @brief 解码后的图像高度 */
int rgb_height = 0;

/**
 * @brief HTTP 事件处理函数
 *
 * 处理 HTTP 请求过程中的各种事件：
 * - HTTP_EVENT_ON_CONNECTED: 连接建立
 * - HTTP_EVENT_HEADERS_SENT: 请求头发送完成
 * - HTTP_EVENT_ON_HEADER: 收到响应头
 * - HTTP_EVENT_ON_DATA: 收到数据
 * - HTTP_EVENT_ON_FINISH: 请求完成
 * - HTTP_EVENT_DISCONNECTED: 连接断开
 * - HTTP_EVENT_ERROR: 发生错误
 *
 * @param evt HTTP 事件数据
 * @return esp_err_t 处理结果
 */
esp_err_t face_http_event_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;

    case HTTP_EVENT_HEADERS_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADERS_SENT");
        break;

    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;

    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /* 如果不是分块传输编码，直接将数据追加到缓冲区 */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            /* 将接收到的数据拷贝到全局缓冲区 */
            memcpy(face_buffer + Jpgsize, evt->data, evt->data_len);
            Jpgsize += evt->data_len;
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;

    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

/**
 * @brief 下载 JPG 图片
 *
 * 从指定的 URL 下载 JPG 图片数据并存储到全局缓冲区。
 *
 * @param url_jpg JPG 图片的 URL 地址
 *
 * @note 下载前需要确保 face_buffer 已经分配了足够的内存
 */
void http_download_jpg(char *url_jpg)
{
    /* 分配输出缓冲区（在 PSRAM 中分配） */
    face_buffer = (char *)heap_caps_malloc(sizeof(char) * 500 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (face_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
        return;
    }

    /* 配置 HTTP 客户端 */
    esp_http_client_config_t config = {
        .url = url_jpg,
        .event_handler = face_http_event_handler,
        .buffer_size = 4 * 1024,   /* 接收缓冲区大小：4KB */
        .timeout_ms = 4000,         /* 超时时间：4秒 */
    };

    ESP_LOGI(TAG, "Downloading from: %s", url_jpg);

    /* 初始化 HTTP 客户端 */
    esp_http_client_handle_t client = esp_http_client_init(&config);

    /* 设置请求头 */
    esp_http_client_set_header(client, "Accept", "*/*");

    /* 执行 HTTP GET 请求 */
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        ESP_LOGI(TAG, "Downloaded %d bytes", Jpgsize);
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    /* 清理 HTTP 客户端 */
    esp_http_client_cleanup(client);
}

/**
 * @brief 下载并解码 JPG 图片
 *
 * 从指定 URL 下载 JPG 图片，解码为 RGB565 格式，并显示在 LVGL 界面上。
 *
 * @param url_jpg JPG 图片的 URL 地址
 * @param img_obj LVGL 图像控件对象
 *
 * @note 此函数会自动分配和释放内存
 */
void http_download_and_display_jpg(char *url_jpg, lv_obj_t *img_obj)
{
    /* 分配缓冲区 */
    face_buffer = (char *)heap_caps_malloc(500 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    img_rgb565 = (uint8_t *)heap_caps_malloc(800 * 480 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (face_buffer == NULL || img_rgb565 == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        if (face_buffer) free(face_buffer);
        if (img_rgb565) free(img_rgb565);
        return;
    }

    /* 下载图片 */
    http_download_jpg(url_jpg);

    if (Jpgsize > 0)
    {
        /* 使用 esp_new_jpeg 组件 API 解码 JPG 图片 */
        jpeg_dec_handle_t jpeg_dec = NULL;
        jpeg_dec_config_t jpeg_cfg = DEFAULT_JPEG_DEC_CONFIG();
        jpeg_cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

        jpeg_error_t ret = jpeg_dec_open(&jpeg_cfg, &jpeg_dec);
        if (ret != JPEG_ERR_OK) {
            ESP_LOGE(TAG, "jpeg_dec_open failed: %d", ret);
            free(face_buffer);
            face_buffer = NULL;
            return;
        }

        /* 获取输出缓冲区大小 */
        jpeg_dec_io_t io = {
            .inbuf = (uint8_t *)face_buffer,
            .inbuf_len = Jpgsize,
        };
        jpeg_dec_header_info_t out_info = {0};
        ret = jpeg_dec_parse_header(jpeg_dec, &io, &out_info);
        if (ret != JPEG_ERR_OK) {
            ESP_LOGE(TAG, "jpeg_dec_parse_header failed: %d", ret);
            jpeg_dec_close(jpeg_dec);
            free(face_buffer);
            face_buffer = NULL;
            return;
        }

        rgb_width = out_info.width;
        rgb_height = out_info.height;
        Rgbsize = rgb_width * rgb_height * 2;
        ESP_LOGI(TAG, "JPEG header: %dx%d", rgb_width, rgb_height);

        /* 分配对齐的输出缓冲区 */
        uint8_t *out_buf = heap_caps_calloc(1, Rgbsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (out_buf == NULL) {
            ESP_LOGE(TAG, "Failed to alloc output buffer");
            jpeg_dec_close(jpeg_dec);
            free(face_buffer);
            face_buffer = NULL;
            return;
        }

        io.outbuf = out_buf;
        io.out_size = Rgbsize;

        ret = jpeg_dec_process(jpeg_dec, &io);
        if (ret == JPEG_ERR_OK)
        {
            /* 拷贝到 img_rgb565 */
            if (Rgbsize <= 800 * 480 * 2) {
                memcpy(img_rgb565, out_buf, Rgbsize);
            }

            ESP_LOGI(TAG, "JPEG decoded: %dx%d, size=%d", rgb_width, rgb_height, Rgbsize);

            /* 更新 LVGL 图像显示 */
            if (img_obj != NULL)
            {
                lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)lv_mem_alloc(sizeof(lv_img_dsc_t));
                if (img_dsc)
                {
                    img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
                    img_dsc->header.always_zero = 0;
                    img_dsc->header.w = rgb_width;
                    img_dsc->header.h = rgb_height;
                    img_dsc->data_size = Rgbsize;
                    img_dsc->data = img_rgb565;

                    lv_img_set_src(img_obj, img_dsc);
                    lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 0);
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "jpeg_dec_process failed: %d", ret);
        }

        free(out_buf);
        jpeg_dec_close(jpeg_dec);
    }
    else
    {
        ESP_LOGE(TAG, "Download failed or empty file");
    }
}
