/**
 * @file modem_http_config.h
 * @brief 4G 路由器 Web 管理服务器接口
 *
 * 提供 HTTP 服务器初始化/反初始化、WiFi 配置读写等功能的公共接口。
 * STA 设备管理相关类型定义在 sta_manager.h 中。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "usbh_modem_wifi.h"
#include "sta_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Web 管理服务器
 *
 * 启动 HTTP 服务器，注册 REST API 路由，
 * 挂载 SPIFFS 文件系统，注册 WiFi 事件处理。
 *
 * @param wifi_config WiFi 配置结构体指针
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t modem_http_init(modem_wifi_config_t *wifi_config);

/**
 * @brief 反初始化 Web 管理服务器
 *
 * 停止 HTTP 服务器，释放资源。
 *
 * @param server HTTP 服务器句柄
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t modem_http_deinit(httpd_handle_t server);

/**
 * @brief 从 NVS 读取 WiFi 配置
 *
 * 从非易失性存储中读取之前保存的 SSID、密码、
 * 认证模式、信道、带宽等 WiFi 参数。
 *
 * @param wifi_config 输出 WiFi 配置结构体
 * @return ESP_OK 成功
 */
esp_err_t modem_http_get_nvs_wifi_config(modem_wifi_config_t *wifi_config);

#ifdef __cplusplus
}
#endif
