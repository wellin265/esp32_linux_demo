/**
 * @file app_main.c
 * @brief 音频录制与播放示例主程序（SD 卡存储版）
 *
 * 本示例演示如何使用 ESP32-S3 实现音频录制和播放功能，并将音频保存到 SD 卡：
 * - 使用 I2S 接口从 ES7210 ADC 读取音频数据（录制）
 * - 使用 I2S 接口向 ES8311 DAC 写入音频数据（播放）
 * - 将录制的音频保存为 WAV 文件到 SD 卡
 * - 从 SD 卡加载 WAV 文件进行播放
 * - 通过 LVGL v8 提供图形用户界面
 *
 * 与内存版的区别：
 * - 本版本将音频持久化存储到 SD 卡
 * - 支持录制更长时间的音频
 * - 录制的文件可以在其他设备上播放
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
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "ui_record.h"
#include "app_speech.h"
#include "wav_handler.h"
#include "fs_manager.h"

/** @brief 日志标签 */
#define TAG "KSDIY_TEST"

/** @brief 音频采样率 */
#define AUDIO_SAMPLE_RATE AUDIO_HAL_16K_SAMPLES

/* ===== 音频缓冲区配置 ===== */

/**
 * @brief 音频缓冲区总大小（字节）
 *
 * 计算公式：采样率 * 录制时长 * sizeof(uint8_t)
 * 16kHz, 16-bit, Mono: 录制时长 = BUF_SIZE / 16000 / 2 秒
 * 当前配置：16000 * 20 = 320000 字节，可录制约 10 秒单声道 16 位音频
 */
#define BUF_SIZE (16000 * 20 * sizeof(uint8_t))

/** @brief 每次读写的帧大小（字节） */
#define FRAME_SIZE (1600)

/** @brief 录制文件路径前缀 */
#define RECORD_FILE_PREFIX "/sdcard/record_"

/** @brief 文件序号（自动递增） */
static uint32_t file_index = 0;

/** @brief 当前音频数据索引（录制/播放位置） */
uint32_t audio_index = 0;

/** @brief 录制的音频总长度（字节） */
uint32_t audio_total = 0;

/** @brief 音频数据缓冲区指针（存储在 PSRAM） */
uint8_t *audio_buffer = NULL;

/* ===== 模式控制（使用 ui_record.h 中的 record_mode_t） ===== */

/** @brief 当前工作模式 */
record_mode_t mode = idle;

/* ===== 模式控制函数 ===== */

/**
 * @brief 设置当前工作模式
 * @param new_mode 新的工作模式
 */
void set_mode(record_mode_t new_mode)
{
    mode = new_mode;
}

/**
 * @brief 获取当前工作模式
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
 * 1. 录制模式：从 I2S RX 通道读取音频数据，保存到缓冲区，然后写入 SD 卡
 * 2. 播放模式：从 SD 卡加载 WAV 文件，通过 I2S TX 通道播放
 * 3. 空闲模式：等待模式切换
 *
 * @param arg 任务参数（未使用）
 */
void audio_record_task(void *arg)
{
    static esp_err_t ret = ESP_OK;
    static size_t bytes_read = 0;
    static size_t bytes_write = 0;
    char filename[64];

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
                ret = i2s_channel_read(rx_handle_, audio_buffer + audio_index,
                                       FRAME_SIZE, &bytes_read, 100);

                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "[echo] i2s read failed");
                    abort();
                }
                audio_index += FRAME_SIZE;
            }

            /* 保存 WAV 文件到 SD 卡 */
            snprintf(filename, sizeof(filename), "%s%ld.wav",
                     RECORD_FILE_PREFIX, file_index++);
            if (wav_save_file(filename, audio_buffer, audio_index) == ESP_OK)
            {
                ESP_LOGI(TAG, "Saved recording to %s", filename);
            }

            if (mode == record)
                mode = idle;
            ESP_LOGI(TAG, "record end");
        }
        else if (mode == play)
        {
            /* ===== 播放模式 ===== */
            ESP_LOGI(TAG, "play start");

            /* 从 SD 卡加载 WAV 文件 */
            snprintf(filename, sizeof(filename), "%s%ld.wav",
                     RECORD_FILE_PREFIX, file_index > 0 ? file_index - 1 : 0);

            size_t loaded_size = 0;
            if (wav_load_file(filename, audio_buffer, BUF_SIZE, &loaded_size) == ESP_OK)
            {
                audio_total = loaded_size;
                audio_index = 0;
                ESP_LOGI(TAG, "Loaded %s, size: %d", filename, loaded_size);

                /* 循环播放音频数据 */
                while ((mode == play) && (audio_index < audio_total))
                {
                    size_t bytes_to_write = MIN(FRAME_SIZE, audio_total - audio_index);

                    /* 向 I2S TX 通道写入音频数据（扬声器输出） */
                    ret = i2s_channel_write(tx_handle_,
                                            audio_buffer + audio_index,
                                            bytes_to_write,
                                            &bytes_write,
                                            100);

                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "[echo] i2s write failed");
                        mode = idle;
                        break;
                    }
                    audio_index += bytes_write;

                    /* 添加适当的延时避免 CPU 占用过高 */
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to load file %s", filename);
            }

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
 * 2. 初始化 SD 卡文件系统
 * 3. 分配音频缓冲区（PSRAM）
 * 4. 初始化 LVGL 显示界面
 * 5. 创建音频录制/播放任务
 *
 * @note SD 卡应使用 FAT32 格式化
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

    /* ===== 初始化 SD 卡文件系统 ===== */
    fs_config_t fs_config = {
        .type = FS_TYPE_SD_CARD,
        .sd_card = {
            .mount_point = "/sdcard",
            .clk = GPIO_NUM_15,     /* SD 卡时钟引脚 */
            .cmd = GPIO_NUM_7,      /* SD 卡命令引脚 */
            .d0 = GPIO_NUM_4,       /* SD 卡数据0引脚 */
            .format_if_mount_failed = false,
            .max_files = 5
        }
    };

    if (fs_manager_init(&fs_config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize filesystem");
        vTaskDelete(NULL);
        return;
    }

    /* 显示 SD 卡文件列表 */
    fs_manager_list_files("/sdcard");

    /* ===== 分配音频缓冲区（使用 PSRAM） ===== */
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
