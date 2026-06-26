/*
 * ADC continuous read example with live display stats.
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "sdkconfig.h"

#define EXAMPLE_ADC_UNIT          ADC_UNIT_1
#define EXAMPLE_ADC_CONV_MODE     ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN         ADC_ATTEN_DB_12
#define EXAMPLE_ADC_BIT_WIDTH     SOC_ADC_DIGI_MAX_BITWIDTH
#define EXAMPLE_READ_LEN          256
#define EXAMPLE_SAMPLE_FREQ_HZ    (20 * 1000)
#define EXAMPLE_UI_UPDATE_MS      20

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[] = {ADC_CHANNEL_5};
#else
static adc_channel_t channel[] = {ADC_CHANNEL_4};
#endif

static TaskHandle_t s_task_handle;
static const char *TAG = "adc_cont";

typedef struct {
    uint32_t last_raw;
    uint32_t min_raw;
    uint32_t max_raw;
    uint32_t avg_raw;
    uint32_t sample_count;
    uint32_t frame_bytes;
} adc_display_stats_t;

static void update_adc_display(const adc_display_stats_t *stats)
{
    char line1[48];
    char line2[48];
    char line3[48];

    snprintf(line1, sizeof(line1), "GPIO5 raw %lu",
             (unsigned long)stats->last_raw);
    snprintf(line2, sizeof(line2), "avg %lu min %lu",
             (unsigned long)stats->avg_raw,
             (unsigned long)stats->min_raw);
    snprintf(line3, sizeof(line3), "max %lu samples %lu",
             (unsigned long)stats->max_raw,
             (unsigned long)stats->sample_count);

    ksdiy_example_display_set_lines(line1, line2, line3);
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle,
                                     const adc_continuous_evt_data_t *edata,
                                     void *user_data)
{
    BaseType_t must_yield = pdFALSE;
    (void)handle;
    (void)edata;
    (void)user_data;

    vTaskNotifyGiveFromISR(s_task_handle, &must_yield);
    return must_yield == pdTRUE;
}

static void continuous_adc_init(adc_channel_t *channels,
                                uint8_t channel_num,
                                adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = EXAMPLE_SAMPLE_FREQ_HZ,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
    };
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};

    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channels[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "pattern[%d] channel=%" PRIx8 " atten=%" PRIx8,
                 i,
                 adc_pattern[i].channel,
                 adc_pattern[i].atten);
    }
    dig_cfg.adc_pattern = adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));
    *out_handle = handle;
}

void app_main(void)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    TickType_t last_ui_tick = 0;

    memset(result, 0xcc, sizeof(result));

    ksdiy_example_display_bootstrap("01.basic.adc_continuous", "GPIO5 live adc");
    ksdiy_example_display_set_lines("waiting for GPIO5...", "sample rate 20 kHz", "single channel");

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1) {
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_ERR_TIMEOUT) {
                break;
            }

            ESP_ERROR_CHECK(ret);

            adc_continuous_data_t parsed_data[EXAMPLE_READ_LEN / SOC_ADC_DIGI_RESULT_BYTES];
            uint32_t num_parsed_samples = 0;
            esp_err_t parse_ret = adc_continuous_parse_data(
                handle, result, ret_num, parsed_data, &num_parsed_samples);

            if (parse_ret != ESP_OK) {
                ESP_LOGE(TAG, "Data parsing failed: %s", esp_err_to_name(parse_ret));
                ksdiy_example_display_set_lines("parse failed", esp_err_to_name(parse_ret), "check serial log");
                continue;
            }

            adc_display_stats_t stats = {
                .min_raw = UINT_MAX,
                .frame_bytes = ret_num,
            };

            for (uint32_t i = 0; i < num_parsed_samples; i++) {
                if (!parsed_data[i].valid) {
                    ESP_LOGW(TAG, "Invalid data [ADC%d_Ch%d_%" PRIu32 "]",
                             parsed_data[i].unit + 1,
                             parsed_data[i].channel,
                             parsed_data[i].raw_data);
                    continue;
                }

                stats.sample_count++;
                stats.last_raw = parsed_data[i].raw_data;
                stats.avg_raw += parsed_data[i].raw_data;
                if (parsed_data[i].raw_data < stats.min_raw) {
                    stats.min_raw = parsed_data[i].raw_data;
                }
                if (parsed_data[i].raw_data > stats.max_raw) {
                    stats.max_raw = parsed_data[i].raw_data;
                }

            }

            if (stats.sample_count == 0) {
                continue;
            }

            stats.avg_raw /= stats.sample_count;
            if (stats.min_raw == UINT_MAX) {
                stats.min_raw = 0;
            }

            TickType_t now = xTaskGetTickCount();
            if ((last_ui_tick == 0) || ((now - last_ui_tick) >= pdMS_TO_TICKS(EXAMPLE_UI_UPDATE_MS))) {
                update_adc_display(&stats);
                last_ui_tick = now;
            }

            vTaskDelay(1);
        }
    }
}
