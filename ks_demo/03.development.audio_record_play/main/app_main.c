/**
 * @file app_main.c
 * @brief 音频录制与播放示例主程序
 *
 * 本示例演示如何使用 ESP32-S3 实现音频录制和播放功能：
 * - 使用 I2S 接口从 ES7210 ADC 读取音频数据（录制）
 * - 使用 I2S 接口向 ES8311 DAC 写入音频数据（播放）
 * - 通过 LVGL v8 提供图形用户界面
 * - 音频数据存储在 PSRAM 中（不保存到文件）
 *
 * 功能模式：
 * - 空闲模式（idle）：等待用户操作
 * - 录制模式（record）：从麦克风录制音频到内存
 * - 播放模式（play）：从内存播放录制的音频
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
#include "ui_record.h"

/** @brief 日志标签 */
#define TAG "KSDIY_TEST"

/* ===== 音频缓冲区配置 ===== */

/** @brief 音频缓冲区总大小（字节） */
#define BUF_SIZE (100 * 1024)  /* 100KB，可录制约 3 秒 16kHz 16位立体声 */

/** @brief 每次读写的帧大小（字节） */
#define FRAME_SIZE 1024

/** @brief 音频数据缓冲区指针（存储在 PSRAM） */
uint8_t *audio_buffer = NULL;

/** @brief 当前音频数据索引（录制/播放位置） */
int audio_index = 0;

/** @brief 录制的音频总长度（字节） */
int audio_total = 0;

/* ===== 模式控制（定义在 ui_record.h 中） ===== */

/** @brief 当前工作模式 */
record_mode_t mode = idle;

/* ===== 外部函数声明 ===== */

/**
 * @brief 设置当前工作模式
 *
 * @param new_mode 新的工作模式
 */
void set_mode(record_mode_t new_mode)
{
    mode = new_mode;
}

/**
 * @brief 获取当前工作模式
 *
 * @return record_mode_t 当前工作模式
 */
record_mode_t get_mode(void)
{
    return mode;
}

/**
 * @brief 音频录制和播放任务
 *
 * 主要功能：
 * 1. 录制模式：从 I2S RX 通道读取音频数据到内存
 * 2. 播放模式：从内存读取音频数据写入 I2S TX 通道
 * 3. 空闲模式：等待模式切换
 *
 * @param arg 任务参数（未使用）
 */
static void audio_record_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;
    static size_t bytes_write = 0;

    /* 初始化音频编解码器（I2S + ES8311 + ES7210） */
    Codec_I2S_init();

    /* 主循环：根据模式执行不同操作 */
    while (1)
    {
        if (mode == record)
        {
            /* ===== 录制模式 ===== */
            ESP_LOGI(TAG, "record start");
            audio_index = 0;

            /* 循环读取音频数据直到缓冲区满或模式改变 */
            while ((mode == record) && (audio_index < (BUF_SIZE - FRAME_SIZE)))
            {
                /* 从 I2S RX 通道读取音频数据（麦克风输入） */
                ret = i2s_channel_read(rx_handle_, audio_buffer + audio_index, FRAME_SIZE, &bytes_read, 100);

                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "[echo] i2s read failed");
                    abort();
                }
                audio_index += FRAME_SIZE;
            }

            /* 录制结束 */
            if (mode == record)
                mode = idle;
            ESP_LOGI(TAG, "record end");
        }
        else if (mode == play)
        {
            /* ===== 播放模式 ===== */
            ESP_LOGI(TAG, "play start");
            audio_total = audio_index;  /* 保存录制长度 */
            audio_index = 0;

            /* 循环写入音频数据直到播放完毕或模式改变 */
            while ((mode == play) && (audio_index < audio_total))
            {
                /* 向 I2S TX 通道写入音频数据（扬声器输出） */
                ret = i2s_channel_write(tx_handle_, audio_buffer + audio_index, FRAME_SIZE, &bytes_write, 100);

                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "[echo] i2s write failed");
                    abort();
                }
                audio_index += FRAME_SIZE;
            }

            /* 播放结束 */
            if (mode == play)
                mode = idle;
            ESP_LOGI(TAG, "play end");
        }
        else
        {
            /* ===== 空闲模式 ===== */
            vTaskDelay((100) / portTICK_PERIOD_MS);
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 分配音频缓冲区（PSRAM）
 * 3. 初始化 LVGL 显示界面
 * 4. 创建音频录制/播放任务
 *
 * @note 本示例将音频存储在内存中，断电后数据会丢失
 *       如需持久化存储，请参考 19.audio_record_sdcard 示例
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

    /* ===== 分配音频缓冲区（使用 PSRAM） ===== */
    /* 缓冲区大小：100KB，可录制约 3 秒 16kHz 16位立体声音频 */
    audio_buffer = heap_caps_malloc(BUF_SIZE, MALLOC_CAP_SPIRAM);

    /* ===== 初始化 LVGL 显示界面 ===== */
    ksdiy_lvgl_port_init();

    /* ===== 创建音频录制界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        ui_record();
        ksdiy_lvgl_unlock();
    }

    /* ===== 创建音频录制/播放任务 ===== */
    /* 任务栈大小：20KB，优先级：5 */
    xTaskCreate(audio_record_task, "audio_record_task", 4096 * 5, NULL, 5, NULL);
}
