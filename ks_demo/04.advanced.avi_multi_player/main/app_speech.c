/**
 * @file app_speech.c
 * @brief 音频编解码器初始化模块
 *
 * 本文件实现 ESP32-S3 的音频编解码器初始化：
 * - I2C 总线初始化（用于控制 ES8311）
 * - I2S 总线初始化（用于音频数据传输）
 * - ES8311 DAC 编解码器配置
 *
 * 硬件配置：
 * - 音频 DAC：ES8311
 * - I2C 地址：0x18
 * - 采样率：16kHz
 * - 位深：16位
 * - 输出通道：立体声
 *
 * @author Kevincoooool
 * @date 2021-05-25
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

/** @brief 语音唤醒检测标志（外部变量） */
extern int detect_flag;

/** @brief 日志标签 */
static const char *TAG = "APP_Wakenet";

/* ===== I2S 句柄 ===== */
i2s_chan_handle_t tx_handle_ = NULL;  /**< I2S 发送通道句柄 */
i2s_chan_handle_t rx_handle_ = NULL;  /**< I2S 接收通道句柄 */

/* ===== GPIO 引脚配置 ===== */
#define PA_GPIO_NUM GPIO_NUM_48              /**< 功放使能引脚 */
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_16      /**< I2S 主时钟引脚 */
#define AUDIO_I2S_GPIO_WS GPIO_NUM_45        /**< I2S 字选择引脚 */
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_9       /**< I2S 位时钟引脚 */
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_10       /**< I2S 数据输入引脚（ADC->ESP32） */
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_8       /**< I2S 数据输出引脚（ESP32->DAC） */
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_17  /**< 编解码器 I2C 数据引脚 */
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_18  /**< 编解码器 I2C 时钟引脚 */

/* ===== 音频编解码器接口 ===== */
const audio_codec_data_if_t *data_if_ = NULL;      /**< 数据接口 */
const audio_codec_ctrl_if_t *out_ctrl_if_ = NULL;  /**< 输出控制接口 */
const audio_codec_if_t *out_codec_if_ = NULL;      /**< 输出编解码器接口 */
const audio_codec_ctrl_if_t *in_ctrl_if_ = NULL;   /**< 输入控制接口 */
const audio_codec_if_t *in_codec_if_ = NULL;       /**< 输入编解码器接口 */
const audio_codec_gpio_if_t *gpio_if_ = NULL;      /**< GPIO 接口 */

/* ===== 编解码器设备句柄 ===== */
esp_codec_dev_handle_t output_dev_ = NULL;  /**< 输出设备句柄（ES8311） */
esp_codec_dev_handle_t input_dev_ = NULL;   /**< 输入设备句柄 */
i2c_master_bus_handle_t codec_i2c_bus_;    /**< I2C 总线句柄 */

/**
 * @brief 初始化编解码器 I2C 总线
 *
 * 配置 I2C 主机模式用于与 ES8311 通信。
 */
void InitializeCodecI2c()
{
    /* I2C 主机总线配置 */
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,  /* 启用内部上拉电阻 */
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
}

/**
 * @brief 创建 I2S 双工通道
 *
 * 初始化 I2S 发送（TX）通道用于向 DAC 发送音频数据。
 * 注意：本示例仅使用 TX 通道用于音频输出。
 *
 * @param mclk  主时钟引脚
 * @param bclk  位时钟引脚
 * @param ws    字选择引脚
 * @param dout  数据输出引脚
 * @param din   数据输入引脚（未使用）
 */
void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din)
{
    /* I2S 通道配置 */
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,          /* DMA 描述符数量 */
        .dma_frame_num = 240,       /* 每 DMA 描述符的帧数 */
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    /* 标准 I2S 配置 */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)16000,  /* 16kHz 采样率 */
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256  /* MCLK = 256 * 采样率 */
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
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = I2S_GPIO_UNUSED,  /* 不使用输入 */
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}
        }
    };

    /* 初始化 I2S 标准模式 */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));

    /* 启用 I2S 通道 */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));

    ESP_LOGI(TAG, "Duplex channels created");
}

/**
 * @brief 初始化音频编解码器
 *
 * 配置 ES8311 DAC 编解码器：
 * - I2C 地址: 0x18
 * - 工作模式: DAC（仅输出）
 * - 采样率: 16kHz
 * - 输出音量: 90%
 */
void Init_codec(void)
{
    /* ===== 创建 I2S 数据接口 ===== */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .tx_handle = tx_handle_,  /* 仅使用发送通道 */
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    /* ===== 配置 ES8311 DAC（输出） ===== */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_1,
        .addr = ES8311_CODEC_DEFAULT_ADDR,  /* 0x18 */
        .bus_handle = codec_i2c_bus_,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    /* 创建 GPIO 接口 */
    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    /* ES8311 编解码器配置 */
    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = out_ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;  /* DAC 模式（仅输出） */
    es8311_cfg.pa_pin = GPIO_NUM_NC;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    out_codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(out_codec_if_ != NULL);

    /* 创建输出设备 */
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    /* ===== 配置音频参数 ===== */
    esp_codec_dev_sample_info_t fs_out = {
        .bits_per_sample = 16,
        .channel = 2,                /* 立体声输出 */
        .channel_mask = 0,
        .sample_rate = (uint32_t)16000,
        .mclk_multiple = 0,
    };

    /* 打开音频设备并设置音量 */
    ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs_out));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, 90));  /* 音量 90% */

    ESP_LOGI(TAG, "AudioDevice initialized");
}

/**
 * @brief 初始化音频编解码器和 I2S
 *
 * 完整的音频系统初始化流程：
 * 1. 初始化 I2C 总线（用于控制 ES8311）
 * 2. 创建 I2S 通道（用于音频数据传输）
 * 3. 初始化 ES8311 编解码器
 * 4. 启用功放
 */
void Codec_I2S_init(void)
{
    /* 初始化 I2C 总线 */
    InitializeCodecI2c();

    /* 创建 I2S 通道 */
    CreateDuplexChannels(AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                         AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);

    /* 初始化编解码器 */
    Init_codec();

    /* 配置功放使能引脚 */
    esp_rom_gpio_pad_select_gpio(PA_GPIO_NUM);
    gpio_set_direction(PA_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(PA_GPIO_NUM, 1);  /* 启用功放 */
}
