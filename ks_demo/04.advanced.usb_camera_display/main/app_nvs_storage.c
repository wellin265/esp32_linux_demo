/**
 * @file app_nvs_storage.c
 * @brief NVS 非易失性存储封装实现
 *
 * 使用 "memory" 命名空间进行键值存储。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_nvs_storage.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "nvs_storage";

/** @brief NVS 命名空间名称 */
#define NVS_NAMESPACE "memory"

esp_err_t app_nvs_get_blob(const char *key, void *value, size_t *size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(handle, key, value, size);
    switch (err) {
    case ESP_OK:
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "键 '%s' 尚未初始化", key);
        break;
    default:
        ESP_LOGE(TAG, "读取 NVS 失败: %s", esp_err_to_name(err));
        break;
    }

    nvs_close(handle);
    return err;
}

esp_err_t app_nvs_set_blob(const char *key, const void *value, size_t size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_set_blob(handle, key, value, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "写入 NVS 失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 提交失败");
    }

    nvs_close(handle);
    return err;
}
