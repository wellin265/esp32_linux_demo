/**
 * @file app_http_stream.c
 * @brief HTTP MJPEG 视频流服务器实现
 *
 * 实现 HTTP multipart/x-mixed-replace 协议的 MJPEG 流推送。
 * 循环从摄像头获取 JPEG 帧，通过 HTTP 分块传输发送给客户端。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_http_stream.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "http_stream";

/* ========== MJPEG 流协议常量 ========== */

#define PART_BOUNDARY "123456789000000000000987654321"

static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART_HEADER  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* ========== HTTP 请求处理 ========== */

/**
 * @brief MJPEG 流 GET 请求处理器
 *
 * 设置 multipart 响应类型后循环获取摄像头帧并推送。
 * 自动计算帧率并输出日志。
 */
static esp_err_t stream_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "客户端连接，可用堆内存: %u", esp_get_free_heap_size());

    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    /* 禁用 Nagle 算法以降低延迟 */
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    int64_t last_frame = esp_timer_get_time();
    char part_buf[64];

    while (true) {
        /* 获取摄像头帧 */
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "摄像头采集失败");
            res = ESP_FAIL;
            break;
        }

        size_t jpg_buf_len = 0;
        uint8_t *jpg_buf = NULL;
        bool need_free = false;

        /* 非 JPEG 格式需转换 */
        if (fb->format != PIXFORMAT_JPEG) {
            bool converted = frame2jpg(fb, 50, &jpg_buf, &jpg_buf_len);
            if (!converted) {
                ESP_LOGE(TAG, "JPEG 压缩失败");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
                break;
            }
            need_free = true;
        } else {
            jpg_buf_len = fb->len;
            jpg_buf = fb->buf;
        }

        /* 发送流边界 */
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res != ESP_OK) goto cleanup;

        /* 发送帧头（含长度） */
        int hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART_HEADER, jpg_buf_len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) goto cleanup;

        /* 发送 JPEG 数据 */
        res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);

    cleanup:
        if (need_free) {
            free(jpg_buf);
        }
        esp_camera_fb_return(fb);

        if (res != ESP_OK) {
            break;
        }

        /* 计算帧率 */
        int64_t now = esp_timer_get_time();
        int64_t frame_time_ms = (now - last_frame) / 1000;
        last_frame = now;

        ESP_LOGI(TAG, "MJPG: %uKB %llums (%.1ffps)",
                 (uint32_t)(jpg_buf_len / 1024),
                 (long long)frame_time_ms,
                 frame_time_ms > 0 ? 1000.0 / frame_time_ms : 0.0);
    }

    return res;
}

/* ========== URI 处理器注册 ========== */

static const httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL,
};

/* ========== 公共接口 ========== */

/**
 * @brief HTTP 服务器任务
 */
static void http_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "启动 HTTP 服务器...");

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "服务器启动成功，端口: %d", config.server_port);
        httpd_register_uri_handler(server, &stream_uri);
    } else {
        ESP_LOGE(TAG, "服务器启动失败！");
    }

    /* 服务器运行在后台守护线程中，本任务可退出 */
    vTaskDelete(NULL);
}

esp_err_t app_http_stream_start(void)
{
    BaseType_t ret = xTaskCreate(&http_server_task, "http_stream",
                                  8192, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 HTTP 服务器任务失败");
        return ESP_FAIL;
    }
    return ESP_OK;
}
