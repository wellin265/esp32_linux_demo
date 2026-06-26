/**
 * @file display.c
 * @brief FFT spectrum display renderer with switchable styles.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "display_styles.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fft_convert.h"
#include "ksdiy_lvgl_port.h"

#define LCD_WIDTH           280                                 // LCD 宽度
#define LCD_HEIGHT          240                                 // LCD 高度
#define LCD_BUFFER_SIZE     (LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t)) // 画布缓冲区大小

/* ==================== 调参区：基础布局 ==================== */
#define GROUP_WIDTH         7                                   // 每组频谱占用总宽度（含柱子和间隔）
#define STRIP_WIDTH         5                                   // 单根柱子的实际宽度
#define BASE_BAR_HEIGHT     1                                   // 静音时的基础保底柱高
#define PEAK_GAP            2                                   // 峰值点与柱顶的间距
#define PEAK_GRAVITY        2.8f                                // 峰值点下落速度

/* ==================== 调参区：柱高映射 ==================== */
#define BAR_ENERGY_FLOOR    2.0f                                // 底噪死区，小于该值直接忽略
#define BAR_LOW_GAIN        2.1f                                // 低频增益
#define BAR_MID_GAIN        1.55f                               // 中低频增益
#define BAR_HIGH_GAIN       1.18f                               // 中高频/高频增益
#define BAR_HEIGHT_SCALE    42.0f                               // 总体柱高缩放倍率

/* ==================== 调参区：柱体平滑 ==================== */
#define BAR_SPATIAL_CENTER  0.84f                               // 当前柱空间平滑权重
#define BAR_SPATIAL_SIDE    0.08f                               // 相邻柱空间平滑权重
#define BAR_JITTER_GATE     0.18f                               // 小于该变化量时忽略微抖动
#define BAR_RISE_LIMIT      16.0f                               // 单帧最大上升幅度
#define BAR_FALL_LIMIT      8.0f                                // 单帧最大下降幅度
#define BAR_ATTACK_KEEP     0.02f                               // 上升滤波时保留上一帧的比例
#define BAR_ATTACK_APPLY    0.95f                               // 上升滤波时应用新值的比例
#define BAR_RELEASE_KEEP    0.32f                               // 下降滤波时保留上一帧的比例
#define BAR_RELEASE_APPLY   0.68f                               // 下降滤波时应用新值的比例

/* ==================== 调参区：球体风格 ==================== */
#define BALL_COUNT          15                                  // 球体风格里小球的数量
#define BALL_OPACITY        LV_OPA_50                           // 球体混色透明度
#define BALL_TOP_MARGIN     12                                  // 球体活动区域顶部边距
#define BALL_BOTTOM_LIMIT   240                                 // 球体活动区域底部边界
#define BALL_SPEED_SCALE    6.0f                                // 球体运动速度倍率

#define STRIP_NUM           DISPLAY_BAND_COUNT

static const char *TAG = "display";

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
static uint32_t frame_counter = 0;

static display_style_t current_style = DISPLAY_STYLE_AURORA;

static display_ball_t balls[BALL_COUNT];
static uint32_t rng_state = 0x24681357;

