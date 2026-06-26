/*
 * @Descripttion :
 * @version      :
 * @Author       : Kevincoooool
 * @Date         : 2021-05-25 09:20:06
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2023-07-15 15:59:03
 * @FilePath: \SP_V2_DEMO\6.lcd_camera_lvgl_v7\main\app_camera.c
 */
#include "app_speech.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include <stdio.h>
#include <stdint.h>

#include <math.h> // 包含 math.h 头文件
#include "driver/i2s_tdm.h"
#include "driver/i2s_std.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include <driver/i2c_master.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

extern int detect_flag;

#define TAG "I2S_TEST"
i2s_chan_handle_t tx_handle_ = NULL;
i2s_chan_handle_t rx_handle_ = NULL;

#define PA_GPIO_NUM GPIO_NUM_48
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_16
#define AUDIO_I2S_GPIO_WS GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_9
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_10
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_8
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_17
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_18

const audio_codec_data_if_t *data_if_ = NULL;
const audio_codec_ctrl_if_t *out_ctrl_if_ = NULL;
const audio_codec_if_t *out_codec_if_ = NULL;
const audio_codec_ctrl_if_t *in_ctrl_if_ = NULL;
const audio_codec_if_t *in_codec_if_ = NULL;
const audio_codec_gpio_if_t *gpio_if_ = NULL;

esp_codec_dev_handle_t output_dev_ = NULL;
esp_codec_dev_handle_t input_dev_ = NULL;
extern i2c_master_bus_handle_t touch_i2c_bus_;

void InitializeCodecI2c()
{
    // Initialize I2C peripheral
    // i2c_master_bus_config_t i2c_bus_cfg = {
    //     .i2c_port = I2C_NUM_1,
    //     .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
    //     .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
    //     .clk_source = I2C_CLK_SRC_DEFAULT,
    //     .glitch_ignore_cnt = 7,
    //     .intr_priority = 0,
    //     .trans_queue_depth = 0,
    //     .flags = {
    //         .enable_internal_pullup = 1,
    //     },
    // };
    // ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
}

void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din)
{

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

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256},
        .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = I2S_SLOT_MODE_STEREO, .slot_mask = I2S_STD_SLOT_BOTH, .ws_width = I2S_DATA_BIT_WIDTH_16BIT, .ws_pol = false, .bit_shift = true, .left_align = true, .big_endian = false, .bit_order_lsb = false},
        .gpio_cfg = {.mclk = mclk, .bclk = bclk, .ws = ws, .dout = dout, .din = I2S_GPIO_UNUSED, .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};
    i2s_tdm_slot_mask_t slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3;
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = I2S_SLOT_MODE_STEREO, .slot_mask = slot_mask, .ws_width = I2S_TDM_AUTO_WS_WIDTH, .ws_pol = false, .bit_shift = true, .left_align = false, .big_endian = false, .bit_order_lsb = false, .skip_mask = false, .total_slot = I2S_TDM_AUTO_SLOT_NUM},
        .gpio_cfg = {.mclk = mclk, .bclk = bclk, .ws = ws, .dout = I2S_GPIO_UNUSED, .din = din, .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    ESP_LOGI(TAG, "Duplex channels created");
}
static void Init_codec(void)
{

    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    // Output
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_1,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = touch_i2c_bus_,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

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

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    // Input
    i2c_cfg.addr = ES7210_CODEC_DEFAULT_ADDR;
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != NULL);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    // es7210_cfg.mic_selected = ES7210_SEL_MIC1 ;

    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != NULL);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 4,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = (uint32_t)16000,
        .mclk_multiple = 0,
    };
    if (0)
    {
        fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
    }
    ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 40.0));

    esp_codec_dev_sample_info_t fs_out = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = (uint32_t)16000,
        .mclk_multiple = 0,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs_out));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, 90));
    ESP_LOGI(TAG, "AudioDevice initialized");
}

/* 初始化I2S 输入输出两个共用一路I2S*/

void Codec_I2S_init(void)
{

    InitializeCodecI2c(); // 最开始触摸初始化已经初始化IIC
    CreateDuplexChannels(AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);

    Init_codec();             // 初始化IIS
    esp_rom_gpio_pad_select_gpio(PA_GPIO_NUM); // 打开功放使能
    gpio_set_direction(PA_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(PA_GPIO_NUM, 1);
}
