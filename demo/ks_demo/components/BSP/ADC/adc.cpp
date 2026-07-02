#include "adc.hpp"

#include "esp_log.h"

#define TAG "Adc"

namespace driver {
AdcOneShot::AdcOneShot(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten) {
    unit_ = unit;
    channel_ = channel;
    atten_ = atten;
}

AdcOneShot::~AdcOneShot() {
    if (adc_cali_handle_) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adc_cali_handle_);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(adc_cali_handle_);
#endif
        adc_cali_handle_ = nullptr;
    }
    if (adc_handle_) {
        adc_oneshot_del_unit(adc_handle_);
        adc_handle_ = nullptr;
    }
}

esp_err_t AdcOneShot::init() {
    /* 步骤 1：创建 ADC oneshot 单元 */
    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = unit_;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
    /* 步骤 2：配置通道衰减与位宽 */
    adc_oneshot_chan_cfg_t config = {
        .atten = atten_,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, channel_, &config));
    /* 步骤 3：尝试创建软件校准（依赖 eFuse / 曲线拟合方案） */
    adc_calibration_init();
    return ESP_OK;
}

/**
 * @brief 初始化 ADC 软件校准句柄
 * @return true 校准创建成功；false 跳过或失败
 */
void AdcOneShot::adc_calibration_init() {
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "Using curve fitting calibration");
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit_,
            .chan = channel_,
            .atten = atten_,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    /* 备选：线性拟合校准方案 */
    ESP_LOGW(TAG, "Using line fitting calibration (lower precision)");
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit_,
            .atten = atten_,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Calibration init failed");
    }

    do_calibration_ = calibrated;
}

int AdcOneShot::read_raw() {
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, channel_, &raw_));
    return raw_;
}

float AdcOneShot::read_voltage() {
    if (do_calibration_) {
        int voltage_mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle_, raw_, &voltage_mv));
        voltage_ = voltage_mv / 1000.0; // 将电压值从毫伏换算为伏特
    }
    else
    {
        // 这里可以根据实际的 ADC 配置进行电压换算，以下是一个简单的示例，假设使用 12dB 衰减和默认位宽（12 位）：
        // 12dB 衰减下，输入电压范围为 0 - 3.3V，12 位分辨率对应 4095 个数值单位
        voltage_ = (raw_ / 4095.0) * 3.3; // 将 ADC 原始值换算为电压值，单位为伏特
    }
    return voltage_;
}

AdcContinuous::AdcContinuous(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten,
                             uint32_t sample_freq_hz, uint32_t conv_frame_size, uint32_t max_store_buf_size) {
    unit_ = unit;
    channel_ = channel;
    atten_ = atten;
    sample_freq_hz_ = sample_freq_hz;
    conv_frame_size_ = conv_frame_size;
    max_store_buf_size_ = max_store_buf_size;
}

AdcContinuous::~AdcContinuous() {
    stop();
    if (adc_cali_handle_) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adc_cali_handle_);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(adc_cali_handle_);
#endif
        adc_cali_handle_ = nullptr;
    }
    if (adc_handle_) {
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
    }
    if (read_buf_) {
        heap_caps_free(read_buf_);
        read_buf_ = nullptr;
    }
    if (parsed_buf_) {
        heap_caps_free(parsed_buf_);
        parsed_buf_ = nullptr;
    }
}

