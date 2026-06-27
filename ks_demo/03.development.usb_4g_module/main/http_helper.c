/**
 * @file http_helper.c
 * @brief HTTP 辅助工具函数实现
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include "esp_log.h"
#include "http_helper.h"

/** @brief 日志标签 */
static const char *TAG = "http_helper";

/* ===== 公共 API 实现 ===== */

void http_helper_set_cors_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
}

esp_err_t http_helper_send_json_response(httpd_req_t *req, const char *json_str, size_t len)
{
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    http_helper_set_cors_headers(req);
    return httpd_resp_send(req, json_str, len);
}

esp_err_t http_helper_send_empty_ok(httpd_req_t *req)
{
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    http_helper_set_cors_headers(req);
    return httpd_resp_send(req, NULL, 0);
}

int http_helper_read_post_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    int remaining = req->content_len;
    if (remaining > (int)buf_size) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "请求体过大");
        return -1;
    }

    int total_read = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + total_read, MIN(remaining, (int)buf_size - total_read));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;  /* 超时重试 */
            }
            return -1;  /* 连接错误 */
        }
        remaining -= ret;
        total_read += ret;
    }

    ESP_LOGI(TAG, "收到 POST 数据: %.*s", total_read, buf);
    return total_read;
}

esp_err_t http_helper_set_content_type_from_ext(httpd_req_t *req, const char *filepath)
{
    const char *ext = strrchr(filepath, '.');
    if (!ext) {
        return httpd_resp_set_type(req, "text/plain");
    }

    if (strcasecmp(ext, ".html") == 0) {
        return httpd_resp_set_type(req, "text/html");
    } else if (strcasecmp(ext, ".js") == 0) {
        return httpd_resp_set_type(req, "application/javascript");
    } else if (strcasecmp(ext, ".css") == 0) {
        return httpd_resp_set_type(req, "text/css");
    } else if (strcasecmp(ext, ".png") == 0) {
        return httpd_resp_set_type(req, "image/png");
    } else if (strcasecmp(ext, ".ico") == 0) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (strcasecmp(ext, ".svg") == 0) {
        return httpd_resp_set_type(req, "text/xml");
    }

    return httpd_resp_set_type(req, "text/plain");
}

void str_delete_char(char *str, char target)
{
    int i, j;
    for (i = j = 0; str[i] != '\0'; i++) {
        if (str[i] != target) {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}
