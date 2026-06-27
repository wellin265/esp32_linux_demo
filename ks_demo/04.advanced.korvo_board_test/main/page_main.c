/**
 * @file page_main.c
 * @brief 主菜单页面实现
 *
 * 实现主菜单页面的创建、销毁和刷新功能。
 * 使用 2 列网格布局，每个测试项显示为带图标的彩色按钮，
 * 适配 280x240 分辨率屏幕。
 *
 * 布局说明：
 *   - 顶部标题栏：40px 高，显示"酷世DIY 测试"
 *   - 内容区域：2 列网格，每行 2 个按钮
 *   - 每个按钮：120x55，包含图标和中文标签
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "page_main.h"
#include "page_manager.h"
#include "esp_log.h"

/** @brief 日志标签 */
static const char *TAG = "page_main";

/** @brief 声明自定义中文字体 */
LV_FONT_DECLARE(myFont);

/** @brief 屏幕宽度（像素） */
#define SCREEN_W 280

/** @brief 屏幕高度（像素） */
#define SCREEN_H 240

/** @brief 标题栏高度 */
#define TITLE_BAR_H 40

/** @brief 按钮宽度 */
#define BTN_W 120

/** @brief 按钮高度 */
#define BTN_H 55

/** @brief 内容区域左右边距 */
#define CONTENT_PAD 10

/**
 * @brief 按钮颜色配置数组
 *
 * 每个按钮使用不同的背景色，增强视觉区分度。
 * 颜色顺序：蓝、绿、橙、紫、青
 */
static const lv_color_t btn_colors[] = {
    LV_COLOR_MAKE(0x1E, 0x88, 0xE5),   /* 蓝色 */
    LV_COLOR_MAKE(0x43, 0xA0, 0x47),   /* 绿色 */
    LV_COLOR_MAKE(0xFB, 0x8C, 0x00),   /* 橙色 */
    LV_COLOR_MAKE(0x8E, 0x24, 0xAA),   /* 紫色 */
    LV_COLOR_MAKE(0x00, 0xAC, 0xC1),   /* 青色 */
};
#define BTN_COLOR_COUNT (sizeof(btn_colors) / sizeof(btn_colors[0]))

/** @brief 主菜单容器对象 */
static lv_obj_t *s_main_cont = NULL;

/**
 * @brief 按钮点击事件回调
 *
 * 当用户点击菜单按钮时，通过页面管理器导航到对应的测试页面。
 *
 * @param evt 指向 LVGL 事件对象的指针
 */
static void btn_event_handler(lv_event_t *evt)
{
    int page_index = (int)(intptr_t)lv_event_get_user_data(evt);
    ESP_LOGI(TAG, "点击菜单项 [%d]", page_index);
    page_manager_navigate_to(page_index);
}

/**
 * @brief 创建单个菜单按钮
 *
 * 在指定位置创建一个带图标和名称的彩色按钮。
 *
 * @param parent     父容器对象
 * @param icon       图标符号字符串（LV_SYMBOL_*）
 * @param name       按钮名称（中文标签）
 * @param color      按钮背景色
 * @param page_index 对应的页面管理器索引
 * @param x          X 坐标偏移
 * @param y          Y 坐标偏移
 */
static void create_menu_button(lv_obj_t *parent, const char *icon,
                                const char *name, lv_color_t color,
                                int page_index, lv_coord_t x, lv_coord_t y)
{
    /* 创建按钮 */
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);

    /* 按钮内使用垂直布局：图标在上，文字在下 */
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);

    /* 创建图标标签 */
    if (icon != NULL) {
        lv_obj_t *icon_label = lv_label_create(btn);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_color(icon_label, lv_color_white(), LV_PART_MAIN);
    }

    /* 创建名称标签 */
    if (name != NULL) {
        lv_obj_t *name_label = lv_label_create(btn);
        lv_label_set_text(name_label, name);
        lv_obj_set_style_text_font(name_label, &myFont, LV_PART_MAIN);
        lv_obj_set_style_text_color(name_label, lv_color_white(), LV_PART_MAIN);
    }

    /* 注册点击事件 */
    lv_obj_add_event_cb(btn, btn_event_handler, LV_EVENT_CLICKED,
                        (void *)(intptr_t)page_index);
}

