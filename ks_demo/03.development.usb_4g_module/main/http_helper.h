/**
 * @file http_helper.h
 * @brief HTTP 辅助工具函数
 *
 * 提供 HTTP 响应构建、JSON 解析请求体读取等
 * 通用工具函数，减少 Web 路由处理中的重复代码。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置 CORS（跨域资源共享）响应头
 *
 * 允许所有来源的跨域请求，用于前端开发调试。
 *
 * @param req HTTP 请求句柄
 */
void http_helper_set_cors_headers(httpd_req_t *req);

/**
 * @brief 发送 JSON 字符串响应
 *
 * 设置状态码 200、Content-Type 为 application/json，
 * 并发送 JSON 字符串。
 *
 * @param req       HTTP 请求句柄
 * @param json_str  JSON 字符串
 * @param len       字符串长度
 * @return ESP_OK 成功
 */
esp_err_t http_helper_send_json_response(httpd_req_t *req, const char *json_str, size_t len);

/**
 * @brief 发送空的 JSON 成功响应
 *
 * @param req HTTP 请求句柄
 * @return ESP_OK 成功
 */
esp_err_t http_helper_send_empty_ok(httpd_req_t *req);

/**
 * @brief 从 POST 请求中读取完整请求体
 *
 * 将请求体读入缓冲区，支持超时重试。
 *
 * @param req HTTP 请求句柄
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际读取字节数，负数表示失败
 */
int http_helper_read_post_body(httpd_req_t *req, char *buf, size_t buf_size);

/**
 * @brief 设置响应的 Content-Type（根据文件扩展名）
 *
 * 支持 .html, .js, .css, .png, .ico, .svg 等常见类型。
 *
 * @param req      HTTP 请求句柄
 * @param filepath 文件路径（含扩展名）
 * @return ESP_OK 成功
 */
esp_err_t http_helper_set_content_type_from_ext(httpd_req_t *req, const char *filepath);

/**
 * @brief 从字符串中删除指定字符
 *
 * 原地修改字符串，压缩删除后的空位。
 *
 * @param str    目标字符串
 * @param target 要删除的字符
 */
void str_delete_char(char *str, char target);

#ifdef __cplusplus
}
#endif
