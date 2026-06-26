/**
 * @file display_styles.h
 * @brief 频谱风格渲染模块对外接口
 *
 * 这个文件负责定义“风格渲染上下文”。
 * display.c 负责准备好频谱数据、动画状态和画布缓冲区，
 * 然后把这些公共资源打包进上下文结构，再交给这里声明的风格模块去绘制。
 *
 * 这样拆分以后：
 * 1. display.c 只关心主流程和调度；
 * 2. display_styles.c 只关心每种视觉风格怎么画；
 * 3. 以后新增风格时，只需要在风格文件里加实现并在调度函数里挂上即可。
 */

#pragma once

#include <stdint.h>

#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 彩球粒子数据
 *
 * 某些风格会复用这些漂浮彩球，例如星座风格、背景粒子等。
 */
typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    lv_color_t color;
} display_ball_t;

/**
 * @brief 风格渲染上下文
 *
 * 这里集中放风格绘制所需的公共只读数据和可写目标缓冲区。
 * 每种风格都通过 ctx 访问统一的数据，不再直接依赖 display.c 内部静态变量。
 */
typedef struct {
    int lcd_width;
    int lcd_height;
    int group_width;
    int strip_width;
    int strip_num;
    int base_bar_height;
    int peak_gap;
    int ball_count;
    uint32_t frame_counter;

    lv_color_t *display_buffer;
    const float *animated_bar_height;
    float *peak_pos;
    float *peak_vel;
    const lv_color_t *strip_color;
    const uint8_t *strip_r8;
    const uint8_t *strip_g8;
    const uint8_t *strip_b8;
    const display_ball_t *balls;
} display_style_context_t;

/**
 * @brief 按指定风格把图像绘制到缓冲区
 *
 * @param style 当前选中的风格
 * @param ctx   公共渲染上下文
 */
void display_render_style(display_style_t style, const display_style_context_t *ctx);

#ifdef __cplusplus
}
#endif
