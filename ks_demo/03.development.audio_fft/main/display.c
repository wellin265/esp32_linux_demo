/**
 * @file display.c
 * @brief FFT spectrum display renderer with switchable styles.
 */

#include <math.h>
#include <string.h>

#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fft_convert.h"
#include "ksdiy_lvgl_port.h"

#define LCD_WIDTH           280
#define LCD_HEIGHT          240
#define LCD_BUFFER_SIZE     (LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t))

#define GROUP_WIDTH         7
#define STRIP_WIDTH         5
#define BASE_BAR_HEIGHT     3
#define PEAK_GAP            2
#define PEAK_GRAVITY        1.8f

#define STRIP_NUM           (LCD_WIDTH / GROUP_WIDTH)

static const char *TAG = "display";

static int16_t fre_point[STRIP_NUM] = {0};
static lv_color_t *display_buffer = NULL;
static lv_obj_t *s_fft_canvas = NULL;
static float animated_bar_height[STRIP_NUM] = {0};
static float target_bar_height[STRIP_NUM] = {0};
static float peak_pos[STRIP_NUM] = {0};
static float peak_vel[STRIP_NUM] = {0};

static lv_color_t strip_color[STRIP_NUM];
static uint8_t strip_r8[STRIP_NUM];
static uint8_t strip_g8[STRIP_NUM];
static uint8_t strip_b8[STRIP_NUM];

static display_style_t current_style = DISPLAY_STYLE_MIRROR;

/* ==================== Frequency mapping ==================== */

static void frequency_multiplier_calculation(void)
{
    const float min_freq = 40.0f;
    const float max_freq = 8000.0f;
    const float bin_hz = 16000.0f / N_SAMPLES;

    for (int i = 0; i < STRIP_NUM; i++) {
        float t = (float)i / (STRIP_NUM - 1);
        float freq = min_freq * powf(max_freq / min_freq, t);
        int bin = (int)lroundf(freq / bin_hz);
        if (bin < 1) {
            bin = 1;
        } else if (bin >= (N_SAMPLES / 2)) {
            bin = (N_SAMPLES / 2) - 1;
        }
        fre_point[i] = (int16_t)bin;
    }

    ESP_LOGI(TAG, "Frequency map ready: %d bars", STRIP_NUM);
}

/* ==================== Color palette ==================== */

static void build_strip_colors(void)
{
    for (int i = 0; i < STRIP_NUM; i++) {
        float t = (float)i / (STRIP_NUM - 1);
        float r, g, b;
        if (t < 0.33f) {
            float s = t / 0.33f;
            r = 1.0f - s * 0.5f;
            g = 0.0f;
            b = s;
        } else if (t < 0.66f) {
            float s = (t - 0.33f) / 0.33f;
            r = 0.5f * (1.0f - s);
            g = s;
            b = 1.0f;
        } else {
            float s = (t - 0.66f) / 0.34f;
            r = 0.0f;
            g = 1.0f - s * 0.3f;
            b = 1.0f - s * 0.2f;
        }
        strip_r8[i] = (uint8_t)(r * 255);
        strip_g8[i] = (uint8_t)(g * 255);
        strip_b8[i] = (uint8_t)(b * 255);
        strip_color[i] = lv_color_make(strip_r8[i], strip_g8[i], strip_b8[i]);
    }
}

/* ==================== Common bar computation ==================== */

static void compute_bars(float *data)
{
    for (int i = 0; i < STRIP_NUM; i++) {
        float energy = data[fre_point[i]];
        if (energy < 0.0f) {
            energy = 0.0f;
        }

        if (i < 8) {
            energy *= 2.8f;
        } else if (i < 18) {
            energy *= 2.0f;
        } else {
            energy *= 1.6f;
        }

        target_bar_height[i] = sqrtf(energy) * 62.0f;
    }

    for (int i = 0; i < STRIP_NUM; i++) {
        float center = target_bar_height[i];
        float left = (i > 0) ? target_bar_height[i - 1] : center;
        float right = (i < STRIP_NUM - 1) ? target_bar_height[i + 1] : center;
        float smoothed = center * 0.7f + left * 0.15f + right * 0.15f;

        if (smoothed > animated_bar_height[i]) {
            animated_bar_height[i] = smoothed;
        } else {
            animated_bar_height[i] = animated_bar_height[i] * 0.55f + smoothed * 0.45f;
        }
    }
}

static void update_peaks(const int *bar_heights)
{
    for (int i = 0; i < STRIP_NUM; i++) {
        float bar_top = (float)bar_heights[i] + PEAK_GAP;

        if (bar_top >= peak_pos[i]) {
            peak_pos[i] = bar_top;
            peak_vel[i] = 0.0f;
        } else {
            peak_vel[i] += PEAK_GRAVITY;
            peak_pos[i] -= peak_vel[i];
            if (peak_pos[i] < bar_top) {
                peak_pos[i] = bar_top;
                peak_vel[i] = 0.0f;
            }
        }
    }
}

/* ==================== Style: Classic (full-screen bars) ==================== */

