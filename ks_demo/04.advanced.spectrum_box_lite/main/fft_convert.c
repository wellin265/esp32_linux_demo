/**
 * @file fft_convert.c
 * @brief Audio capture to FFT conversion pipeline.
 */

#include <math.h>
#include <string.h>
#include <limits.h>

#include "display.h"
#include "esp_check.h"
#include "esp_dsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "fft_convert.h"

#define FFT_SAMPLE_RATE         16000
#define RB_LENGTH               (N_SAMPLES * sizeof(int16_t) * 16)
#define DISPLAY_TIMEOUT_COUNT   40
#define MEL_MIN_FREQ            40.0f
#define MEL_MAX_FREQ            7600.0f
#define MEL_BAND_FLOOR          1.0e-4f
#define PCEN_SMOOTH_ALPHA       0.10f
#define PCEN_GAIN               0.82f
#define PCEN_BIAS               2.8f
#define PCEN_POWER              0.50f
#define PCEN_EPSILON            1.0e-4f
#define PCEN_OUTPUT_GAIN        18.0f
#define TRANSIENT_GAIN          6.0f
#define BAND_NOISE_FAST         0.08f
#define BAND_NOISE_SLOW         0.002f
#define BAND_GATE_OPEN          1.45f
#define BAND_GATE_CLOSE         1.18f

static const char *TAG = "FFT_CONVERT";

static RingbufHandle_t rb_handle = NULL;
static float *fft_buff = NULL;
static float *window_coeff = NULL;
static float *fft_mag = NULL;
static int16_t *fft_input = NULL;
static int16_t *sample_frame = NULL;
static uint16_t mel_bin_start[DISPLAY_BAND_COUNT];
static uint16_t mel_bin_center[DISPLAY_BAND_COUNT];
static uint16_t mel_bin_end[DISPLAY_BAND_COUNT];
static float pcen_state[DISPLAY_BAND_COUNT] = {0};
static float band_prev[DISPLAY_BAND_COUNT] = {0};
static float band_noise[DISPLAY_BAND_COUNT] = {0};
static uint8_t band_active[DISPLAY_BAND_COUNT] = {0};

static void build_hann_window(void)
{
    for (int i = 0; i < N_SAMPLES; i++) {
        window_coeff[i] = 0.5f * (1.0f - cosf((2.0f * 3.14159265359f * i) / (N_SAMPLES - 1)));
    }
}

