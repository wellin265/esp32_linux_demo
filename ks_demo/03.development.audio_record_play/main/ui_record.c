/**
 * @file ui_record.c
 * @brief 音频录制播放器 LVGL 用户界面
 *
 * 本文件实现音频录制播放器的图形用户界面：
 * - 录制按钮：开始/停止录制
 * - 播放按钮：播放录制的音频
 * - 状态显示：当前操作状态
 *
 * 界面布局：
 * ┌────────────────────────┐
 * │       状态显示         │
 * │                        │
 * │    [录制]   [播放]     │
 * │                        │
 * │     等待操作...        │
 * └────────────────────────┘
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#include "ui_record.h"
#include "lvgl.h"

/** @brief 外部变量声明 - 工作模式 */
extern record_mode_t mode;

/* ===== 外部函数声明 ===== */

/**
 * @brief 设置当前工作模式
 * @param new_mode 新的工作模式
 */
extern void set_mode(record_mode_t new_mode);

/**
 * @brief 获取当前工作模式
 * @return 当前工作模式
 */
extern record_mode_t get_mode(void);

/* ===== UI 控件句柄 ===== */

/** @brief 录制按钮对象 */
static lv_obj_t *btn_record;

/** @brief 播放按钮对象 */
static lv_obj_t *btn_play;

/** @brief 状态标签对象 */
static lv_obj_t *label_status;

/**
 * @brief 录制按钮事件回调
 *
 * 当录制按钮被点击时调用：
 * - 如果当前是空闲状态，开始录制
 * - 如果当前是录制状态，停止录制
 *
 * @param e 事件对象指针
 */
static void btn_record_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        record_mode_t current_mode = get_mode();

        if (current_mode == idle)  /* 空闲状态 */
        {
            /* 开始录制 */
            set_mode(record);
            lv_label_set_text(label_status, "Recording...");
            lv_obj_add_state(btn_record, LV_STATE_PRESSED);
        }
        else if (current_mode == record)  /* 录制状态 */
        {
            /* 停止录制 */
            set_mode(idle);
            lv_label_set_text(label_status, "Record stopped");
            lv_obj_clear_state(btn_record, LV_STATE_PRESSED);
        }
    }
}

/**
 * @brief 播放按钮事件回调
 *
 * 当播放按钮被点击时调用：
 * - 如果当前是空闲状态且有录制数据，开始播放
 * - 如果当前是播放状态，停止播放
 *
 * @param e 事件对象指针
 */
static void btn_play_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        record_mode_t current_mode = get_mode();

        if (current_mode == idle)  /* 空闲状态 */
        {
            /* 开始播放 */
            set_mode(play);
            lv_label_set_text(label_status, "Playing...");
            lv_obj_add_state(btn_play, LV_STATE_PRESSED);
        }
        else if (current_mode == play)  /* 播放状态 */
        {
            /* 停止播放 */
            set_mode(idle);
            lv_label_set_text(label_status, "Play stopped");
            lv_obj_clear_state(btn_play, LV_STATE_PRESSED);
        }
    }
}

/**
 * @brief 创建录制按钮
 *
 * 创建一个带有录制图标的按钮。
 *
 * @param parent 父对象
 * @return lv_obj_t* 按钮对象指针
 */
static lv_obj_t *create_record_button(lv_obj_t *parent)
{
    /* 创建按钮 */
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 80, 80);

    /* 设置圆形样式 */
    lv_obj_set_style_radius(btn, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x880000), LV_STATE_PRESSED);

    /* 添加录制图标 */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_STOP);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(label);

    /* 添加事件回调 */
    lv_obj_add_event_cb(btn, btn_record_event_cb, LV_EVENT_ALL, NULL);

    return btn;
}

/**
 * @brief 创建播放按钮
 *
 * 创建一个带有播放图标的按钮。
 *
 * @param parent 父对象
 * @return lv_obj_t* 按钮对象指针
 */
static lv_obj_t *create_play_button(lv_obj_t *parent)
{
    /* 创建按钮 */
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 80, 80);

    /* 设置圆形样式 */
    lv_obj_set_style_radius(btn, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA00), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x006600), LV_STATE_PRESSED);

    /* 添加播放图标 */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(label);

    /* 添加事件回调 */
    lv_obj_add_event_cb(btn, btn_play_event_cb, LV_EVENT_ALL, NULL);

    return btn;
}

/**
 * @brief 创建音频录制界面
 *
 * 创建完整的音频录制播放界面：
 * 1. 状态显示标签
 * 2. 录制按钮（红色圆形）
 * 3. 播放按钮（绿色圆形）
 *
 * 界面使用水平布局排列按钮。
 */
void ui_record(void)
{
    /* ===== 创建状态标签 ===== */
    label_status = lv_label_create(lv_scr_act());
    lv_label_set_text(label_status, "Ready");
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_20, LV_STATE_DEFAULT);

    /* ===== 创建按钮容器 ===== */
    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 280, 120);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ===== 创建录制按钮 ===== */
    btn_record = create_record_button(cont);

    /* ===== 创建播放按钮 ===== */
    btn_play = create_play_button(cont);

    /* ===== 创建提示标签 ===== */
    lv_obj_t *label_hint = lv_label_create(lv_scr_act());
    lv_label_set_text(label_hint, "Tap RED to record, GREEN to play");
    lv_obj_align(label_hint, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_text_font(label_hint, &lv_font_montserrat_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0x888888), LV_STATE_DEFAULT);
}
