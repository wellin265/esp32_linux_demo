/**
 * @file app_main.c
 * @brief AVI 视频播放器主程序（简化版）
 *
 * 从 SPIFFS 读取 AVI 视频文件并通过 LCD 播放。
 * 与 avi_player 的区别：音频输出使用直接 I2S 写入（i2s_channel_write），
 * 而非 esp_codec_dev 抽象层。播放结束后循环重放同一文件。
 *
 * 视频格式要求：
 *   - 视频编码：MJPEG
 *   - 音频编码：PCM S16LE
 *   - 分辨率：建议 280x240 或更小
 *
 * 模块划分：
 *   - app_jpeg_dec.c/h : JPEG 解码器
 *   - avi_player.h     : AVI 容器解析与播放控制
 *   - app_speech.h     : 音频编解码器初始化（Init_codec）
 *   - ksdiy_lcd_port.h : LCD 显示驱动
 *
 * @note 视频转换命令：
 *   ffmpeg -i input.avi -t 30 -vcodec mjpeg -vf scale=280:240 \
 *          -r 14 -q:v 60 -acodec pcm_s16le -ar 16000 output.avi
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include <stdio.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/i2s_std.h"

#include "ksdiy_lcd_port.h"
#include "app_jpeg_dec.h"
#include "avi_player.h"
#include "app_speech.h"

static const char *TAG = "avi_multi_player";

/* ========== 辅助函数 ========== */

/**
 * @brief 遍历并打印 SPIFFS 目录内容（调试用）
 */
static void list_spiffs_directory(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGW(TAG, "无法打开目录: %s", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s (type=%x)", entry->d_name, entry->d_type);
    }
    closedir(dir);
}

/* ========== AVI 播放器回调 ========== */

/**
 * @brief 视频帧回调 —— JPEG 解码并显示
 */
static void on_video_frame(frame_data_t *data, void *arg)
{
    uint8_t *img_rgb565 = NULL;
    int rgb_size = 0;

    /* 解码 JPEG 帧 */
    jpeg_error_t ret = esp_jpeg_decode_one_picture(
        data->data, data->data_bytes, &img_rgb565, &rgb_size);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGW(TAG, "JPEG 解码失败: %d", ret);
        return;
    }

    /* 刷新到 LCD（偏移 20 像素居中） */
    esp_lcd_panel_draw_bitmap(panel_handle,
                              20, 0, rgb_width + 20, rgb_height,
                              img_rgb565);
    free(img_rgb565);
}

/**
 * @brief 音频帧回调 —— 直接通过 I2S 写入
 *
 * 与 avi_player 不同，此处使用 i2s_channel_write 直接写入音频数据。
 */
static void on_audio_frame(frame_data_t *data, void *arg)
{
    size_t bytes_write = 0;
    i2s_channel_write(tx_handle_, data->data, data->data_bytes, &bytes_write, 100);
}

/**
 * @brief 音频时钟设置回调
 */
static void on_audio_set_clock(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
{
    ESP_LOGI(TAG, "音频时钟: rate=%" PRIu32 " bits=%" PRIu32 " ch=%" PRIu32,
             rate, bits_cfg, ch);
}

/**
 * @brief 播放结束回调 —— 循环重放同一文件
 */
static void on_avi_play_end(void *arg)
{
    ESP_LOGI(TAG, "播放结束，重新播放");
    avi_player_play_from_file("/spiffs/output.avi");
}

/* ========== 主函数 ========== */

/**
 * @brief 应用程序入口
 *
 * 初始化流程：
 * 1. NVS 非易失性存储
 * 2. SPIFFS 文件系统
 * 3. 音频编解码器（I2S 直接模式）
 * 4. LCD 显示屏
 * 5. AVI 播放器
 * 6. 开始播放视频
 */
void app_main(void)
{
    ESP_LOGI(TAG, "编译时间: %s %s", __DATE__, __TIME__);

    /* 1. 初始化 NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* 2. 挂载 SPIFFS 文件系统 */
    ESP_LOGI(TAG, "初始化 SPIFFS...");
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path              = "/spiffs",
        .partition_label        = "storage",
        .max_files              = 2,
        .format_if_mount_failed = true,
    };

    err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS 挂载失败: %s", esp_err_to_name(err));
        return;
    }
    list_spiffs_directory("/spiffs/");

    /* 3. 初始化音频编解码器（I2S 直接模式） */
    Init_codec();

    /* 4. 初始化 LCD 显示屏 */
    ksdiy_lvgl_lcd_init();

    /* 5. 配置并初始化 AVI 播放器 */
    avi_player_config_t avi_config = {
        .buffer_size     = 50 * 1024,
        .audio_cb        = on_audio_frame,
        .video_cb        = on_video_frame,
        .avi_play_end_cb = on_avi_play_end,
        .coreID          = 1,
    };
    avi_player_init(avi_config);

    /* 6. 开始播放视频 */
    avi_player_play_from_file("/spiffs/output.avi");
}
