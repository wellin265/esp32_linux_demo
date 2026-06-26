/**
 * @file lvgl_demo_ui.c
 * @brief LVGL 仪表盘演示 UI
 *
 * 本示例改编自 LVGL 官方示例：
 * https://docs.lvgl.io/master/widgets/extra/meter.html#simple-meter
 *
 * 功能演示：
 * - 创建一个仪表盘控件（Meter）
 * - 添加刻度、弧线和指针指示器
 * - 使用动画驱动指针旋转
 * - 添加旋转按钮，支持屏幕方向切换
 *
 * @copyright SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"

/** @brief 仪表盘对象指针 */
static lv_obj_t *meter;

/** @brief 旋转按钮对象指针 */
static lv_obj_t * btn;

/** @brief 当前屏幕旋转角度 */
static lv_disp_rot_t rotation = LV_DISP_ROT_NONE;

/**
 * @brief 设置仪表盘指示器值
 *
 * 动画回调函数，用于更新仪表盘指针的位置。
 *
 * @param indic 指示器对象指针
 * @param v 新的指示值（0-100）
 */
static void set_value(void *indic, int32_t v)
{
    lv_meter_set_indicator_end_value(meter, indic, v);
}

/**
 * @brief 旋转按钮点击事件回调
 *
 * 每次点击按钮，屏幕旋转 90 度，循环切换四个方向。
 *
 * @param e 事件对象指针
 */
static void btn_cb(lv_event_t * e)
{
    lv_disp_t *disp = lv_event_get_user_data(e);
    rotation++;
    if (rotation > LV_DISP_ROT_270) {
        rotation = LV_DISP_ROT_NONE;
    }
    lv_disp_set_rotation(disp, rotation);
}

/**
 * @brief 创建 LVGL 仪表盘演示界面
 *
 * 创建一个包含以下元素的界面：
 * 1. 仪表盘控件：
 *    - 41 个刻度线（每 2.5 度一个）
 *    - 每 8 个小刻度有一个主刻度
 *    - 蓝色弧线表示起始区域（0-20）
 *    - 红色弧线表示警告区域（80-100）
 *    - 灰色指针
 * 2. 旋转按钮：
 *    - 点击可切换屏幕方向
 * 3. 动画效果：
 *    - 指针从 0 摆动到 100，然后回摆
 *    - 无限循环播放
 *
 * @param disp 显示设备指针
 */
void example_lvgl_demo_ui(lv_disp_t *disp)
{
    /* 获取当前活动屏幕 */
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    /* ===== 创建仪表盘 ===== */
    meter = lv_meter_create(scr);
    lv_obj_center(meter);
    lv_obj_set_size(meter, 200, 200);

    /* 添加刻度盘 */
    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    /* 设置小刻度：共 41 个，线宽 2，长度 10，灰色 */
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    /* 设置主刻度：每 8 个小刻度一个，线宽 4，长度 15，黑色，标签间隔 10 */
    lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 15, lv_color_black(), 10);

    lv_meter_indicator_t *indic;

    /* ===== 添加蓝色起始区域弧线（0-20） ===== */
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /* 添加蓝色起始区域刻度线 */
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /* ===== 添加红色警告区域弧线（80-100） ===== */
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /* 添加红色警告区域刻度线 */
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /* ===== 添加指针（灰色线条，宽度 4） ===== */
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    /* ===== 创建旋转按钮 ===== */
    btn = lv_btn_create(scr);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_REFRESH" ROTATE");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);

    /* 注册按钮点击事件 */
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, disp);

    /* ===== 创建指针动画 ===== */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_value);      // 设置动画回调函数
    lv_anim_set_var(&a, indic);               // 动画变量（指示器）
    lv_anim_set_values(&a, 0, 100);           // 动画值范围
    lv_anim_set_time(&a, 2000);               // 正向动画时间：2 秒
    lv_anim_set_repeat_delay(&a, 100);        // 重复延迟：100ms
    lv_anim_set_playback_time(&a, 500);       // 回放时间：500ms
    lv_anim_set_playback_delay(&a, 100);      // 回放延迟：100ms
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);  // 无限循环
    lv_anim_start(&a);
}
