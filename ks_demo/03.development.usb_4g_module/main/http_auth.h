/**
 * @file http_auth.h
 * @brief HTTP Basic 认证模块
 *
 * 提供 HTTP Basic Authentication 功能，
 * 用于 Web 路由管理界面的用户身份验证。
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
 * @brief Basic 认证信息结构体
 */
typedef struct {
    char *username;  /**< 用户名 */
    char *password;  /**< 密码 */
} basic_auth_info_t;

/**
 * @brief 对 HTTP 请求执行 Basic 认证
 *
 * 从请求头中读取 Authorization 字段，与预设的用户名密码进行比对。
 * 认证失败时会自动设置 401 响应头。
 *
 * @param req         HTTP 请求句柄
 * @param auth_info   认证信息（用户名和密码）
 * @return ESP_OK 认证成功, ESP_FAIL 认证失败
 */
esp_err_t http_auth_basic_check(httpd_req_t *req, basic_auth_info_t *auth_info);

#ifdef __cplusplus
}
#endif
