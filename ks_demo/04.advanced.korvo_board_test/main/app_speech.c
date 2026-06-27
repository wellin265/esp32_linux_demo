/**
 * @file app_speech.c
 * @brief 语音识别唤醒模块
 *
 * 本模块基于 ESP-SR 框架实现语音唤醒（WakeNet）和命令词识别功能。
 * 集成 I2S 音频采集、AFE（音频前端处理）和语音命令识别。
 *
 * 硬件连接:
 *   - I2S 数据线连接 ES8311/ES7210 音频编解码器
 *   - I2C 控制线配置编解码器参数
 *   - 使用板上麦克风采集语音
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_speech.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"
#include "esp_process_sdkconfig.h"
#include "page_wakeup.h"
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

static const char *TAG = "APP_Wakenet";
esp_afe_sr_data_t *afe_detection_data_ = NULL;
static int play_voice = -2;
static esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
int detect_flag = 0;
srmodel_list_t *models = NULL;

char *wakenet_model_ = NULL;
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


#define AUDIO_BUFFER 2048 // buffer size for reading the wav file and sending to i2s
/* 播放WAV音频文件 也就是原始音频直接给I2S就行了*/
esp_err_t play_wav(char *fp)
{
    FILE *fh = fopen(fp, "rb");
    if (fh == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file");
        return ESP_ERR_INVALID_ARG;
    }

    // skip the header...
    fseek(fh, 44, SEEK_SET);

    // create a writer buffer
    int16_t *buf = calloc(AUDIO_BUFFER, sizeof(int16_t));
    size_t bytes_read = 0;
    size_t bytes_written = 0;

    bytes_read = fread(buf, sizeof(int16_t), AUDIO_BUFFER, fh);

    while (bytes_read > 0)
    {
        // write the buffer to the i2s
        // i2s_channel_write(tx_handle_, buf, bytes_read * sizeof(int16_t), &bytes_written, portMAX_DELAY);
        esp_codec_dev_write(output_dev_, (void*)buf, bytes_read * sizeof(int16_t));
        bytes_read = fread(buf, sizeof(int16_t), AUDIO_BUFFER, fh);
        ESP_LOGV(TAG, "Bytes read: %d", bytes_read);
    }

    free(buf);
    fclose(fh);
    return ESP_OK;
}

void play_music(void *arg)
{
    size_t bytes_written;

    while (1)
    {
        switch (play_voice)
        {
        case -2:
            vTaskDelay(10);
            break;
        case -1:
            // wake_up_action();
            play_voice = -2;
            ESP_LOGI(TAG, "唤醒播放");
            page_wakeup_start();
            // wakeup_set_text("Say!");
            play_wav("/spiffs/wozai.wav");
            break;
        default:
            ESP_LOGI(TAG, "命令词播放");
            wakeup_set_text("好的,请继续说:");
            // speech_commands_action(play_voice);
            play_wav("/spiffs/haode.wav");
            play_voice = -2;
            break;
        }
    }
    vTaskDelete(NULL);
}
//之前初始化触摸时已经初始化了IIC 这里就不用再次初始化了
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
        .dma_desc_num = 6,
        .dma_frame_num = 240,
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
    // es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC3;

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
    if (1)
    {
        fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2);
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
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, 80));
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

/* 通过此任务把音频数据读取并且喂给ESP AFE进行识别*/

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_total_channel_num(afe_data);
    int feed_channel = 2;
    assert(nch <= feed_channel);
    uint32_t samples = audio_chunksize * sizeof(int16_t) * feed_channel;
    esp_err_t ret = ESP_OK;

    int16_t *i2s_buff = malloc(samples);
    assert(i2s_buff);
    // int audio_chunksize = samples / (sizeof(int32_t));

    while (1)
    {
        size_t bytes_read;
        // ret = i2s_channel_read(rx_handle_, i2s_buff, samples, &bytes_read, portMAX_DELAY);
        esp_codec_dev_read(input_dev_, (void*)i2s_buff, samples );


        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff)
    {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}
/* 通过此任务获取识别到的结果 并且进行处理*/

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    printf("multinet:%s\n", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);
    esp_mn_commands_update_from_sdkconfig(multinet, model_data); // Add speech commands from sdkconfig
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == afe_chunksize);
    bool is_speaking_ = false;

    // print active speech commands
    multinet->print_active_speech_commands(model_data);
    printf("------------detect start------------\n");
    while (1)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            printf("fetch error!\n");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED)
        {
            printf("WAKEWORD DETECTED\n");
            multinet->clean(model_data); // clean all status of multinet
        }
        else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            play_voice = -1;
            detect_flag = 1;
            printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
        }
        // VAD state change
        if (res->vad_state == AFE_VAD_SPEECH && !is_speaking_)
        {
            is_speaking_ = true;
        }
        else if (res->vad_state == AFE_VAD_SILENCE && is_speaking_)
        {
            is_speaking_ = false;
        }

        if (detect_flag == 1)
        {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING)
            {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++)
                {
                    printf("TOP %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n",
                           i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                    play_voice = mn_result->command_id[i];
                }

                printf("\n-----------listening-----------\n");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                detect_flag = 0;
                printf("\n-----------awaits to be waken up-----------\n");
                page_wakeup_end();
                continue;
            }
        }
    }
    if (model_data)
    {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    printf("detect exit\n");
    vTaskDelete(NULL);
}
/* 初始化flash里面的语音识别模型和识别相关任务*/

void Wakenet_init(void)
{
    Codec_I2S_init();
    models = esp_srmodel_init("model");

    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();

    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config.aec_init = true;
    afe_config.pcm_config.total_ch_num = 2;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 1;
    afe_config.wakenet_mode = DET_MODE_90;
    afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    afe_config.se_init = true;
    afe_config.vad_init = true;
    afe_config.afe_mode = SR_MODE_LOW_COST;
    afe_data = afe_handle->create_from_config(&afe_config);

    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void *)afe_data, 5, NULL, 1);
    xTaskCreatePinnedToCore(&play_music, "play", 6 * 1024, NULL, 5, NULL, 1);
}
