/**
 * @file display_styles.c
 * @brief FFT 频谱多风格渲染实现
 *
 * 这里专门放“视觉风格”相关的代码。
 * 设计目标是让每种风格都能独立阅读、独立维护、独立扩展。
 *
 * 阅读建议：
 * 1. 先看最底部的 display_render_style()，了解调度入口；
 * 2. 再看上面的公共小工具函数；
 * 3. 最后按 render_xxx() 逐个看每一种风格的实现。
 */

#include <math.h>
#include <stdlib.h>

#include "display_styles.h"

/**
 * @brief 把浮点值夹紧到 0~1 范围
 *
 * 很多透明度、混合系数、发光强度都需要保证在这个区间内。
 */
static float clamp_unit(float v)
{
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

/**
 * @brief 按比例缩放 LVGL 透明度
 *
 * @param opa    原始透明度
 * @param factor 缩放比例，建议范围 0~1
 * @return 缩放后的透明度
 */
static lv_opa_t scaled_opa(lv_opa_t opa, float factor)
{
    float scaled = (float)opa * clamp_unit(factor);
    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    if (scaled > 255.0f) {
        scaled = 255.0f;
    }
    return (lv_opa_t)scaled;
}

/**
 * @brief 把颜色按比例压暗
 *
 * 常用于生成辉光、阴影、远景和弱化版本的颜色。
 */
static lv_color_t dim_color(lv_color_t color, float factor)
{
    factor = clamp_unit(factor);
    lv_color32_t rgb;
    rgb.full = lv_color_to32(color);
    return lv_color_make((uint8_t)(LV_COLOR_GET_R32(rgb) * factor),
                         (uint8_t)(LV_COLOR_GET_G32(rgb) * factor),
                         (uint8_t)(LV_COLOR_GET_B32(rgb) * factor));
}

/**
 * @brief 以 Alpha 混合方式向目标缓冲区写一个像素
 *
 * 这是风格层最常用的底层接口。所有柔光、线条、发光点，最终都会落到这里。
 */
static void blend_pixel(const display_style_context_t *ctx, int x, int y, lv_color_t color, lv_opa_t opa)
{
    if (x < 0 || x >= ctx->lcd_width || y < 0 || y >= ctx->lcd_height || opa <= LV_OPA_MIN) {
        return;
    }

    lv_color_t *dst = &ctx->display_buffer[y * ctx->lcd_width + x];
    *dst = lv_color_mix(color, *dst, opa);
}

/**
 * @brief 计算当前整帧的平均能量水平
 *
 * 用于让某些全局特效随整体音乐强度联动，例如：
 * - 星座连线范围
 * - 雷达扫光长度
 * - 圆环内核光晕强度
 */
static float average_bar_level(const display_style_context_t *ctx)
{
    float sum = 0.0f;
    for (int i = 0; i < ctx->strip_num; i++) {
        sum += ctx->animated_bar_height[i];
    }
    return clamp_unit(sum / ((float)ctx->strip_num * 70.0f));
}

/**
 * @brief 在缓冲区里画一条带少量厚度的软线
 *
 * 这里不是严格意义上的抗锯齿线，而是通过对线段周围少量像素做覆盖，
 * 得到更柔和、更适合视觉频谱的“发光线”效果。
 */
static void draw_line_soft(const display_style_context_t *ctx,
                           int x0, int y0, int x1, int y1,
                           lv_color_t color, lv_opa_t opa, int thickness)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int radius = thickness > 1 ? thickness / 2 : 0;

    while (1) {
        for (int oy = -radius; oy <= radius; oy++) {
            for (int ox = -radius; ox <= radius; ox++) {
                blend_pixel(ctx, x0 + ox, y0 + oy, color, opa);
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief 绘制彩球之间的动态连线
 *
 * 这个效果主要给星座风格用。音乐能量越高，可连线距离越远，
 * 于是画面会变得更“密”、更活跃。
 */
static void render_ball_links(const display_style_context_t *ctx, float level)
{
    float link_limit = 32.0f + level * 54.0f;
    float link_limit_sq = link_limit * link_limit;

    for (int i = 0; i < ctx->ball_count; i++) {
        for (int j = i + 1; j < ctx->ball_count; j++) {
            float dx = ctx->balls[j].x - ctx->balls[i].x;
            float dy = ctx->balls[j].y - ctx->balls[i].y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq > link_limit_sq) {
                continue;
            }

            float dist = sqrtf(dist_sq);
            float near_t = 1.0f - dist / link_limit;
            lv_color_t link_color = lv_color_mix(ctx->balls[i].color, ctx->balls[j].color, LV_OPA_50);
            lv_opa_t link_opa = scaled_opa(LV_OPA_70, 0.25f + near_t * 0.75f);
            int thickness = near_t > 0.65f ? 2 : 1;

            draw_line_soft(ctx,
                           (int)lroundf(ctx->balls[i].x), (int)lroundf(ctx->balls[i].y),
                           (int)lroundf(ctx->balls[j].x), (int)lroundf(ctx->balls[j].y),
                           link_color, link_opa, thickness);
        }
    }
}

/**
 * @brief 更新峰值保持位置
 *
 * 经典柱状和镜像柱状都会用到“峰值帽”效果：
 * 当当前柱高变高时，峰值立刻跟上；
 * 当当前柱高下降时，峰值以重力感的速度缓慢下落。
 */
static void update_peaks(const display_style_context_t *ctx, const int *bar_heights)
{
    for (int i = 0; i < ctx->strip_num; i++) {
        float bar_top = (float)bar_heights[i] + ctx->peak_gap;

        if (bar_top >= ctx->peak_pos[i]) {
            ctx->peak_pos[i] = bar_top;
            ctx->peak_vel[i] = 0.0f;
        } else {
            ctx->peak_vel[i] += 1.8f;
            ctx->peak_pos[i] -= ctx->peak_vel[i];
            if (ctx->peak_pos[i] < bar_top) {
                ctx->peak_pos[i] = bar_top;
                ctx->peak_vel[i] = 0.0f;
            }
        }
    }
}

/**
 * @brief 经典柱状频谱
 *
 * 这是最传统、最直接的样式：
 * - 每个频段对应一根竖条
 * - 竖条从底部向上增长
 * - 顶部有峰值保持线
 */
static void render_classic(const display_style_context_t *ctx)
{
    int bar_heights[ctx->strip_num];

    for (int i = 0; i < ctx->strip_num; i++) {
        bar_heights[i] = ctx->base_bar_height + (int)ctx->animated_bar_height[i];
        if (bar_heights[i] > ctx->lcd_height) {
            bar_heights[i] = ctx->lcd_height;
        }
    }

    update_peaks(ctx, bar_heights);

    for (int i = 0; i < ctx->strip_num; i++) {
        const int x_start = i * ctx->group_width;
        int bar_h = bar_heights[i];
        lv_color_t col = ctx->strip_color[i];
        int peak_row = ctx->lcd_height - (int)ctx->peak_pos[i];

        for (int dx = 0; dx < ctx->strip_width; dx++) {
            int x = x_start + dx;
            if (x >= ctx->lcd_width) {
                break;
            }

            for (int y = 0; y < bar_h; y++) {
                ctx->display_buffer[(ctx->lcd_height - 1 - y) * ctx->lcd_width + x] = col;
            }

            if (peak_row >= 0 && peak_row < ctx->lcd_height) {
                ctx->display_buffer[peak_row * ctx->lcd_width + x] = col;
            }
        }
    }
}

/**
 * @brief 镜像倒影频谱
 *
 * 上半部分是主频谱，下半部分是压缩后的倒影，
 * 模拟水面、玻璃倒影之类的视觉语言。
 */
static void render_mirror(const display_style_context_t *ctx)
{
    const int mirror_gap = 3;
    const int upper_base = ctx->lcd_height / 2 - mirror_gap / 2;
    const int lower_base = ctx->lcd_height / 2 + (mirror_gap + 1) / 2;
    const int max_upper = upper_base - ctx->base_bar_height;
    const float scale = (float)max_upper / (float)(ctx->lcd_height - ctx->base_bar_height);
    int bar_heights[ctx->strip_num];

    for (int i = 0; i < ctx->strip_num; i++) {
        bar_heights[i] = ctx->base_bar_height + (int)(ctx->animated_bar_height[i] * scale);
        if (bar_heights[i] > upper_base) {
            bar_heights[i] = upper_base;
        }
    }

    update_peaks(ctx, bar_heights);

    for (int i = 0; i < ctx->strip_num; i++) {
        const int x_start = i * ctx->group_width;
        int bar_h = bar_heights[i];
        int reflect_h = (bar_h + 2) / 3;
        lv_color_t col = ctx->strip_color[i];
        lv_color_t col_dim = lv_color_make(ctx->strip_r8[i] / 2, ctx->strip_g8[i] / 2, ctx->strip_b8[i] / 2);
        int peak_up_row = upper_base - (int)ctx->peak_pos[i];
        int reflect_peak = (int)ctx->peak_pos[i] / 3;
        int peak_dn_row = lower_base + reflect_peak;

        for (int dx = 0; dx < ctx->strip_width; dx++) {
            int x = x_start + dx;
            if (x >= ctx->lcd_width) {
                break;
            }

            for (int y = 0; y < bar_h; y++) {
                int row = upper_base - 1 - y;
                if (row >= 0) {
                    ctx->display_buffer[row * ctx->lcd_width + x] = col;
                }
            }

            for (int y = 0; y < reflect_h; y++) {
                int row = lower_base + y;
                if (row < ctx->lcd_height) {
                    ctx->display_buffer[row * ctx->lcd_width + x] = col_dim;
                }
            }

            if (peak_up_row >= 0 && peak_up_row < upper_base) {
                ctx->display_buffer[peak_up_row * ctx->lcd_width + x] = col;
            }
            if (peak_dn_row >= lower_base && peak_dn_row < ctx->lcd_height) {
                ctx->display_buffer[peak_dn_row * ctx->lcd_width + x] = col_dim;
            }
        }
    }
}

/**
 * @brief 极光风格
 *
 * 通过主柱 + 两侧柔雾 + 顶部高光，做出像极光幕布一样的柔和彩带感。
 */
static void render_aurora(const display_style_context_t *ctx)
{
    int bar_heights[40];

    for (int i = 0; i < ctx->strip_num; i++) {
        bar_heights[i] = 12 + (int)(ctx->animated_bar_height[i] * 1.45f);
        if (bar_heights[i] > ctx->lcd_height) {
            bar_heights[i] = ctx->lcd_height;
        }
    }

    for (int i = 0; i < ctx->strip_num; i++) {
        const int x_start = i * ctx->group_width;
        int bar_h = bar_heights[i];
        lv_color_t core = ctx->strip_color[i];
        lv_color_t haze = dim_color(core, 0.55f);

        for (int dx = -1; dx <= ctx->strip_width; dx++) {
            int x = x_start + dx;
            if (x < 0 || x >= ctx->lcd_width) {
                continue;
            }

            int local_bar = (dx < 0 || dx >= ctx->strip_width) ? bar_h * 3 / 4 : bar_h;

            for (int y = 0; y < local_bar; y++) {
                int row = ctx->lcd_height - 1 - y;
                float t = (float)y / (float)local_bar;
                float fade = 1.0f - t;

                if (dx < 0 || dx >= ctx->strip_width) {
                    blend_pixel(ctx, x, row, haze, scaled_opa(LV_OPA_60, fade * 0.7f));
                } else {
                    blend_pixel(ctx, x, row, core, scaled_opa(LV_OPA_80, 0.25f + fade * 0.75f));
                }
            }
        }

        int crest = ctx->lcd_height - bar_h;
        if (crest >= 1 && crest < ctx->lcd_height - 1) {
            lv_color_t sparkle = lv_color_mix(lv_color_white(), core, LV_OPA_30);
            for (int x = x_start; x < x_start + ctx->strip_width && x < ctx->lcd_width; x++) {
                blend_pixel(ctx, x, crest, sparkle, LV_OPA_70);
            }
        }
    }
}

/**
 * @brief 星座风格
 *
 * 画面底部保留较轻的柱体作为频谱基座，
 * 上层则让彩球在空间中漂浮并根据距离自动连线。
 */
static void render_constellation(const display_style_context_t *ctx)
{
    float level = average_bar_level(ctx);

    for (int i = 0; i < ctx->strip_num; i++) {
        const int x_start = i * ctx->group_width;
        int pillar_h = 8 + (int)(ctx->animated_bar_height[i] * (0.25f + level * 0.25f));
        lv_color_t col = dim_color(ctx->strip_color[i], 0.65f);

        for (int dx = 1; dx < ctx->strip_width - 1; dx++) {
            int x = x_start + dx;
            if (x >= ctx->lcd_width) {
                break;
            }

            for (int y = 0; y < pillar_h; y++) {
                int row = ctx->lcd_height - 1 - y;
                float fade = 1.0f - ((float)y / (float)pillar_h);
                blend_pixel(ctx, x, row, col, scaled_opa(LV_OPA_40, fade));
            }
        }
    }

    render_ball_links(ctx, level);
}

/**
 * @brief 梦幻波带风格
 *
 * 把每个频段看成上下边界的一对采样点，然后在相邻频段之间插值填充，
 * 最终得到一条漂浮的彩色丝带。
 */
static void render_dream_wave(const display_style_context_t *ctx)
{
    const int center_y = ctx->lcd_height / 2;
    int prev_x = 0;
    int prev_top = center_y;
    int prev_bottom = center_y;

    for (int i = 0; i < ctx->strip_num; i++) {
        int x = i * ctx->group_width + ctx->strip_width / 2;
        float amp = 10.0f + ctx->animated_bar_height[i] * 0.33f;
        int top = center_y - (int)amp;
        int bottom = center_y + (int)amp;
        lv_color_t ribbon = ctx->strip_color[i];
        lv_color_t haze = dim_color(ribbon, 0.45f);

        if (i > 0) {
            int span = x - prev_x;
            if (span < 1) {
                span = 1;
            }

            for (int step = 0; step <= span; step++) {
                float t = (float)step / (float)span;
                int xi = prev_x + step;
                int yi_top = (int)lroundf(prev_top + (top - prev_top) * t);
                int yi_bottom = (int)lroundf(prev_bottom + (bottom - prev_bottom) * t);

                for (int y = yi_top; y <= yi_bottom; y++) {
                    float fill_t = (yi_bottom == yi_top) ? 0.0f :
                                   (float)(y - yi_top) / (float)(yi_bottom - yi_top);
                    lv_color_t fill = fill_t < 0.5f ? haze : ribbon;
                    lv_opa_t opa = fill_t < 0.5f ? LV_OPA_30 : LV_OPA_40;
                    blend_pixel(ctx, xi, y, fill, opa);
                }
            }

            draw_line_soft(ctx, prev_x, prev_top, x, top,
                           lv_color_mix(lv_color_white(), ribbon, LV_OPA_30), LV_OPA_90, 1);
            draw_line_soft(ctx, prev_x, prev_bottom, x, bottom, ribbon, LV_OPA_80, 1);
        }

        prev_x = x;
        prev_top = top;
        prev_bottom = bottom;
    }
}

/**
 * @brief 圆环向外绽放频谱
 *
 * 这是一个“圆形向外长”的风格：
 * - 中间有发光圆核
 * - 每个频段对应圆周上的一个角度
 * - 柱条沿半径方向向外延伸
 * - 外圈有亮点，强化音乐节奏感
 */
static void render_radial_bloom(const display_style_context_t *ctx)
{
    const int cx = ctx->lcd_width / 2;
    const int cy = ctx->lcd_height / 2;
    const float inner_r = 30.0f;
    const float ring_r = 26.0f;
    const int spoke_step = 2;
    const int spoke_count = (ctx->strip_num + spoke_step - 1) / spoke_step;
    float level = average_bar_level(ctx);
    float spin = ctx->frame_counter * 0.018f;
    lv_color_t core = lv_color_make(90, 40, 180);
    lv_color_t halo = lv_color_make(60, 180, 255);

    for (int y = -34; y <= 34; y++) {
        for (int x = -34; x <= 34; x++) {
            float dist_sq = (float)(x * x + y * y);
            if (dist_sq > 34.0f * 34.0f) {
                continue;
            }

            float dist = sqrtf(dist_sq);
            if (dist <= ring_r) {
                float t = 1.0f - dist / ring_r;
                blend_pixel(ctx, cx + x, cy + y, core, scaled_opa(LV_OPA_70, 0.35f + t * 0.65f));
            } else {
                float ring_t = 1.0f - (dist - ring_r) / 8.0f;
                blend_pixel(ctx, cx + x, cy + y, halo, scaled_opa(LV_OPA_30, ring_t));
            }
        }
    }

    for (int spoke = 0; spoke < spoke_count; spoke++) {
        int i0 = spoke * spoke_step;
        int i1 = i0 + 1;
        float h0 = ctx->animated_bar_height[i0];
        float h1 = (i1 < ctx->strip_num) ? ctx->animated_bar_height[i1] : h0;
        float bar_height = h0 > h1 ? h0 : h1;
        float t = (float)spoke / (float)spoke_count;
        float angle = -1.5707963f + t * 6.2831853f + spin;
        float len = 10.0f + bar_height * 0.88f;
        float width_bias = 0.45f + 0.55f * (1.0f - fabsf(t * 2.0f - 1.0f));
        lv_color_t col = (i1 < ctx->strip_num)
            ? lv_color_mix(ctx->strip_color[i0], ctx->strip_color[i1], LV_OPA_50)
            : ctx->strip_color[i0];
        lv_color_t glow = lv_color_mix(lv_color_white(), col, LV_OPA_30);
        int thickness = 4;
        if (bar_height > 22.0f) {
            thickness = 5;
        }
        if (bar_height > 42.0f) {
            thickness = 5;
        }
        float outer_r = inner_r + len;
        int x0 = cx + (int)lroundf(cosf(angle) * inner_r);
        int y0 = cy + (int)lroundf(sinf(angle) * inner_r);
        int x1 = cx + (int)lroundf(cosf(angle) * outer_r);
        int y1 = cy + (int)lroundf(sinf(angle) * outer_r);

        draw_line_soft(ctx, x0, y0, x1, y1, dim_color(col, 0.58f), LV_OPA_40, thickness + 1);
        draw_line_soft(ctx, x0, y0, x1, y1, glow, scaled_opa(LV_OPA_80, 0.65f + width_bias * 0.35f), thickness - 1);

        if (bar_height > 6.0f) {
            int px = cx + (int)lroundf(cosf(angle) * (outer_r + 1.0f));
            int py = cy + (int)lroundf(sinf(angle) * (outer_r + 1.0f));
            lv_opa_t peak_opa = scaled_opa(LV_OPA_90, 0.30f + bar_height / 75.0f);
            blend_pixel(ctx, px, py, lv_color_white(), peak_opa);
            blend_pixel(ctx, px + 1, py, col, LV_OPA_70);
            blend_pixel(ctx, px - 1, py, col, LV_OPA_70);
            blend_pixel(ctx, px, py + 1, col, LV_OPA_70);
            blend_pixel(ctx, px, py - 1, col, LV_OPA_70);
        }
    }

    if (level > 0.08f) {
        for (int i = 0; i < 18; i++) {
            float angle = spin * 2.4f + i * (6.2831853f / 18.0f);
            float flare_r = inner_r - 8.0f + sinf(ctx->frame_counter * 0.05f + i) * 4.0f;
            int x = cx + (int)lroundf(cosf(angle) * flare_r);
            int y = cy + (int)lroundf(sinf(angle) * flare_r);
            blend_pixel(ctx, x, y, halo, scaled_opa(LV_OPA_60, level));
        }
    }
}

/**
 * @brief 雷达扫描风格
 *
 * 画面下方是一个雷达原点，频谱会沿扇形角度打出去，
 * 同时有扫光在扇区内来回摆动。
 */
static void render_radar(const display_style_context_t *ctx)
{
    const int origin_x = ctx->lcd_width / 2;
    const int origin_y = ctx->lcd_height + 26;
    float sweep = -2.55f + sinf(ctx->frame_counter * 0.05f) * 0.95f;
    float level = average_bar_level(ctx);
    lv_color_t grid_color = lv_color_make(20, 96, 72);
    lv_color_t arc_glow = lv_color_make(48, 180, 128);

    for (int r = 48; r <= 212; r += 34) {
        for (int i = 0; i < ctx->strip_num; i++) {
            float t = (float)i / (float)(ctx->strip_num - 1);
            float angle = -2.85f + t * 2.55f;
            int x = origin_x + (int)lroundf(cosf(angle) * r);
            int y = origin_y + (int)lroundf(sinf(angle) * r);
            blend_pixel(ctx, x, y, grid_color, LV_OPA_50);
            if ((i & 1) == 0) {
                blend_pixel(ctx, x, y, arc_glow, LV_OPA_20);
            }
        }
    }

    for (int i = 0; i < ctx->strip_num; i++) {
        float t = (float)i / (float)(ctx->strip_num - 1);
        float angle = -2.8f + t * 2.5f;
        float amplitude = 24.0f + ctx->animated_bar_height[i] * 1.55f;
        lv_color_t beam = ctx->strip_color[i];
        lv_color_t soft = dim_color(beam, 0.50f);
        lv_color_t beam_glow = lv_color_mix(lv_color_white(), beam, LV_OPA_20);
        int x1 = origin_x + (int)lroundf(cosf(angle) * amplitude);
        int y1 = origin_y + (int)lroundf(sinf(angle) * amplitude);

        draw_line_soft(ctx, origin_x, origin_y, x1, y1, soft, LV_OPA_30, 3);
        draw_line_soft(ctx, origin_x, origin_y, x1, y1, beam, LV_OPA_70, 2);
        draw_line_soft(ctx, origin_x, origin_y, x1, y1, beam_glow, LV_OPA_80, 1);

        if (ctx->animated_bar_height[i] > 10.0f) {
            lv_opa_t ping_opa = scaled_opa(LV_OPA_90, 0.35f + ctx->animated_bar_height[i] / 80.0f);
            blend_pixel(ctx, x1, y1, lv_color_white(), ping_opa);
            blend_pixel(ctx, x1 + 1, y1, beam, LV_OPA_70);
            blend_pixel(ctx, x1 - 1, y1, beam, LV_OPA_70);
            blend_pixel(ctx, x1, y1 + 1, beam, LV_OPA_70);
            blend_pixel(ctx, x1, y1 - 1, beam, LV_OPA_70);
        }

        float sweep_diff = fabsf(angle - sweep);
        if (sweep_diff < 0.10f) {
            float intensity = 1.0f - sweep_diff / 0.10f;
            int sweep_len = 42 + (int)(level * 120.0f);
            int sx = origin_x + (int)lroundf(cosf(angle) * sweep_len);
            int sy = origin_y + (int)lroundf(sinf(angle) * sweep_len);
            lv_color_t sweep_col = lv_color_mix(lv_color_white(), beam, LV_OPA_30);
            draw_line_soft(ctx, origin_x, origin_y, sx, sy, sweep_col,
                           scaled_opa(LV_OPA_40, intensity), 5);
            draw_line_soft(ctx, origin_x, origin_y, sx, sy,
                           lv_color_mix(lv_color_white(), beam, LV_OPA_20),
                           scaled_opa(LV_OPA_90, intensity), intensity > 0.6f ? 3 : 2);
        }
    }
}

/**
 * @brief 余烬火焰风格
 *
 * 视觉重点是暖色、上涌、火星和热浪感。
 */
static void render_ember(const display_style_context_t *ctx)
{
    for (int i = 0; i < ctx->strip_num; i++) {
        const int x_start = i * ctx->group_width;
        int bar_h = 14 + (int)(ctx->animated_bar_height[i] * 1.50f);
        if (bar_h > ctx->lcd_height) {
            bar_h = ctx->lcd_height;
        }

        lv_color_t ember_red = lv_color_make(255, 72, 16);
        lv_color_t ember_gold = lv_color_make(255, 180, 32);
        lv_color_t base = lv_color_mix(ember_red, ctx->strip_color[i], LV_OPA_30);
        lv_color_t mid = lv_color_mix(ember_gold, base, LV_OPA_50);
        lv_color_t core = lv_color_mix(lv_color_white(), mid, LV_OPA_30);

        for (int dx = -1; dx <= ctx->strip_width; dx++) {
            int x = x_start + dx;
            if (x < 0 || x >= ctx->lcd_width) {
                continue;
            }

            int local_h = (dx < 0 || dx >= ctx->strip_width) ? bar_h * 2 / 3 : bar_h;
            for (int y = 0; y < local_h; y++) {
                int row = ctx->lcd_height - 1 - y;
                float t = (float)y / (float)local_h;
                float wobble = sinf(ctx->frame_counter * 0.08f + i * 0.55f + t * 5.0f) * 0.08f;
                lv_color_t col = t < 0.35f ? base : (t < 0.72f ? mid : core);
                lv_opa_t opa = scaled_opa((dx < 0 || dx >= ctx->strip_width) ? LV_OPA_30 : LV_OPA_80,
                                          (1.0f - t) + wobble + 0.15f);
                blend_pixel(ctx, x, row, col, opa);
            }
        }

        int spark_y = ctx->lcd_height - 1 - bar_h - (int)(ctx->peak_pos[i] * 0.18f);
        if (spark_y > 6 && spark_y < ctx->lcd_height - 6 && ctx->animated_bar_height[i] > 8.0f) {
            int spark_x = x_start + ctx->strip_width / 2 + (int)lroundf(sinf(ctx->frame_counter * 0.05f + i) * 2.0f);
            draw_line_soft(ctx, spark_x, spark_y, spark_x + ((i & 1) ? 2 : -2), spark_y - 4,
                           lv_color_mix(lv_color_white(), mid, LV_OPA_20), LV_OPA_80, 1);
        }
    }
}

/**
 * @brief 霓虹城市风格
 *
 * 通过高楼轮廓、窗口光点、地平线霓虹线，营造赛博夜景的感觉。
 */
static void render_neon_city(const display_style_context_t *ctx)
{
    const int horizon = ctx->lcd_height * 3 / 4;
    float pulse = 0.55f + 0.45f * sinf(ctx->frame_counter * 0.05f);
    lv_color_t haze = lv_color_make(40, 24, 84);
    lv_color_t horizon_glow = lv_color_make(255, 80, 160);

    for (int row = horizon - 28; row <= horizon + 4; row += 8) {
        if (row >= 0 && row < ctx->lcd_height) {
            for (int x = 0; x < ctx->lcd_width; x++) {
                blend_pixel(ctx, x, row, haze, LV_OPA_20);
            }
        }
    }

    for (int i = 0; i < ctx->strip_num; i++) {
        const int x_start = i * ctx->group_width;
        int bar_h = 10 + (int)(ctx->animated_bar_height[i] * 1.10f);
        int top = horizon - bar_h;
        lv_color_t edge = ctx->strip_color[i];
        lv_color_t face = dim_color(edge, 0.22f + 0.10f * pulse);
        lv_color_t glow = lv_color_mix(lv_color_white(), edge, LV_OPA_30);
        int mid_x = x_start + ctx->strip_width / 2;

        if (top < 4) {
            top = 4;
        }

        for (int dx = 0; dx < ctx->strip_width; dx++) {
            int x = x_start + dx;
            if (x >= ctx->lcd_width) {
                break;
            }

            for (int y = top; y < horizon; y++) {
                lv_color_t fill = (dx == 0 || dx == ctx->strip_width - 1) ? edge : face;
                lv_opa_t opa = (dx == 0 || dx == ctx->strip_width - 1) ? LV_OPA_70 : LV_OPA_40;
                blend_pixel(ctx, x, y, fill, opa);
            }

            if (dx == 2) {
                for (int wy = top + 6; wy < horizon - 3; wy += 10) {
                    if (((wy / 10) + i) % 2 == 0) {
                        blend_pixel(ctx, x, wy, glow, scaled_opa(LV_OPA_50, pulse));
                        blend_pixel(ctx, x, wy + 1, glow, scaled_opa(LV_OPA_30, pulse));
                    }
                }
            }
        }

        if (i > 0) {
            int prev_x = (i - 1) * ctx->group_width + ctx->strip_width / 2;
            int this_x = x_start + ctx->strip_width / 2;
            int prev_top = horizon - (10 + (int)(ctx->animated_bar_height[i - 1] * 1.10f));
            draw_line_soft(ctx, prev_x, prev_top, this_x, top, dim_color(edge, 0.80f), LV_OPA_30, 1);
        }

        if ((i % 3) != 1 && top + 3 < horizon) {
            blend_pixel(ctx, mid_x, top, lv_color_white(), LV_OPA_70);
            blend_pixel(ctx, mid_x, top + 1, glow, LV_OPA_50);
        }
    }

    for (int x = 0; x < ctx->lcd_width; x++) {
        blend_pixel(ctx, x, horizon, horizon_glow, LV_OPA_40);
    }
}

/**
 * @brief 风格调度入口
 *
 * display.c 每帧只调用这一个函数。
 * 如果以后要新增风格，只需要：
 * 1. 在 display.h 里添加新的枚举值；
 * 2. 在本文件新增对应的 render_xxx()；
 * 3. 在下面这个 switch 里挂上新分支。
 */
void display_render_style(display_style_t style, const display_style_context_t *ctx)
{
    switch (style) {
    case DISPLAY_STYLE_AURORA:
        render_aurora(ctx);
        break;
    case DISPLAY_STYLE_CONSTELLATION:
        render_constellation(ctx);
        break;
    case DISPLAY_STYLE_DREAM_WAVE:
        render_dream_wave(ctx);
        break;
    case DISPLAY_STYLE_RADIAL_BLOOM:
        render_radial_bloom(ctx);
        break;
    case DISPLAY_STYLE_RADAR:
        render_radar(ctx);
        break;
    case DISPLAY_STYLE_EMBER:
        render_ember(ctx);
        break;
    case DISPLAY_STYLE_NEON_CITY:
        render_neon_city(ctx);
        break;
    case DISPLAY_STYLE_MIRROR:
        render_mirror(ctx);
        break;
    case DISPLAY_STYLE_CLASSIC:
    default:
        render_classic(ctx);
        break;
    }
}
