/**
 * @file nvs_storage.h
 * @brief NVS 非易失性存储操作模块
 *
 * 封装 NVS 键值存储的读写操作，为 4G 路由器提供
 * WiFi 配置参数的持久化存储能力。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 向 NVS 写入字符串值
 *
 * @param key   键名（最大 15 字节）
 * @param value 值字符串
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t nvs_storage_set_str(const char *key, const char *value);

/**
 * @brief 从 NVS 读取字符串值
 *
 * @param key   键名
 * @param value 输出缓冲区
 * @param size  缓冲区大小（输入时为容量，输出时为实际长度）
 * @return ESP_OK 成功, ESP_ERR_NVS_NOT_FOUND 未找到, 其他失败
 */
esp_err_t nvs_storage_get_str(const char *key, char *value, size_t *size);

#ifdef __cplusplus
}
#endif