static float hz_to_mel(float hz)
{
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

static float mel_to_hz(float mel)
{
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

static void build_mel_filterbank(void)
{
    float mel_min = hz_to_mel(MEL_MIN_FREQ);
    float mel_max = hz_to_mel(MEL_MAX_FREQ);
    float mel_step = (mel_max - mel_min) / (DISPLAY_BAND_COUNT + 1);
    float bin_hz = (float)FFT_SAMPLE_RATE / (float)N_SAMPLES;

    for (int band = 0; band < DISPLAY_BAND_COUNT; band++) {
        float left_hz = mel_to_hz(mel_min + mel_step * band);
        float center_hz = mel_to_hz(mel_min + mel_step * (band + 1));
        float right_hz = mel_to_hz(mel_min + mel_step * (band + 2));
        int left_bin = (int)floorf(left_hz / bin_hz);
        int center_bin = (int)lroundf(center_hz / bin_hz);
        int right_bin = (int)ceilf(right_hz / bin_hz);

        if (left_bin < 1) {
            left_bin = 1;
        }
        if (center_bin <= left_bin) {
            center_bin = left_bin + 1;
        }
        if (right_bin <= center_bin) {
            right_bin = center_bin + 1;
        }
        if (right_bin >= (N_SAMPLES / 2)) {
            right_bin = (N_SAMPLES / 2) - 1;
        }
        if (center_bin >= right_bin) {
            center_bin = right_bin - 1;
        }

        mel_bin_start[band] = (uint16_t)left_bin;
        mel_bin_center[band] = (uint16_t)center_bin;
        mel_bin_end[band] = (uint16_t)right_bin;
    }
}

static float accumulate_mel_band(int band)
{
    int left = mel_bin_start[band];
    int center = mel_bin_center[band];
    int right = mel_bin_end[band];
    float sum = 0.0f;

    for (int bin = left; bin < center; bin++) {
        float weight = (float)(bin - left) / (float)(center - left);
        sum += fft_mag[bin] * weight;
    }
    for (int bin = center; bin <= right; bin++) {
        float weight = (float)(right - bin) / (float)(right - center);
        if (weight < 0.0f) {
            weight = 0.0f;
        }
        sum += fft_mag[bin] * weight;
    }

    return sum;
}

static void fft_process(const int16_t *samples, float *out_buff)
{
    float dc_sum = 0.0f;

    for (int i = 0; i < N_SAMPLES; i++) {
        dc_sum += samples[i];
    }
    const float dc_offset = dc_sum / N_SAMPLES;

    for (int i = 0; i < N_SAMPLES; i++) {
        float centered = ((float)samples[i] - dc_offset) * window_coeff[i];
        if (centered > INT16_MAX) {
            centered = INT16_MAX;
        } else if (centered < INT16_MIN) {
            centered = INT16_MIN;
        }
        fft_input[i * 2] = (int16_t)centered;
        fft_input[i * 2 + 1] = 0;
    }

    dsps_fft2r_sc16_ansi(fft_input, N_SAMPLES);
    dsps_bit_rev_sc16_ansi(fft_input, N_SAMPLES);

    fft_mag[0] = 0.0f;
    for (int i = 1; i < (N_SAMPLES / 2); i++) {
        float real = (float)fft_input[i * 2];
        float imag = (float)fft_input[i * 2 + 1];
        float magnitude = sqrtf(real * real + imag * imag) / N_SAMPLES;
        fft_mag[i] = magnitude * magnitude;
    }

    for (int band = 0; band < DISPLAY_BAND_COUNT; band++) {
        float energy = accumulate_mel_band(band);
        float smooth;
        float pcen;
        float transient;
        float out;

        if (energy < MEL_BAND_FLOOR) {
            energy = MEL_BAND_FLOOR;
        }

        if (band_noise[band] == 0.0f) {
            band_noise[band] = energy;
        } else if (energy < band_noise[band]) {
            band_noise[band] = band_noise[band] * (1.0f - BAND_NOISE_FAST) + energy * BAND_NOISE_FAST;
        } else {
            band_noise[band] = band_noise[band] * (1.0f - BAND_NOISE_SLOW) + energy * BAND_NOISE_SLOW;
        }

        if (!band_active[band]) {
            if (energy > band_noise[band] * BAND_GATE_OPEN) {
                band_active[band] = 1;
            }
        } else if (energy < band_noise[band] * BAND_GATE_CLOSE) {
            band_active[band] = 0;
        }

        if (!band_active[band]) {
            energy = band_noise[band];
        }

        if (pcen_state[band] == 0.0f) {
            pcen_state[band] = energy;
        } else {
            pcen_state[band] = pcen_state[band] * (1.0f - PCEN_SMOOTH_ALPHA) + energy * PCEN_SMOOTH_ALPHA;
        }

        smooth = powf(PCEN_EPSILON + pcen_state[band], PCEN_GAIN);
        pcen = powf(energy / smooth + PCEN_BIAS, PCEN_POWER) - powf(PCEN_BIAS, PCEN_POWER);
        if (pcen < 0.0f) {
            pcen = 0.0f;
        }

        transient = pcen - band_prev[band];
        if (transient < 0.0f) {
            transient = 0.0f;
        }

        out = pcen * PCEN_OUTPUT_GAIN + transient * TRANSIENT_GAIN;
        if (out > 250.0f) {
            out = 250.0f;
        }

        out_buff[band] = out;
        band_prev[band] = pcen;
    }

    for (int i = DISPLAY_BAND_COUNT; i < N_SAMPLES; i++) {
        out_buff[i] = 0.0f;
    }
}

static esp_err_t fft_init(void)
{
    fft_buff = calloc(N_SAMPLES, sizeof(float));
    window_coeff = calloc(N_SAMPLES, sizeof(float));
    fft_mag = calloc(N_SAMPLES / 2, sizeof(float));
    fft_input = calloc(N_SAMPLES * 2, sizeof(int16_t));
    sample_frame = calloc(N_SAMPLES, sizeof(int16_t));
    if (!fft_buff || !window_coeff || !fft_mag || !fft_input || !sample_frame) {
        ESP_LOGE(TAG, "Failed to allocate FFT buffers");
        return ESP_ERR_NO_MEM;
    }

    build_hann_window();
    build_mel_filterbank();

    esp_err_t ret = dsps_fft2r_init_sc16(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FFT init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "FFT engine ready: %d samples @ %d Hz, %d mel bands",
             N_SAMPLES, FFT_SAMPLE_RATE, DISPLAY_BAND_COUNT);
    return ESP_OK;
}

static esp_err_t rb_init(void)
{
    rb_handle = xRingbufferCreate(RB_LENGTH, RINGBUF_TYPE_BYTEBUF);
    if (rb_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create ringbuffer");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void fft_convert_task(void *pvParameter)
{
    size_t item_size = 0;
    size_t collected_bytes = 0;
    size_t display_wtd = 0;
    const size_t frame_bytes = N_SAMPLES * sizeof(int16_t);

    while (1) {
        int16_t *data = (int16_t *)xRingbufferReceive(rb_handle, &item_size, pdMS_TO_TICKS(20));
        if (data == NULL) {
            if (++display_wtd > DISPLAY_TIMEOUT_COUNT) {
                display_draw(NULL);
                display_wtd = 0;
            }
            continue;
        }

        display_wtd = 0;

        size_t copy_bytes = item_size;
        if ((collected_bytes + copy_bytes) > frame_bytes) {
            copy_bytes = frame_bytes - collected_bytes;
        }

        memcpy((uint8_t *)sample_frame + collected_bytes, data, copy_bytes);
        collected_bytes += copy_bytes;
        vRingbufferReturnItem(rb_handle, (void *)data);

        if (collected_bytes >= frame_bytes) {
            fft_process(sample_frame, fft_buff);
            display_draw(fft_buff);
            collected_bytes = 0;
        }
    }
}

void rb_write(int16_t *buf, size_t size)
{
    if (buf == NULL || size == 0 || rb_handle == NULL) {
        return;
    }

    if (xRingbufferSend(rb_handle, (void *)buf, size, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Ringbuffer full, dropping %u bytes", (unsigned)size);
    }
}

esp_err_t fft_convert_init(void)
{
    ESP_RETURN_ON_ERROR(rb_init(), TAG, "ringbuffer init failed");
    ESP_RETURN_ON_ERROR(fft_init(), TAG, "fft init failed");

    BaseType_t ok = xTaskCreate(fft_convert_task, "fft_convert_task", 1024 * 8, NULL, 4, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create fft task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FFT task started");
    return ESP_OK;
}