/**
 * @brief 创建主菜单页面
 *
 * 构建包含标题和网格按钮的主菜单界面。
 * 遍历所有已注册页面，为每个页面创建一个菜单按钮。
 */
void page_main_create(void)
{
    page_main_refresh();
}

/**
 * @brief 销毁主菜单页面
 *
 * 清理主菜单页面的引用。
 * 实际控件由页面管理器的屏幕切换机制自动回收。
 */
void page_main_destroy(void)
{
    s_main_cont = NULL;
}

/**
 * @brief 刷新主菜单内容
 *
 * 重新构建完整的菜单界面，包括标题栏和按钮网格。
 * 此函数在 page_main_create() 内部调用，
 * 也可以在动态注册新页面后手动调用以更新显示。
 */
void page_main_refresh(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* 如果已有容器，先删除重建 */
    if (s_main_cont != NULL) {
        lv_obj_del(s_main_cont);
        s_main_cont = NULL;
    }

    /* ===== 创建主容器，填充整个屏幕 ===== */
    s_main_cont = lv_obj_create(scr);
    lv_obj_set_size(s_main_cont, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_main_cont, 0, 0);
    lv_obj_set_style_pad_all(s_main_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_main_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_main_cont, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_radius(s_main_cont, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_main_cont, LV_SCROLLBAR_MODE_OFF);

    /* ===== 顶部标题栏 ===== */
    lv_obj_t *title_bar = lv_obj_create(s_main_cont);
    lv_obj_set_size(title_bar, SCREEN_W, TITLE_BAR_H);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1E88E5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(title_bar, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, "酷世DIY 测试");
    lv_obj_set_style_text_font(title_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* ===== 内容区域 ===== */
    lv_coord_t content_h = SCREEN_H - TITLE_BAR_H;
    lv_obj_t *content = lv_obj_create(s_main_cont);
    lv_obj_set_size(content, SCREEN_W, content_h);
    lv_obj_set_pos(content, 0, TITLE_BAR_H);
    lv_obj_set_style_pad_all(content, CONTENT_PAD, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(content, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    /* 获取已注册页面数量 */
    int count = page_manager_count();

    if (count == 0) {
        /* 没有注册页面时显示提示文字 */
        lv_obj_t *empty_label = lv_label_create(content);
        lv_label_set_text(empty_label, "暂无测试项目");
        lv_obj_set_style_text_font(empty_label, &myFont, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x999999), LV_PART_MAIN);
        lv_obj_align(empty_label, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    /* 计算网格布局参数 */
    int col_count = 2;
    int row_count = (count + col_count - 1) / col_count;

    /* 可用空间 */
    lv_coord_t usable_w = SCREEN_W - 2 * CONTENT_PAD;
    lv_coord_t usable_h = content_h - 2 * CONTENT_PAD;

    /* 计算水平间距（等分排列） */
    lv_coord_t h_gap = (usable_w - col_count * BTN_W) / (col_count + 1);

    /* 垂直间距 */
    lv_coord_t v_gap = 10;

    /* 计算网格总高度并垂直居中 */
    lv_coord_t total_grid_h = row_count * BTN_H + (row_count - 1) * v_gap;
    lv_coord_t start_y = (usable_h - total_grid_h) / 2;
    if (start_y < v_gap) {
        start_y = v_gap;
    }

    /* 遍历所有已注册页面，创建按钮 */
    for (int i = 0; i < count; i++) {
        int row = i / col_count;
        int col = i % col_count;

        /* 计算按钮坐标（相对于 content 容器） */
        lv_coord_t x = col * (BTN_W + h_gap) + h_gap;
        lv_coord_t y = start_y + row * (BTN_H + v_gap);

        /* 获取页面信息 */
        const char *name = page_manager_get_name(i);
        const char *icon = page_manager_get_icon(i);

        /* 选择按钮颜色（循环使用颜色数组） */
        lv_color_t color = btn_colors[i % BTN_COLOR_COUNT];

        /* 创建菜单按钮 */
        create_menu_button(content, icon, name, color, i, x, y);
    }
}
