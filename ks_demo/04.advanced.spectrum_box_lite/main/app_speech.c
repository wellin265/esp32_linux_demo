/**
 * @file app_speech.c
 * @brief Audio codec and I2S setup for the FFT demo.
 */

#include "app_speech.h"

#include <driver/i2c_master.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"

#define TAG                     "I2S_CODEC"
#define PA_GPIO_NUM             GPIO_NUM_48

#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_16
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_9
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_10
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_8

#define AUDIO_SAMPLE_RATE       16000

i2s_chan_handle_t tx_handle_ = NULL;
i2s_chan_handle_t rx_handle_ = NULL;

static const audio_codec_data_if_t *data_if_ = NULL;
static const audio_codec_ctrl_if_t *out_ctrl_if_ = NULL;
static const audio_codec_if_t *out_codec_if_ = NULL;
static const audio_codec_ctrl_if_t *in_ctrl_if_ = NULL;
static const audio_codec_if_t *in_codec_if_ = NULL;
static const audio_codec_gpio_if_t *gpio_if_ = NULL;

esp_codec_dev_handle_t output_dev_ = NULL;
esp_codec_dev_handle_t input_dev_ = NULL;

extern i2c_master_bus_handle_t touch_i2c_bus_;

static void create_duplex_channels(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 256,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = AUDIO_SAMPLE_RATE,
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
            .mclk = AUDIO_I2S_GPIO_MCLK,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws = AUDIO_I2S_GPIO_WS,
            .dout = AUDIO_I2S_GPIO_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {0},
        },
    };

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_TDM_SLOT0,
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
            .mclk = AUDIO_I2S_GPIO_MCLK,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws = AUDIO_I2S_GPIO_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = AUDIO_I2S_GPIO_DIN,
            .invert_flags = {0},
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));

    ESP_LOGI(TAG, "I2S duplex channels ready @ %d Hz", AUDIO_SAMPLE_RATE);
}

static void init_codec(void)
{
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_1,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = touch_i2c_bus_,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8311_codec_cfg_t es8311_cfg = {0};
    es8311_cfg.ctrl_if = out_ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311_cfg.pa_pin = GPIO_NUM_NC;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0f;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3f;
    out_codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(out_codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    i2c_cfg.addr = ES7210_CODEC_DEFAULT_ADDR;
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != NULL);

    es7210_codec_cfg_t es7210_cfg = {0};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1;
    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != NULL);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);

    esp_codec_dev_sample_info_t fs_in = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .mclk_multiple = 0,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs_in));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 40.0f));

    esp_codec_dev_sample_info_t fs_out = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .mclk_multiple = 0,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs_out));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, 90));

    ESP_LOGI(TAG, "Codec initialized: ES8311 + ES7210");
}

void Codec_I2S_init(void)
{
    create_duplex_channels();
    init_codec();

    esp_rom_gpio_pad_select_gpio(PA_GPIO_NUM);
    gpio_set_direction(PA_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(PA_GPIO_NUM, 1);

    ESP_LOGI(TAG, "Audio pipeline ready");
}
