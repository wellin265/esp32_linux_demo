/**
 * @file page_manager.h
 * @brief 页面管理器头文件
 *
 * 提供页面注册、导航和生命周期管理接口。
 * 每个页面通过 create/destroy 回调实现自身的创建和销毁，
 * 页面管理器负责管理 LVGL 屏幕切换和页面栈。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 页面生命周期回调结构体
 *
 * 定义页面的创建、销毁回调以及显示属性。
 * 每个测试页面需要实现此结构体并注册到页面管理器。
 */
typedef struct {
    void (*create)(void);      /**< 页面创建回调，进入页面时调用 */
    void (*destroy)(void);     /**< 页面销毁回调，离开页面时调用 */
    const char *name;          /**< 页面名称（中文标签） */
    const char *icon;          /**< 页面图标（LV_SYMBOL_* 宏） */
} page_t;

/**
 * @brief 注册一个页面到页面管理器
 *
 * 将页面添加到内部页面列表，注册后可通过索引导航。
 * 应在 page_manager_init() 之前调用。
 *
 * @param page 指向 page_t 结构体的指针（需保持有效）
 */
void page_manager_register(page_t *page);

/**
 * @brief 导航到指定索引的页面
 *
 * 销毁当前页面，创建新屏幕，然后调用目标页面的 create 回调。
 *
 * @param index 目标页面的索引（从 0 开始）
 */
void page_manager_navigate_to(int index);

/**
 * @brief 返回主菜单
 *
 * 销毁当前页面，加载主菜单屏幕。
 */
void page_manager_go_back(void);

/**
 * @brief 获取当前页面索引
 *
 * @return 当前页面索引，-1 表示主菜单
 */
int page_manager_current(void);

/**
 * @brief 初始化页面管理器
 *
 * 在 LVGL 初始化完成后调用，创建主菜单屏幕。
 */
void page_manager_init(void);

/**
 * @brief 获取已注册页面的数量
 *
 * @return 已注册页面数量
 */
int page_manager_count(void);

/**
 * @brief 获取指定页面的名称
 *
 * @param index 页面索引
 * @return 页面名称字符串，索引无效时返回 NULL
 */
const char *page_manager_get_name(int index);

/**
 * @brief 获取指定页面的图标符号
 *
 * @param index 页面索引
 * @return 页面图标字符串（LV_SYMBOL_*），索引无效时返回 NULL
 */
const char *page_manager_get_icon(int index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGE_MANAGER_H */