static uint32_t next_rand(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static float rand_range(float min_v, float max_v)
{
    float unit = (float)(next_rand() & 0xFFFF) / 65535.0f;
    return min_v + (max_v - min_v) * unit;
}

static lv_color_t blend_opa(lv_color_t fg, lv_color_t bg, lv_opa_t opa)
{
    return lv_color_mix(fg, bg, opa);
}

static void init_balls(void)
{
    static const lv_color_t palette[7] = {
        LV_COLOR_MAKE(255, 64, 64),
        LV_COLOR_MAKE(255, 140, 32),
        LV_COLOR_MAKE(255, 220, 64),
        LV_COLOR_MAKE(80, 220, 96),
        LV_COLOR_MAKE(64, 180, 255),
        LV_COLOR_MAKE(96, 96, 255),
        LV_COLOR_MAKE(220, 96, 255),
    };

    for (int i = 0; i < BALL_COUNT; i++) {
        balls[i].radius = 8.0f + (float)(i % 3);
        balls[i].x = rand_range(24.0f, (float)LCD_WIDTH - 24.0f);
        balls[i].y = rand_range((float)BALL_TOP_MARGIN + 12.0f, (float)BALL_BOTTOM_LIMIT - 12.0f);
        balls[i].vx = rand_range(-1.05f * BALL_SPEED_SCALE, 1.05f * BALL_SPEED_SCALE);
        balls[i].vy = rand_range(-0.85f * BALL_SPEED_SCALE, 0.85f * BALL_SPEED_SCALE);
        if (fabsf(balls[i].vx) < 0.36f * BALL_SPEED_SCALE) {
            balls[i].vx = balls[i].vx < 0.0f ? -0.46f * BALL_SPEED_SCALE : 0.46f * BALL_SPEED_SCALE;
        }
        if (fabsf(balls[i].vy) < 0.36f * BALL_SPEED_SCALE) {
            balls[i].vy = balls[i].vy < 0.0f ? -0.40f * BALL_SPEED_SCALE : 0.40f * BALL_SPEED_SCALE;
        }
        balls[i].color = palette[next_rand() % 7];
    }
}

static void update_balls(void)
{
    for (int i = 0; i < BALL_COUNT; i++) {
        balls[i].x += balls[i].vx;
        balls[i].y += balls[i].vy;

        if (balls[i].x - balls[i].radius < 0.0f) {
            balls[i].x = balls[i].radius;
            balls[i].vx = fabsf(balls[i].vx);
        } else if (balls[i].x + balls[i].radius > (float)(LCD_WIDTH - 1)) {
            balls[i].x = (float)(LCD_WIDTH - 1) - balls[i].radius;
            balls[i].vx = -fabsf(balls[i].vx);
        }

        if (balls[i].y - balls[i].radius < (float)BALL_TOP_MARGIN) {
            balls[i].y = (float)BALL_TOP_MARGIN + balls[i].radius;
            balls[i].vy = fabsf(balls[i].vy);
        } else if (balls[i].y + balls[i].radius > (float)BALL_BOTTOM_LIMIT) {
            balls[i].y = (float)BALL_BOTTOM_LIMIT - balls[i].radius;
            balls[i].vy = -fabsf(balls[i].vy);
        }
    }

    for (int i = 0; i < BALL_COUNT; i++) {
        for (int j = i + 1; j < BALL_COUNT; j++) {
            float dx = balls[j].x - balls[i].x;
            float dy = balls[j].y - balls[i].y;
            float min_dist = balls[i].radius + balls[j].radius;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq > 0.01f && dist_sq < min_dist * min_dist) {
                float dist = sqrtf(dist_sq);
                float nx = dx / dist;
                float ny = dy / dist;
                float overlap = min_dist - dist;

                balls[i].x -= nx * overlap * 0.5f;
                balls[i].y -= ny * overlap * 0.5f;
                balls[j].x += nx * overlap * 0.5f;
                balls[j].y += ny * overlap * 0.5f;

                float tmp_vx = balls[i].vx;
                float tmp_vy = balls[i].vy;
                balls[i].vx = balls[j].vx;
                balls[i].vy = balls[j].vy;
                balls[j].vx = tmp_vx;
                balls[j].vy = tmp_vy;
            }
        }
    }
}

