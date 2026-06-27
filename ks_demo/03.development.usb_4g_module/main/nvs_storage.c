/**
 * @file nvs_storage.c
 * @brief NVS 非易失性存储操作模块实现
 *
 * 封装 NVS 分区 "memory" 的键值读写操作。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_storage.h"

/** @brief 日志标签 */
static const char *TAG = "nvs_storage";

/** @brief NVS 命名空间 */
#define NVS_NAMESPACE "memory"

esp_err_t nvs_storage_set_str(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "写入成功: %s", key);
    } else {
        ESP_LOGE(TAG, "写入失败: %s, 错误: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return (err == ESP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_storage_get_str(const char *key, char *value, size_t *size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_get_str(handle, key, value, size);
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "读取成功: %s = %s", key, value);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "键值不存在: %s", key);
        break;
    default:
        ESP_LOGE(TAG, "读取错误: %s", esp_err_to_name(err));
        break;
    }

    nvs_close(handle);
    return err;
}
