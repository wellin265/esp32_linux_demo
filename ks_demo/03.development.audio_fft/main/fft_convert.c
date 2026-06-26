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

static const char *TAG = "FFT_CONVERT";

static RingbufHandle_t rb_handle = NULL;
static float *fft_buff = NULL;
static float *window_coeff = NULL;
static int16_t *fft_input = NULL;
static int16_t *sample_frame = NULL;
static float noise_floor[N_SAMPLES / 2] = {0};

static void build_hann_window(void)
{
    for (int i = 0; i < N_SAMPLES; i++) {
        window_coeff[i] = 0.5f * (1.0f - cosf((2.0f * 3.14159265359f * i) / (N_SAMPLES - 1)));
    }
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

    out_buff[0] = 0;
    for (int i = 1; i < (N_SAMPLES / 2); i++) {
        float real = (float)fft_input[i * 2];
        float imag = (float)fft_input[i * 2 + 1];
        float magnitude = sqrtf(real * real + imag * imag) / N_SAMPLES;
        float db = 20.0f * log10f(magnitude + 1.0f) + 22.0f;

        if (noise_floor[i] == 0.0f) {
            noise_floor[i] = db;
        } else if (db < noise_floor[i]) {
            noise_floor[i] = noise_floor[i] * 0.80f + db * 0.20f;
        } else {
            noise_floor[i] = noise_floor[i] * 0.9995f + db * 0.0005f;
        }

        float clean = db - noise_floor[i];
        if (clean < 0.0f) {
            clean = 0.0f;
        }
        clean = clean * sqrtf(clean) * 0.8f;

        if (clean > 250.0f) {
            clean = 250.0f;
        }

        out_buff[i] = clean;
    }

    for (int i = N_SAMPLES / 2; i < N_SAMPLES; i++) {
        out_buff[i] = 0.0f;
    }
}

static esp_err_t fft_init(void)
{
    fft_buff = calloc(N_SAMPLES, sizeof(float));
    window_coeff = calloc(N_SAMPLES, sizeof(float));
    fft_input = calloc(N_SAMPLES * 2, sizeof(int16_t));
    sample_frame = calloc(N_SAMPLES, sizeof(int16_t));
    if (!fft_buff || !window_coeff || !fft_input || !sample_frame) {
        ESP_LOGE(TAG, "Failed to allocate FFT buffers");
        return ESP_ERR_NO_MEM;
    }

    build_hann_window();

    esp_err_t ret = dsps_fft2r_init_sc16(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FFT init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "FFT engine ready: %d samples @ %d Hz", N_SAMPLES, FFT_SAMPLE_RATE);
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
