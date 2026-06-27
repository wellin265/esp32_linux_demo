/**
 * @file page_button.c
 * @brief ADC按键测试页面实现
 *
 * 实现ADC按键测试页面，实时显示6路ADC按键的事件状态。
 * 使用 esp_timer 周期性轮询按键状态变量并更新LVGL标签。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "page_button.h"
#include "page_manager.h"
#include "app_adc.h"
#include "ksdiy_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

/** @brief 日志标签 */
static const char *TAG = "page_button";

/** @brief 声明自定义中文字体 */
LV_FONT_DECLARE(myFont);

/** @brief 按键事件显示标签 */
static lv_obj_t *s_label_btn = NULL;

/** @brief 按键状态轮询定时器 */
static esp_timer_handle_t s_btn_timer = NULL;

/** @brief 按键事件历史记录标签 */
static lv_obj_t *s_label_history = NULL;

/** @brief 历史记录文本缓冲区 */
static char s_history_buf[256] = {0};

/**
 * @brief 按键状态轮询定时器回调
 *
 * 检查 Button_Value 标志，将按键事件文本更新到LVGL标签。
 *
 * @param arg 未使用
 */
static void btn_timer_cb(void *arg)
{
    if (s_label_btn != NULL && Button_Value == 1) {
        ksdiy_lvgl_lock(0);

        /* 更新当前事件显示 */
        lv_label_set_text(s_label_btn, button_str);

        /* 追加到历史记录 */
        if (strlen(s_history_buf) < 200) {
            strcat(s_history_buf, button_str);
            strcat(s_history_buf, "\n");
        } else {
            /* 缓冲区满，清空重新开始 */
            memset(s_history_buf, 0, sizeof(s_history_buf));
            strcat(s_history_buf, button_str);
            strcat(s_history_buf, "\n");
        }
        if (s_label_history != NULL) {
            lv_label_set_text(s_label_history, s_history_buf);
        }

        ksdiy_lvgl_unlock();
        Button_Value = 0;
    }
}

/**
 * @brief 返回按钮点击事件回调
 *
 * @param e LVGL事件对象
 */
static void back_btn_cb(lv_event_t *e)
{
    if (s_btn_timer != NULL) {
        esp_timer_stop(s_btn_timer);
        esp_timer_delete(s_btn_timer);
        s_btn_timer = NULL;
    }
    page_manager_go_back();
}

/**
 * @brief 创建按键测试页面
 *
 * 构建按键测试界面，包括：
 * - 绿色标题栏（按键测试）
 * - 操作提示文字
 * - 实时事件显示标签
 * - 历史记录显示区域
 * - 返回按钮
 */
static void page_button_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    memset(s_history_buf, 0, sizeof(s_history_buf));

    /* ===== 顶部标题栏 ===== */
    lv_obj_t *title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 280, 36);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x43A047), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, " 按键测试");
    lv_obj_set_style_text_font(title_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* ===== 操作提示 ===== */
    lv_obj_t *instr = lv_label_create(scr);
    lv_label_set_text(instr, "请按下开发板上的按键");
    lv_obj_set_style_text_font(instr, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(instr, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_align(instr, LV_ALIGN_TOP_LEFT, 10, 42);

    /* ===== 当前事件显示 ===== */
    s_label_btn = lv_label_create(scr);
    lv_label_set_text(s_label_btn, "等待按键...");
    lv_obj_set_style_text_font(s_label_btn, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_btn, lv_color_hex(0xE53935), LV_PART_MAIN);
    lv_obj_align(s_label_btn, LV_ALIGN_TOP_LEFT, 10, 65);

    /* ===== 历史记录区域 ===== */
    lv_obj_t *hist_title = lv_label_create(scr);
    lv_label_set_text(hist_title, "事件记录:");
    lv_obj_set_style_text_font(hist_title, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(hist_title, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_align(hist_title, LV_ALIGN_TOP_LEFT, 10, 90);

    s_label_history = lv_label_create(scr);
    lv_label_set_text(s_label_history, "");
    lv_obj_set_style_text_font(s_label_history, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_history, lv_color_hex(0x1565C0), LV_PART_MAIN);
    lv_obj_align(s_label_history, LV_ALIGN_TOP_LEFT, 10, 112);
    lv_obj_set_size(s_label_history, 260, 80);
    lv_label_set_long_mode(s_label_history, LV_LABEL_LONG_WRAP);

    /* ===== 返回按钮 ===== */
    lv_obj_t *btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, 80, 32);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_back, 8, LV_PART_MAIN);

    lv_obj_t *btn_label = lv_label_create(btn_back);
    lv_label_set_text(btn_label, "返回");
    lv_obj_set_style_text_font(btn_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn_back, back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* ===== 启动按键轮询定时器 ===== */
    esp_timer_create_args_t timer_args = {
        .callback = btn_timer_cb,
        .name = "btn_timer"
    };
    esp_timer_create(&timer_args, &s_btn_timer);
    esp_timer_start_periodic(s_btn_timer, 200000); /* 200ms 轮询 */

    ESP_LOGI(TAG, "按键测试页面已创建");
}

/**
 * @brief 销毁按键测试页面
 */
static void page_button_destroy(void)
{
    if (s_btn_timer != NULL) {
        esp_timer_stop(s_btn_timer);
        esp_timer_delete(s_btn_timer);
        s_btn_timer = NULL;
    }
    s_label_btn = NULL;
    s_label_history = NULL;
    ESP_LOGI(TAG, "按键测试页面已销毁");
}

/** @brief 按键测试页面实例 */
page_t page_button = {
    .create = page_button_create,
    .destroy = page_button_destroy,
    .name = "按键测试",
    .icon = LV_SYMBOL_SETTINGS,
};
