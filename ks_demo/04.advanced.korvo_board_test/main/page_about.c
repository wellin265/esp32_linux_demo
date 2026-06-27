/**
 * @file page_about.c
 * @brief 系统信息页面实现
 *
 * 显示系统信息包括：
 * - 芯片型号和修订版本
 * - CPU频率和核心数
 * - 可用堆内存和最小可用内存
 * - Flash大小
 * - 编译日期和时间
 * - IDF版本
 * - I2C设备扫描结果
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "page_about.h"
#include "page_manager.h"
#include "ksdiy_lvgl_port.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

/** @brief 日志标签 */
static const char *TAG = "page_about";

/** @brief 声明自定义中文字体 */
LV_FONT_DECLARE(myFont);

/** @brief 系统信息文本标签 */
static lv_obj_t *s_label_info = NULL;

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
 * @brief 构建系统信息文本
 *
 * 收集芯片信息、内存状态、编译信息等，格式化为文本。
 *
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
static void build_sys_info(char *buf, size_t buf_size)
{
    char *p = buf;
    int len = 0;

    /* 芯片信息 */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    /* 获取Flash大小 */
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    const char *model_name = "Unknown";
    switch (chip_info.model) {
        case CHIP_ESP32:    model_name = "ESP32"; break;
        case CHIP_ESP32S2:  model_name = "ESP32-S2"; break;
        case CHIP_ESP32S3:  model_name = "ESP32-S3"; break;
        case CHIP_ESP32C3:  model_name = "ESP32-C3"; break;
        default: break;
    }

    len = snprintf(p, buf_size - (p - buf),
        "#3333FF 系统信息#\n"
        "芯片: %s Rev.%d\n"
        "CPU: %d核心 %dMHz\n"
        "Flash: %dMB %s\n"
        "IDF: %s\n"
        "\n#3333FF 内存信息#\n"
        "可用堆: %lu 字节\n"
        "最小可用: %lu 字节\n"
        "PSRAM可用: %lu 字节\n"
        "\n#3333FF 编译信息#\n"
        "日期: %s\n"
        "时间: %s\n",
        model_name, chip_info.revision,
        chip_info.cores, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        (int)(flash_size / (1024 * 1024)),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "内嵌" : "外部",
        esp_get_idf_version(),
        (unsigned long)esp_get_free_heap_size(),
        (unsigned long)esp_get_minimum_free_heap_size(),
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        __DATE__,
        __TIME__
    );
    p += len;
}

/**
 * @brief 创建系统信息页面
 *
 * 构建系统信息界面，包括：
 * - 紫色标题栏（系统信息）
 * - 滚动显示的系统信息文本
 * - 返回按钮
 */
static void page_about_create(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* ===== 顶部标题栏 ===== */
    lv_obj_t *title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 280, 36);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x8E24AA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, LV_SYMBOL_BELL " 系统信息");
    lv_obj_set_style_text_font(title_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* ===== 系统信息滚动容器 ===== */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 260, 300);
    lv_obj_align(cont, LV_ALIGN_TOP_LEFT, 10, 40);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 5, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_NONE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_ELASTIC);

    /* 系统信息文本标签 */
    s_label_info = lv_label_create(cont);
    lv_label_set_long_mode(s_label_info, LV_LABEL_LONG_WRAP);
    lv_label_set_recolor(s_label_info, true);
    lv_obj_set_style_text_font(s_label_info, &myFont, LV_PART_MAIN);
    lv_obj_set_width(s_label_info, 250);
    char info_buf[1024];
    build_sys_info(info_buf, sizeof(info_buf));
    lv_label_set_text(s_label_info, info_buf);

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

    ESP_LOGI(TAG, "系统信息页面已创建");
}

/**
 * @brief 销毁系统信息页面
 */
static void page_about_destroy(void)
{
    s_label_info = NULL;
    ESP_LOGI(TAG, "系统信息页面已销毁");
}

/** @brief 系统信息页面实例 */
page_t page_about = {
    .create = page_about_create,
    .destroy = page_about_destroy,
    .name = "系统信息",
    .icon = LV_SYMBOL_BELL,
};
