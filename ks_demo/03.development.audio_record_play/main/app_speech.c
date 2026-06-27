/**
 * @file app_speech.c
 * @brief 音频编解码器（I2S + ES8311/ES7210）初始化实现
 *
 * 实现 I2S 双工通道配置、ES8311 DAC 和 ES7210 ADC 编解码器初始化。
 * TX 通道使用 I2S STD 模式（16kHz），RX 通道使用 I2S TDM 模式（16kHz，4 麦克风）。
 *
 * 硬件连接（KSDIY Korvo ESP32-S3）：
 * - I2S MCLK:  GPIO16    - I2S BCLK:  GPIO9
 * - I2S WS:    GPIO45    - I2S DOUT:  GPIO8 (DAC 输出)
 * - I2S DIN:   GPIO10 (ADC 输入)
 * - PA 使能:   GPIO48
 * - I2C SDA:   GPIO17    - I2C SCL:   GPIO18
 *
 * @copyright SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "app_speech.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "driver/i2s_tdm.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include <driver/i2c_master.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

/* ===== 常量定义 ===== */

/** @brief 日志标签 */
#define TAG "I2S_CODEC"

/** @brief 功放 PA 使能引脚 */
#define PA_GPIO_NUM             GPIO_NUM_48

/** @brief I2S 音频引脚配置 */
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_16
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_9
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_10
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_8

/** @brief 编解码器 I2C 引脚 */
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_17
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_18

/** @brief 音频采样率 */
#define SAMPLE_RATE  16000

/* ===== 全局变量 ===== */

i2s_chan_handle_t tx_handle_ = NULL;     /**< I2S 发送通道（DAC 输出） */
i2s_chan_handle_t rx_handle_ = NULL;     /**< I2S 接收通道（ADC 输入） */

/** @brief 编解码器接口 */
static const audio_codec_data_if_t  *data_if_     = NULL;  /**< I2S 数据接口 */
static const audio_codec_ctrl_if_t  *out_ctrl_if_ = NULL;  /**< DAC I2C 控制接口 */
static const audio_codec_if_t       *out_codec_if_ = NULL;  /**< ES8311 编解码器接口 */
static const audio_codec_ctrl_if_t  *in_ctrl_if_  = NULL;   /**< ADC I2C 控制接口 */
static const audio_codec_if_t       *in_codec_if_  = NULL;   /**< ES7210 编解码器接口 */
static const audio_codec_gpio_if_t  *gpio_if_     = NULL;    /**< GPIO 接口 */

esp_codec_dev_handle_t output_dev_ = NULL;   /**< DAC 输出设备句柄 */
esp_codec_dev_handle_t input_dev_  = NULL;   /**< ADC 输入设备句柄 */

/** @brief 触摸 I2C 总线句柄（由 ksdiy_lvgl_port 初始化） */
extern i2c_master_bus_handle_t touch_i2c_bus_;

/* ===== 内部函数 ===== */

/**
 * @brief 创建 I2S 双工通道
 *
 * TX 通道：I2S STD 模式，16kHz 采样率，16 位立体声
 * RX 通道：I2S TDM 模式，16kHz 采样率，16 位立体声，Slot0~3（4 麦克风）
 */
static void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk,
                                  gpio_num_t ws, gpio_num_t dout, gpio_num_t din)
{
    /* 配置 I2S 通道（双工模式） */
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 2,
        .dma_frame_num = 240 * 3,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    /* TX 通道配置：I2S STD 模式（DAC 输出，16kHz） */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = mclk, .bclk = bclk, .ws = ws,
            .dout = dout, .din = I2S_GPIO_UNUSED,
            .invert_flags = {0},
        },
    };

    /* RX 通道配置：I2S TDM 模式（ADC 输入，16kHz，4 麦克风 Slot0~3） */
    i2s_tdm_slot_mask_t slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 |
                                     I2S_TDM_SLOT2 | I2S_TDM_SLOT3;

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = slot_mask,
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM,
        },
        .gpio_cfg = {
            .mclk = mclk, .bclk = bclk, .ws = ws,
            .dout = I2S_GPIO_UNUSED, .din = din,
            .invert_flags = {0},
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    ESP_LOGI(TAG, "I2S 双工通道创建完成 (TX: STD, RX: TDM 4MIC, 16kHz)");
}

/**
 * @brief 初始化 ES8311 (DAC) 和 ES7210 (ADC) 编解码器
 *
 * 配置编解码器的 I2S 数据接口、I2C 控制接口、GPIO 接口，
 * 设置采样参数（16kHz, 16bit）和音量。
 */
static void Init_codec(void)
{
    /* 创建 I2S 数据接口 */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    /* ES8311 DAC：I2C 控制接口 */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_1,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = touch_i2c_bus_,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    /* ES8311 DAC 编解码器配置 */
    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = out_ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311_cfg.pa_pin = GPIO_NUM_NC;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    out_codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(out_codec_if_ != NULL);

    /* 创建 DAC 输出设备 */
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    /* ES7210 ADC：I2C 控制接口 */
    i2c_cfg.addr = ES7210_CODEC_DEFAULT_ADDR;
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != NULL);

    /* ES7210 ADC 编解码器配置（使用 MIC1~MIC4，4 麦克风阵列） */
    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 |
                               ES7210_SEL_MIC3 | ES7210_SEL_MIC4;

    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != NULL);

    /* 创建 ADC 输入设备 */
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);

    /* 配置输入采样参数：16kHz, 16bit, 4 通道 */
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 4,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = SAMPLE_RATE,
        .mclk_multiple = 0,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_,
                    ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 40.0));

    /* 配置输出采样参数：16kHz, 16bit, 单声道 */
    esp_codec_dev_sample_info_t fs_out = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = SAMPLE_RATE,
        .mclk_multiple = 0,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs_out));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, 90));

    ESP_LOGI(TAG, "音频编解码器初始化完成 (ES8311 DAC + ES7210 ADC 4MIC)");
}

/* ===== 公共 API ===== */

void Codec_I2S_init(void)
{
    /* 创建 I2S 双工通道 */
    CreateDuplexChannels(
        AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN
    );

    /* 初始化编解码器 */
    Init_codec();

    /* 使能功放 PA */
    esp_rom_gpio_pad_select_gpio(PA_GPIO_NUM);
    gpio_set_direction(PA_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(PA_GPIO_NUM, 1);

    ESP_LOGI(TAG, "音频子系统初始化完成");
}