static void draw_balls(void)
{
    for (int i = 0; i < BALL_COUNT; i++) {
        float radius = balls[i].radius;
        int cx = (int)lroundf(balls[i].x);
        int cy = (int)lroundf(balls[i].y);
        int draw_radius = (int)ceilf(radius);
        float threshold = radius + 0.35f;
        float threshold_sq = threshold * threshold;

        for (int dy = -draw_radius; dy <= draw_radius; dy++) {
            int yy = cy + dy;
            if (yy < 0 || yy >= LCD_HEIGHT) {
                continue;
            }

            for (int dx = -draw_radius; dx <= draw_radius; dx++) {
                int xx = cx + dx;
                if (xx < 0 || xx >= LCD_WIDTH) {
                    continue;
                }

                float px = (float)dx + 0.5f;
                float py = (float)dy + 0.5f;
                float dist_sq = px * px + py * py;
                if (dist_sq > threshold_sq) {
                    continue;
                }

                lv_color_t *dst = &display_buffer[yy * LCD_WIDTH + xx];
                *dst = blend_opa(balls[i].color, *dst, BALL_OPACITY);
            }
        }
    }
}

/* display.c 持有底层状态数组，这里把它们组装成上下文交给风格渲染层。 */
static display_style_context_t make_style_context(void)
{
    display_style_context_t ctx = {
        .lcd_width = LCD_WIDTH,
        .lcd_height = LCD_HEIGHT,
        .group_width = GROUP_WIDTH,
        .strip_width = STRIP_WIDTH,
        .strip_num = STRIP_NUM,
        .base_bar_height = BASE_BAR_HEIGHT,
        .peak_gap = PEAK_GAP,
        .ball_count = BALL_COUNT,
        .frame_counter = frame_counter,
        .display_buffer = display_buffer,
        .animated_bar_height = animated_bar_height,
        .peak_pos = peak_pos,
        .peak_vel = peak_vel,
        .strip_color = strip_color,
        .strip_r8 = strip_r8,
        .strip_g8 = strip_g8,
        .strip_b8 = strip_b8,
        .balls = balls,
    };
    return ctx;
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
    static float filtered_bar_height[STRIP_NUM] = {0};

    for (int i = 0; i < STRIP_NUM; i++) {
        float energy = data[i];
        if (energy < 0.0f) {
            energy = 0.0f;
        }

        if (energy < BAR_ENERGY_FLOOR) {
            energy = 0.0f;
        } else {
            energy -= BAR_ENERGY_FLOOR;
        }

        if (i < 8) {
            energy *= BAR_LOW_GAIN;
        } else if (i < 18) {
            energy *= BAR_MID_GAIN;
        } else {
            energy *= BAR_HIGH_GAIN;
        }

        target_bar_height[i] = sqrtf(energy) * BAR_HEIGHT_SCALE;
    }

    for (int i = 0; i < STRIP_NUM; i++) {
        float center = target_bar_height[i];
        float left = (i > 0) ? target_bar_height[i - 1] : center;
        float right = (i < STRIP_NUM - 1) ? target_bar_height[i + 1] : center;
        float spatial = center * BAR_SPATIAL_CENTER
            + left * BAR_SPATIAL_SIDE
            + right * BAR_SPATIAL_SIDE;
        float current = filtered_bar_height[i];
        float delta = spatial - current;

        if (fabsf(delta) < BAR_JITTER_GATE) {
            spatial = current;
        }

        delta = spatial - current;
        if (delta > BAR_RISE_LIMIT) {
            spatial = current + BAR_RISE_LIMIT;
        } else if (delta < -BAR_FALL_LIMIT) {
            spatial = current - BAR_FALL_LIMIT;
        }

        if (spatial > current) {
            filtered_bar_height[i] = current * BAR_ATTACK_KEEP
                + spatial * BAR_ATTACK_APPLY;
        } else {
            filtered_bar_height[i] = current * BAR_RELEASE_KEEP
                + spatial * BAR_RELEASE_APPLY;
        }

        animated_bar_height[i] = filtered_bar_height[i];
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
    frame_counter = 0;
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
        display_style_context_t style_ctx;

        compute_bars(data);
        update_balls();
        frame_counter++;
        style_ctx = make_style_context();
        display_render_style(current_style, &style_ctx);
    }

    draw_balls();

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

    build_strip_colors();
    init_balls();

    ESP_LOGI(TAG, "Display buffer ready: %dx%d", LCD_WIDTH, LCD_HEIGHT);
    return ESP_OK;
}
