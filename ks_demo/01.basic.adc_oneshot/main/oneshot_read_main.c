/*
 * ADC oneshot example for GPIO5 with live display output.
 */

#include <stdio.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "soc/soc_caps.h"

#define EXAMPLE_ADC_ATTEN       ADC_ATTEN_DB_12
#define EXAMPLE_READ_PERIOD_MS  40

#if CONFIG_IDF_TARGET_ESP32
#define EXAMPLE_ADC1_CHAN0      ADC_CHANNEL_5
#else
#define EXAMPLE_ADC1_CHAN0      ADC_CHANNEL_4
#endif

static const char *TAG = "adc_oneshot";

static bool example_adc_calibration_init(adc_unit_t unit,
                                         adc_channel_t channel,
                                         adc_atten_t atten,
                                         adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

static void update_adc_display(int raw, int voltage_mv, bool calibrated)
{
    char line1[48];
    char line2[48];

    snprintf(line1, sizeof(line1), "GPIO5 raw %d", raw);
    if (calibrated) {
        snprintf(line2, sizeof(line2), "voltage %d mV", voltage_mv);
    } else {
        snprintf(line2, sizeof(line2), "calibration unavailable");
    }

    ksdiy_example_display_set_lines(line1, line2, "oneshot mode");
}

void app_main(void)
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t adc1_cali_handle = NULL;
    bool do_calibration = false;

    ksdiy_example_display_bootstrap("01.basic.adc_oneshot", "GPIO5 single read");
    ksdiy_example_display_set_lines("waiting for GPIO5...", "configuring adc", "oneshot mode");

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    do_calibration = example_adc_calibration_init(
        ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_handle);

    while (1) {
        int raw = 0;
        int voltage_mv = 0;

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &raw));
        if (do_calibration) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage_mv));
        }

        update_adc_display(raw, voltage_mv, do_calibration);
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_READ_PERIOD_MS));
    }
}

static bool example_adc_calibration_init(adc_unit_t unit,
                                         adc_channel_t channel,
                                         adc_atten_t atten,
                                         adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Calibration init failed");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#else
    (void)handle;
#endif
}
