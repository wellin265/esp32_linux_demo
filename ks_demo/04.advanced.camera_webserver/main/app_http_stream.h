/**
 * @file app_http_stream.h
 * @brief HTTP MJPEG 视频流服务器模块
 *
 * 提供 HTTP MJPEG 流服务器功能，将摄像头捕获的图像
 * 实时通过 HTTP multipart/x-mixed-replace 协议推送到浏览器。
 *
 * 协议说明：
 *   客户端访问 /stream 端点，服务器持续推送 JPEG 帧数据，
 *   每帧以 boundary 分隔，浏览器自动渲染为实时视频流。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef APP_HTTP_STREAM_H
#define APP_HTTP_STREAM_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 HTTP MJPEG 流服务器
 *
 * 创建 HTTP 服务器并注册 /stream 端点的 MJPEG 流处理器。
 * 服务器运行在独立任务中，端口使用 HTTPD 默认端口 (80)。
 *
 * @return esp_err_t
 *   - ESP_OK: 服务器启动成功
 *   - ESP_FAIL: 服务器启动失败
 */
esp_err_t app_http_stream_start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HTTP_STREAM_H */