static void render_classic(void)
{
    int bar_heights[STRIP_NUM];
    for (int i = 0; i < STRIP_NUM; i++) {
        bar_heights[i] = BASE_BAR_HEIGHT + (int)animated_bar_height[i];
        if (bar_heights[i] > LCD_HEIGHT) {
            bar_heights[i] = LCD_HEIGHT;
        }
    }

    update_peaks(bar_heights);

    for (int i = 0; i < STRIP_NUM; i++) {
        const int x_start = i * GROUP_WIDTH;
        int bar_h = bar_heights[i];
        lv_color_t col = strip_color[i];
        int peak_row = LCD_HEIGHT - (int)peak_pos[i];

        for (int dx = 0; dx < STRIP_WIDTH; dx++) {
            int x = x_start + dx;
            if (x >= LCD_WIDTH) break;

            for (int y = 0; y < bar_h; y++) {
                display_buffer[(LCD_HEIGHT - 1 - y) * LCD_WIDTH + x] = col;
            }

            if (peak_row >= 0 && peak_row < LCD_HEIGHT) {
                display_buffer[peak_row * LCD_WIDTH + x] = col;
            }
        }
    }
}

/* ==================== Style: Mirror (water reflection) ==================== */

static void render_mirror(void)
{
    const int mirror_gap = 3;
    const int upper_base = LCD_HEIGHT / 2 - mirror_gap / 2;
    const int lower_base = LCD_HEIGHT / 2 + (mirror_gap + 1) / 2;
    const int max_upper = upper_base - BASE_BAR_HEIGHT;
    const float scale = (float)max_upper / (float)(LCD_HEIGHT - BASE_BAR_HEIGHT);

    int bar_heights[STRIP_NUM];
    for (int i = 0; i < STRIP_NUM; i++) {
        bar_heights[i] = BASE_BAR_HEIGHT + (int)(animated_bar_height[i] * scale);
        if (bar_heights[i] > upper_base) {
            bar_heights[i] = upper_base;
        }
    }

    update_peaks(bar_heights);

    for (int i = 0; i < STRIP_NUM; i++) {
        const int x_start = i * GROUP_WIDTH;
        int bar_h = bar_heights[i];
        int reflect_h = (bar_h + 2) / 3;

        lv_color_t col = strip_color[i];
        uint8_t cr_half = strip_r8[i] / 2;
        uint8_t cg_half = strip_g8[i] / 2;
        uint8_t cb_half = strip_b8[i] / 2;
        lv_color_t col_dim = lv_color_make(cr_half, cg_half, cb_half);

        int peak_up_row = upper_base - (int)peak_pos[i];
        int reflect_peak = (int)peak_pos[i] / 3;
        int peak_dn_row = lower_base + reflect_peak;

        for (int dx = 0; dx < STRIP_WIDTH; dx++) {
            int x = x_start + dx;
            if (x >= LCD_WIDTH) break;

            for (int y = 0; y < bar_h; y++) {
                int row = upper_base - 1 - y;
                if (row >= 0) {
                    display_buffer[row * LCD_WIDTH + x] = col;
                }
            }

            for (int y = 0; y < reflect_h; y++) {
                int row = lower_base + y;
                if (row < LCD_HEIGHT) {
                    display_buffer[row * LCD_WIDTH + x] = col_dim;
                }
            }

            if (peak_up_row >= 0 && peak_up_row < upper_base) {
                display_buffer[peak_up_row * LCD_WIDTH + x] = col;
            }
            if (peak_dn_row >= lower_base && peak_dn_row < LCD_HEIGHT) {
                display_buffer[peak_dn_row * LCD_WIDTH + x] = col_dim;
            }
        }
    }
}

/* ==================== Public API ==================== */

void display_set_canvas(lv_obj_t *canvas)
{
    s_fft_canvas = canvas;
}

void display_set_style(display_style_t style)
{
    current_style = style;
    memset(animated_bar_height, 0, sizeof(animated_bar_height));
    memset(target_bar_height, 0, sizeof(target_bar_height));
    memset(peak_pos, 0, sizeof(peak_pos));
    memset(peak_vel, 0, sizeof(peak_vel));
    ESP_LOGI(TAG, "Style switched to %d", (int)style);
}

display_style_t display_get_style(void)
{
    return current_style;
}

void display_next_style(void)
{
    int next = ((int)current_style + 1) % DISPLAY_STYLE_MAX;
    display_set_style((display_style_t)next);
}

esp_err_t display_draw(float *data)
{
    if (display_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(display_buffer, 0, LCD_BUFFER_SIZE);

    if (data == NULL) {
        memset(animated_bar_height, 0, sizeof(animated_bar_height));
        memset(target_bar_height, 0, sizeof(target_bar_height));
        memset(peak_pos, 0, sizeof(peak_pos));
        memset(peak_vel, 0, sizeof(peak_vel));
    } else {
        compute_bars(data);

        switch (current_style) {
        case DISPLAY_STYLE_MIRROR:
            render_mirror();
            break;
        case DISPLAY_STYLE_CLASSIC:
        default:
            render_classic();
            break;
        }
    }

    if (s_fft_canvas != NULL && ksdiy_lvgl_lock(10)) {
        lv_canvas_set_buffer(s_fft_canvas, display_buffer, LCD_WIDTH, LCD_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(s_fft_canvas, LCD_WIDTH, LCD_HEIGHT);
        lv_obj_center(s_fft_canvas);
        lv_obj_invalidate(s_fft_canvas);
        ksdiy_lvgl_unlock();
    }

    return ESP_OK;
}

esp_err_t display_buffer_init(void)
{
    display_buffer = heap_caps_calloc(1, LCD_BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (display_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate display buffer");
        return ESP_ERR_NO_MEM;
    }

    frequency_multiplier_calculation();
    build_strip_colors();

    ESP_LOGI(TAG, "Display buffer ready: %dx%d", LCD_WIDTH, LCD_HEIGHT);
    return ESP_OK;
}
