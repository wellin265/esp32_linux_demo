#pragma once

#include <stdint.h>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"

namespace driver {
class AdcBase {
public:
    virtual ~AdcBase() = default;
    virtual esp_err_t init() = 0;
    virtual int read_raw() = 0;
    virtual float read_voltage() = 0;
protected:
    adc_unit_t unit_;
    adc_channel_t channel_;
    adc_atten_t atten_;
};

class AdcOneShot : public AdcBase {
public:
    explicit AdcOneShot(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten);
    ~AdcOneShot() override;
    esp_err_t init() override;
    int read_raw() override;
    float read_voltage() override;
protected:
    void adc_calibration_init();
private:
    bool do_calibration_ = false;

    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t adc_cali_handle_ = nullptr;

    int raw_ = 0;
    float voltage_ = 0.0;
};

class AdcContinuous : public AdcBase {
public:
    explicit AdcContinuous(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten,
                           uint32_t sample_freq_hz = 20000,
                           uint32_t conv_frame_size = 256,
                           uint32_t max_store_buf_size = 1024);
    ~AdcContinuous() override;
    esp_err_t init() override;
    int read_raw() override;
    float read_voltage() override;

    esp_err_t start();
    esp_err_t stop();
    esp_err_t fetch(uint32_t timeout_ms = 100);

    uint32_t get_last_raw()   const { return stats_.last_raw; }
    uint32_t get_min_raw()    const { return stats_.min_raw; }
    uint32_t get_max_raw()    const { return stats_.max_raw; }
    uint32_t get_avg_raw()    const { return stats_.avg_raw; }
    uint32_t get_sample_count() const { return stats_.sample_count; }
    uint32_t get_frame_bytes() const { return stats_.frame_bytes; }

private:
    void adc_calibration_init();

    uint32_t sample_freq_hz_;
    uint32_t conv_frame_size_;
    uint32_t max_store_buf_size_;

    adc_continuous_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t adc_cali_handle_ = nullptr;
    bool do_calibration_ = false;

    uint8_t *read_buf_ = nullptr;
    adc_continuous_data_t *parsed_buf_ = nullptr;
    uint32_t parsed_buf_size_ = 0;

    float voltage_ = 0.0f;

    struct Statistics {
        uint32_t last_raw = 0;
        uint32_t min_raw = UINT32_MAX;
        uint32_t max_raw = 0;
        uint32_t avg_raw = 0;
        uint32_t sample_count = 0;
        uint32_t frame_bytes = 0;

        void reset() {
            last_raw = 0;
            min_raw = UINT32_MAX;
            max_raw = 0;
            avg_raw = 0;
            sample_count = 0;
            frame_bytes = 0;
        }
        void update(uint32_t raw) {
            last_raw = raw;
            if (raw < min_raw) min_raw = raw;
            if (raw > max_raw) max_raw = raw;
        }
    };
    Statistics stats_;
};

} // namespace driver