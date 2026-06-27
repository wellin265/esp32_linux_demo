/**
 * @file app_main.c
 * @brief ESP32-S3 网页摄像头视频流服务器
 *
 * 本示例演示如何使用 ESP32-S3 开发板和摄像头模块实现网页实时视频流功能。
 * 使用 HTTP MJPEG 流技术，将摄像头捕获的图像实时传输到网页浏览器。
 *
 * 功能：
 * - 摄像头初始化和图像采集
 * - WiFi Station 连接
 * - HTTP MJPEG 流服务器（/stream 端点）
 * - 帧率统计和性能监控
 *
 * 模块划分：
 * - app_camera.c/h : 摄像头硬件配置与初始化
 * - app_wifi.c/h   : WiFi Station 连接管理
 * - app_http_stream.c/h : HTTP MJPEG 流服务器
 *
 * @note 本示例需要启用 PSRAM 才能正常运行
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>

#include "app_camera.h"
#include "app_wifi.h"
#include "app_http_stream.h"
#include "esp_camera.h"

static const char *TAG = "camera_webserver";

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 初始化摄像头（OV2640, VGA, JPEG）
 * 3. 验证摄像头是否正常工作
 * 4. 连接 WiFi（Station 模式）
 * 5. 启动 HTTP MJPEG 流服务器
 */
void app_main(void)
{
    /* 1. 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. 初始化摄像头 */
    ESP_ERROR_CHECK(app_camera_init());

    /* 3. 验证摄像头工作正常 */
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "摄像头采集失败，请检查硬件连接");
        return;
    }
    ESP_LOGI(TAG, "摄像头采集正常，首帧大小: %u bytes", fb->len);
    esp_camera_fb_return(fb);

    /* 4. 连接 WiFi */
    wifi_init_sta();

    /* 5. 启动 HTTP MJPEG 流服务器 */
    ESP_ERROR_CHECK(app_http_stream_start());

    ESP_LOGI(TAG, "系统初始化完成，请在浏览器访问 http://<IP>/stream");
}
