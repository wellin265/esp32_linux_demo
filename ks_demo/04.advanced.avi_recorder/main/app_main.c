/**
 * @file app_main.c
 * @brief AVI 视频录制器
 *
 * 使用 ESP32-S3 摄像头录制 AVI 视频文件到 TF 卡。
 * 目前仅支持视频录制（MJPEG），暂不支持音频。
 *
 * 硬件要求：
 *   - TF 卡（MicroSD）已插入
 *   - OV2640 摄像头模块
 *
 * 录制参数：
 *   - 分辨率：VGA (640x480)
 *   - 时长：30 秒
 *   - 帧率：10fps
 *   - 输出路径：/sdcard/record.avi
 *
 * 模块依赖：
 *   - app_camera.h  : 摄像头初始化
 *   - avi_recorder.h : AVI 录制控制
 *   - file_manager.h : TF 卡文件管理
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "avi_recorder.h"
#include "file_manager.h"
#include "app_camera.h"

/** @brief 日志标签 */
static const char *TAG = "avi_recorder_main";

/** @brief 录制分辨率 */
#define RECORD_RESOLUTION  FRAMESIZE_VGA

/** @brief 录制时长（10fps * 30秒 = 300帧） */
#define RECORD_FRAMES      (10 * 3)

/** @brief 录制帧率 (fps) */
#define RECORD_FPS         10

/** @brief 输出文件路径 */
#define RECORD_FILE_PATH   "/sdcard/record.avi"

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. NVS 非易失性存储
 * 2. TF 卡文件系统
 * 3. 摄像头
 * 4. 开始录制 AVI 视频
 *
 * @note 必须插入 TF 卡才能正常录制
 */
void app_main(void)
{
    ESP_LOGI(TAG, "AVI 录制器启动");

    /* 1. 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要擦除并重新初始化");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. 初始化 TF 卡 */
    fm_sdcard_init();

    /* 3. 初始化摄像头 */
    app_camera_init();

    /* 4. 开始录制 30 秒视频 */
    ESP_LOGI(TAG, "开始录制到 %s (VGA, %dfps, %d帧)",
             RECORD_FILE_PATH, RECORD_FPS, RECORD_FRAMES);
    avi_recorder_start(RECORD_FILE_PATH, RECORD_RESOLUTION,
                       RECORD_FRAMES, RECORD_FPS);
}
