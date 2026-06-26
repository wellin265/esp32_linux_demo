/**
 * @file http_auth.c
 * @brief HTTP Basic 认证模块实现
 *
 * 实现 HTTP Basic Authentication 的编码和验证逻辑。
 * 使用 Base64 编码用户名密码进行传输验证。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_tls_crypto.h"
#include "http_auth.h"

/** @brief 日志标签 */
static const char *TAG = "http_auth";

/** @brief HTTP 401 未授权状态码字符串 */
#define HTTPD_401 "401 UNAUTHORIZED"

/**
 * @brief 生成 Basic Auth 凭据字符串
 *
 * 将用户名和密码拼接后进行 Base64 编码，
 * 生成 "Basic <base64>" 格式的认证凭据。
 *
 * @param username  用户名
 * @param password  密码
 * @return 认证凭据字符串（需调用者释放），失败返回 NULL
 */
static char *generate_auth_credentials(const char *username, const char *password)
{
    /* 拼接 "username:password" */
    char *user_info = NULL;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "内存不足，无法创建用户信息");
        return NULL;
    }

    /* 计算 Base64 编码后的长度 */
    int out;
    size_t n = 0;
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 分配 "Basic " + base64 + '\0' 的空间 */
    char *digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out,
                                 (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

esp_err_t http_auth_basic_check(httpd_req_t *req, basic_auth_info_t *auth_info)
{
    /* 读取 Authorization 请求头 */
    size_t buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len <= 1) {
        ESP_LOGE(TAG, "未收到认证头");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"router\"");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    char *buf = calloc(1, buf_len);
    if (!buf) {
        ESP_LOGE(TAG, "内存不足，无法读取认证头");
        return ESP_ERR_NO_MEM;
    }

    if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) != ESP_OK) {
        ESP_LOGE(TAG, "读取认证头失败");
        free(buf);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "认证头 => Authorization: %s", buf);

    /* 生成期望的凭据并进行比对 */
    char *expected = generate_auth_credentials(auth_info->username, auth_info->password);
    if (!expected) {
        ESP_LOGE(TAG, "内存不足，无法生成认证凭据");
        free(buf);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret;
    if (strncmp(expected, buf, buf_len) != 0) {
        ESP_LOGE(TAG, "认证失败");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"router\"");
        httpd_resp_send(req, NULL, 0);
        ret = ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "认证成功！");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        ret = ESP_OK;
    }

    free(expected);
    free(buf);
    return ret;
}
