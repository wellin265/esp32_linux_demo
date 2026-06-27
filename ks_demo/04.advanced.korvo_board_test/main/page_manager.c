/**
 * @file page_manager.c
 * @brief 页面管理器实现
 *
 * 管理 LVGL 页面的注册、导航和生命周期。
 * 使用 lv_scr_load() 实现屏幕切换，每个子页面拥有独立的
 * lv_obj_t 屏幕，主菜单也有独立屏幕。
 *
 * 导航流程：
 *   1. 调用当前页面的 destroy 回调（清理旧页面资源）
 *   2. 创建新的 lv_obj_t 屏幕
 *   3. 调用 lv_scr_load() 切换显示
 *   4. 调用目标页面的 create 回调（构建新页面 UI）
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "page_manager.h"
#include "ksdiy_lvgl_port.h"
#include "esp_log.h"
#include <string.h>

/** @brief 日志标签 */
static const char *TAG = "page_mgr";

/** @brief 最大支持页面数量 */
#define PAGE_MAX_COUNT 16

/** @brief 已注册页面数组 */
static page_t *s_pages[PAGE_MAX_COUNT];

/** @brief 已注册页面数量 */
static int s_page_count = 0;

/** @brief 当前页面索引，-1 表示主菜单 */
static int s_current_page = -1;

/** @brief 主菜单屏幕对象 */
static lv_obj_t *s_main_screen = NULL;

/**
 * @brief 注册页面到页面管理器
 */
void page_manager_register(page_t *page)
{
    if (page == NULL) {
        ESP_LOGE(TAG, "注册失败: page 为 NULL");
        return;
    }
    if (s_page_count >= PAGE_MAX_COUNT) {
        ESP_LOGE(TAG, "注册失败: 页面数量已达上限 %d", PAGE_MAX_COUNT);
        return;
    }

    s_pages[s_page_count] = page;
    ESP_LOGI(TAG, "注册页面 [%d] %s", s_page_count, page->name ? page->name : "unnamed");
    s_page_count++;
}

/**
 * @brief 获取当前页面索引
 */
int page_manager_current(void)
{
    return s_current_page;
}

/**
 * @brief 获取已注册页面数量
 */
int page_manager_count(void)
{
    return s_page_count;
}

/**
 * @brief 导航到指定页面
 */
void page_manager_navigate_to(int index)
{
    if (index < 0 || index >= s_page_count) {
        ESP_LOGE(TAG, "导航失败: 无效索引 %d (共 %d 页)", index, s_page_count);
        return;
    }

    /* 销毁当前页面 */
    if (s_current_page >= 0 && s_pages[s_current_page]->destroy != NULL) {
        ESP_LOGI(TAG, "销毁页面 [%d]", s_current_page);
        s_pages[s_current_page]->destroy();
    }

    /* 创建新屏幕并切换 */
    lv_obj_t *new_screen = lv_obj_create(NULL);
    lv_disp_load_scr(new_screen);

    ESP_LOGI(TAG, "导航到页面 [%d] %s", index,
             s_pages[index]->name ? s_pages[index]->name : "unnamed");

    /* 更新当前页面索引 */
    s_current_page = index;

    /* 调用目标页面的创建回调 */
    if (s_pages[index]->create != NULL) {
        s_pages[index]->create();
    }
}

/**
 * @brief 返回主菜单
 */
void page_manager_go_back(void)
{
    /* 如果已经在主菜单，无需操作 */
    if (s_current_page == -1) {
        return;
    }

    /* 销毁当前页面 */
    if (s_pages[s_current_page]->destroy != NULL) {
        ESP_LOGI(TAG, "销毁页面 [%d]，返回主菜单", s_current_page);
        s_pages[s_current_page]->destroy();
    }

    /* 加载主菜单屏幕 */
    if (s_main_screen != NULL) {
        lv_disp_load_scr(s_main_screen);
    }

    s_current_page = -1;
    ESP_LOGI(TAG, "已返回主菜单");
}

/**
 * @brief 获取指定页面的名称
 */
const char *page_manager_get_name(int index)
{
    if (index < 0 || index >= s_page_count) {
        return NULL;
    }
    return s_pages[index]->name;
}

/**
 * @brief 获取指定页面的图标符号
 */
const char *page_manager_get_icon(int index)
{
    if (index < 0 || index >= s_page_count) {
        return NULL;
    }
    return s_pages[index]->icon;
}

/**
 * @brief 初始化页面管理器
 *
 * 创建主菜单屏幕并设为当前活动屏幕。
 * 必须在 LVGL 初始化完成后调用。
 */
void page_manager_init(void)
{
    /* 创建主菜单屏幕 */
    s_main_screen = lv_obj_create(NULL);
    lv_disp_load_scr(s_main_screen);
    s_current_page = -1;

    ESP_LOGI(TAG, "页面管理器初始化完成，已注册 %d 个页面", s_page_count);
}
