/**
 * @file app_nvs_storage.h
 * @brief NVS 非易失性存储封装模块
 *
 * 提供 NVS 键值存储的读写接口，用于保存摄像头分辨率等配置参数。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef APP_NVS_STORAGE_H
#define APP_NVS_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从 NVS 读取二进制数据
 *
 * @param[in]  key    键名
 * @param[out] value  输出数据缓冲区
 * @param[in,out] size 输入：缓冲区大小；输出：实际读取大小
 * @return esp_err_t
 *   - ESP_OK: 读取成功
 *   - ESP_ERR_NVS_NOT_FOUND: 键不存在
 */
esp_err_t app_nvs_get_blob(const char *key, void *value, size_t *size);

/**
 * @brief 向 NVS 写入二进制数据
 *
 * @param[in] key   键名
 * @param[in] value 数据缓冲区
 * @param[in] size  数据大小（字节）
 * @return esp_err_t
 *   - ESP_OK: 写入成功
 */
esp_err_t app_nvs_set_blob(const char *key, const void *value, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* APP_NVS_STORAGE_H */
