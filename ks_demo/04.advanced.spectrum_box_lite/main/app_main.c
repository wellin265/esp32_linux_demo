/**
 * @file app_main.c
 * @brief FFT 音频频谱分析仪主程序
 *
 * 本示例通过 I2S 采集麦克风音频数据，使用 ESP-DSP 库进行 FFT 变换，
 * 然后在 LCD 屏幕上实时显示音频频谱柱状图。
 *
 * 功能流程：
 * 1. 初始化 NVS、LVGL 显示、音频编解码器
 * 2. 创建 FFT 画布用于频谱显示
 * 3. 在主循环中持续采集音频数据写入环形缓冲区
 * 4. FFT 转换任务从缓冲区读取数据并进行频谱分析和显示
 *
 * 硬件平台：KSDIY Korvo 开发板（ESP32-S3）
 *
 * @copyright SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_timer.h"
#endif
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_check.h"

#include "display.h"
#include "fft_convert.h"
#include "ksdiy_lvgl_port.h"
#include "app_speech.h"

/* ===== 常量定义 ===== */

/** @brief 日志标签 */
static const char *TAG = "fft";

/** @brief 兼容 IDF v5.0 的 GPIO 选择函数 */
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define gpio_pad_select_gpio esp_rom_gpio_pad_select_gpio
#endif

/** @brief 每次读取的音频采样点数 */
#define FRAME_SAMPLES (256)

/** @brief 每次读取的字节数 */
#define FRAME_SIZE (FRAME_SAMPLES * sizeof(int16_t))

#define STYLE_KEY_GPIO      GPIO_NUM_5
#define STYLE_KEY_ACTIVE    0

/* ===== 全局变量 ===== */

/**
 * @brief FFT 频谱显示画布（LVGL 对象）
 *
 * 由 app_main 创建，display.c 通过 display_set_canvas() 设置后使用。
 */
static lv_obj_t *fft_canvas = NULL;

/** @brief 音频采集缓冲区 */
static int16_t *audio_buffer = NULL;

/* ===== 主函数 ===== */

/**
 * @brief 应用程序主入口
 *
 * 初始化所有子系统后，进入主循环持续采集音频数据。
 * FFT 计算和显示在独立任务 fft_convert_task 中完成。
 */
void app_main(void)
{
    int last_key_level;

    ESP_LOGI(TAG, "编译时间: %s %s", __DATE__, __TIME__);

    /* 步骤 1：初始化 NVS 非易失性存储 */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 分区需要擦除，重新初始化");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* 步骤 2：初始化 LVGL 显示 */
    ksdiy_lvgl_port_init();

    /* 步骤 3：初始化音频编解码器和 I2S */
    Codec_I2S_init();

    /* 步骤 4：初始化显示缓冲区和频率映射表 */
    ESP_ERROR_CHECK(display_buffer_init());

    /* 步骤 5：创建 FFT 画布并交给显示模块 */
    if (ksdiy_lvgl_lock(100)) {
        fft_canvas = lv_canvas_create(lv_scr_act());
        assert(fft_canvas);
        display_set_canvas(fft_canvas);
        lv_obj_center(fft_canvas);
        ksdiy_lvgl_unlock();
    }

    /* 步骤 6：初始化 FFT 转换（创建环形缓冲区和 FFT 任务） */
    ESP_ERROR_CHECK(fft_convert_init());

    /* 步骤 7：分配音频采集缓冲区 */
    audio_buffer = heap_caps_malloc(FRAME_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "无法分配音频缓冲区");
        return;
    }

    /* 步骤 8：使能功放 PA（GPIO48） */
    gpio_pad_select_gpio(GPIO_NUM_48);
    gpio_set_direction(GPIO_NUM_48, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_48, 1);

    gpio_config_t key_io_conf = {
        .pin_bit_mask = 1ULL << STYLE_KEY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&key_io_conf));
    last_key_level = gpio_get_level(STYLE_KEY_GPIO);

    /* ===== 主循环：持续采集音频数据 ===== */
    while (1) {
        int key_level = gpio_get_level(STYLE_KEY_GPIO);
        if (last_key_level != STYLE_KEY_ACTIVE && key_level == STYLE_KEY_ACTIVE) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (gpio_get_level(STYLE_KEY_GPIO) == STYLE_KEY_ACTIVE) {
                display_next_style();
            }
        }
        last_key_level = key_level;

        esp_err_t ret = esp_codec_dev_read(input_dev_, audio_buffer, FRAME_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "音频采集失败: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        rb_write(audio_buffer, FRAME_SIZE);
    }
}