esp_err_t AdcContinuous::init() {
    adc_continuous_handle_cfg_t adc_config = {};
    adc_config.max_store_buf_size = max_store_buf_size_;
    adc_config.conv_frame_size = conv_frame_size_;
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle_));

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {};

    adc_continuous_config_t dig_cfg = {};
    dig_cfg.sample_freq_hz = sample_freq_hz_;
    dig_cfg.conv_mode = (unit_ == ADC_UNIT_1) ? ADC_CONV_SINGLE_UNIT_1 : ADC_CONV_SINGLE_UNIT_2;
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    adc_pattern[0].atten = atten_;
    adc_pattern[0].channel = channel_ & 0x7;
    adc_pattern[0].unit = unit_;
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    dig_cfg.adc_pattern = adc_pattern;
    dig_cfg.pattern_num = 1;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle_, &dig_cfg));

    adc_calibration_init();

    read_buf_ = (uint8_t *)heap_caps_calloc(1, conv_frame_size_, MALLOC_CAP_DMA);
    if (!read_buf_) {
        ESP_LOGE(TAG, "Failed to allocate DMA read buffer");
        return ESP_ERR_NO_MEM;
    }
    parsed_buf_size_ = conv_frame_size_ / SOC_ADC_DIGI_RESULT_BYTES;
    parsed_buf_ = (adc_continuous_data_t *)heap_caps_calloc(parsed_buf_size_, sizeof(adc_continuous_data_t), MALLOC_CAP_8BIT);
    if (!parsed_buf_) {
        ESP_LOGE(TAG, "Failed to allocate parsed buffer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ADC continuous init: unit=%d chan=%d freq=%lu Hz",
             unit_ & 0x1, channel_, sample_freq_hz_);
    return ESP_OK;
}

void AdcContinuous::adc_calibration_init() {
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "Using curve fitting calibration");
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit_,
            .chan = channel_,
            .atten = atten_,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGW(TAG, "Using line fitting calibration (lower precision)");
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit_,
            .atten = atten_,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Calibration init failed");
    }

    do_calibration_ = calibrated;
}

esp_err_t AdcContinuous::start() {
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle_));
    ESP_LOGI(TAG, "ADC continuous started");
    return ESP_OK;
}

esp_err_t AdcContinuous::stop() {
    if (adc_handle_) {
        adc_continuous_stop(adc_handle_);
    }
    ESP_LOGI(TAG, "ADC continuous stopped");
    return ESP_OK;
}

esp_err_t AdcContinuous::fetch(uint32_t timeout_ms) {
    stats_.reset();
    uint32_t total_bytes = 0;
    uint64_t sum_raw = 0;

    // 阻塞等待第一帧数据（阻塞期间让出 CPU）
    uint32_t ret_num = 0;
    esp_err_t ret = adc_continuous_read(adc_handle_, read_buf_, conv_frame_size_, &ret_num,
                                         pdMS_TO_TICKS(timeout_ms));
    if (ret == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC continuous read err: %d", ret);
        return ret;
    }

    // 解析第一帧
    do {
        total_bytes += ret_num;
        uint32_t num_parsed = 0;
        ret = adc_continuous_parse_data(adc_handle_, read_buf_, ret_num, parsed_buf_, &num_parsed);
        if (ret != ESP_OK) break;

        for (uint32_t i = 0; i < num_parsed; i++) {
            if (!parsed_buf_[i].valid) continue;
            uint32_t raw = parsed_buf_[i].raw_data;
            stats_.update(raw);
            sum_raw += raw;
            stats_.sample_count++;
        }

        // 非阻塞排空 DMA 池中剩余数据
        ret = adc_continuous_read(adc_handle_, read_buf_, conv_frame_size_, &ret_num, 0);
    } while (ret == ESP_OK);

    stats_.frame_bytes = total_bytes;
    if (stats_.sample_count > 0) {
        stats_.avg_raw = (uint32_t)(sum_raw / stats_.sample_count);
    }
    return ESP_OK;
}

int AdcContinuous::read_raw() {
    return static_cast<int>(stats_.last_raw);
}

float AdcContinuous::read_voltage() {
    int raw = static_cast<int>(stats_.last_raw);
    if (do_calibration_ && stats_.last_raw > 0) {
        int voltage_mv = 0;
        esp_err_t ret = adc_cali_raw_to_voltage(adc_cali_handle_, raw, &voltage_mv);
        if (ret == ESP_OK) {
            voltage_ = voltage_mv / 1000.0f;
        }
    } else {
        voltage_ = (raw / 4095.0f) * 3.3f;
    }
    return voltage_;
}

} // namespace driver