/**
 * @file app_camera.h
 * @brief 摄像头硬件配置与初始化模块
 *
 * 配置 OV2640 摄像头引脚映射、时钟频率、输出格式等参数，
 * 并提供摄像头初始化接口。
 *
 * 硬件连接（KSDIY Korvo 开发板）：
 *   - XCLK  -> GPIO40 (20MHz)
 *   - SCCB  -> SDA=GPIO17, SCL=GPIO18
 *   - D0-D7 -> GPIO13,14,47,3,12,42,41,39
 *   - VSYNC -> GPIO21
 *   - HREF  -> GPIO38
 *   - PCLK  -> GPIO11
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef APP_CAMERA_H
#define APP_CAMERA_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化摄像头
 *
 * 配置 OV2640 摄像头：
 * - 20MHz 系统时钟
 * - VGA 分辨率 (640x480)
 * - JPEG 输出格式
 * - 2 个帧缓冲区（PSRAM）
 * - 连续采集模式
 *
 * @return esp_err_t
 *   - ESP_OK: 初始化成功
 *   - ESP_FAIL: 初始化失败
 */
esp_err_t app_camera_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAMERA_H */
