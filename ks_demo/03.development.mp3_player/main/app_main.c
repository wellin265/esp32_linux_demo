/**
 * @file app_main.c
 * @brief MP3 播放器主程序（不使用 ESP-ADF）
 *
 * 本示例演示如何使用 ESP32-S3 实现完整的 MP3 播放器功能：
 * - 使用软件 MP3 解码器（helix_mp3）
 * - 通过 LVGL v8 提供图形用户界面
 * - 支持播放/暂停、上一曲/下一曲、音量调节
 * - 从 SPIFFS 文件系统读取 MP3 文件
 * - 使用 ES8311 DAC 输出音频
 *
 * 特点：
 * - 不依赖 ESP-ADF，仅使用 ESP-IDF 标准组件
 * - 支持多种采样率自动切换
 * - 提供完整的 UI 控制界面
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

#include "esp_timer.h"
#include "ksdiy_lvgl_port.h"
#include "lvgl.h"
#include "app_speech.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "ui_audio.h"
#include "audio.h"

/** @brief 日志标签 */
#define TAG "KSDIY_TEST"

/**
 * @brief 显示 SPIFFS 目录中的所有文件
 *
 * 遍历指定目录并打印所有文件的信息，用于调试。
 *
 * @param path 目录路径
 */
static void SPIFFS_Directory(char *path)
{
    DIR *dir = opendir(path);
    assert(dir != NULL);

    while (true)
    {
        struct dirent *pe = readdir(dir);
        if (!pe)
            break;

        ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x",
                 pe->d_name, pe->d_ino, pe->d_type);
    }
    closedir(dir);
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 挂载 SPIFFS 文件系统
 * 3. 初始化 LVGL 显示界面
 * 4. 创建音频播放 UI
 * 5. 启动 MP3 播放器任务
 *
 * @note MP3 文件应放置在 SPIFFS 分区中
 */
void app_main(void)
{
    /* ===== 初始化 NVS ===== */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ===== 初始化 SPIFFS 文件系统 ===== */
    /* 用于存放 MP3 音频文件 */
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true
    };

    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    /* 显示 SPIFFS 文件列表（调试用） */
    SPIFFS_Directory("/spiffs/");

    /* ===== 初始化 LVGL 显示界面 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建音频播放 UI ===== */
    if (ksdiy_lvgl_lock(0))
    {
        /* 初始化音频控制界面 */
        ui_audio_start();
        ksdiy_lvgl_unlock();
    }

    /* ===== 启动 MP3 播放器 ===== */
    /* 从 SPIFFS 目录播放 MP3 文件 */
    ESP_ERROR_CHECK(mp3_player_start("/spiffs"));
}
