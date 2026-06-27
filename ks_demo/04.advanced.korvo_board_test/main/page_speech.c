/**
 * @file page_speech.c
 * @brief 语音识别页面实现
 *
 * 显示语音识别功能说明、唤醒词和可用命令词列表。
 * 语音识别在后台持续运行，用户说出唤醒词后可使用命令。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "page_speech.h"
#include "page_manager.h"
#include "ksdiy_lvgl_port.h"
#include "esp_log.h"
#include "lvgl.h"

/** @brief 日志标签 */
static const char *TAG = "page_speech";

/** @brief 声明自定义中文字体 */
LV_FONT_DECLARE(myFont);

/**
 * @brief 返回按钮点击事件回调
 *
 * @param e LVGL事件对象
 */
static void back_btn_cb(lv_event_t *e)
{
    page_manager_go_back();
}

/**
 * @brief 创建语音识别页面
 *
 * 显示语音唤醒词、使用说明和可用命令词列表。
 */
static void page_speech_create(void)
{
    lv_obj_t *scr = lv_scr_act();

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
    lv_label_set_text(title_label, LV_SYMBOL_VOLUME_MAX " 语音识别");
    lv_obj_set_style_text_font(title_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* ===== 提示信息 ===== */
    lv_obj_t *label_hint = lv_label_create(scr);
    lv_label_set_recolor(label_hint, true);
    lv_label_set_long_mode(label_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label_hint, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_width(label_hint, 260);
    lv_label_set_text(label_hint,
        "#3333FF 唤醒词:#\n"
        "  \"嗨，乐鑫\"\n"
        "\n"
        "#3333FF 使用方法:#\n"
        "  先说唤醒词，听到提示音后说命令\n"
        "\n"
        "#3333FF 可用命令:#\n"
        "  打开空调 / 关闭空调\n"
        "  增大风速 / 减小风速\n"
        "  升高一度 / 降低一度\n"
        "  制热模式 / 制冷模式\n"
        "  送风模式 / 节能模式\n"
        "  除湿模式 / 健康模式\n"
        "  睡眠模式\n"
        "  打开蓝牙 / 关闭蓝牙\n"
        "  开始播放 / 暂停播放\n"
        "  定时一小时\n"
        "  打开电灯 / 关闭电灯"
    );
    lv_obj_align(label_hint, LV_ALIGN_TOP_LEFT, 10, 42);

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

    ESP_LOGI(TAG, "语音识别页面已创建");
}

/**
 * @brief 销毁语音识别页面
 */
static void page_speech_destroy(void)
{
    ESP_LOGI(TAG, "语音识别页面已销毁");
}

/** @brief 语音识别页面实例 */
page_t page_speech = {
    .create = page_speech_create,
    .destroy = page_speech_destroy,
    .name = "语音识别",
    .icon = LV_SYMBOL_VOLUME_MAX,
};
