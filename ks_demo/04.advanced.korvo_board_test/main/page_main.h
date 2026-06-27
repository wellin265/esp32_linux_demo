/**
 * @file page_main.h
 * @brief 主菜单页面头文件
 *
 * 提供主菜单页面的创建和注册接口。
 * 主菜单以网格布局展示所有已注册的测试项目，
 * 每个按钮包含图标和中文名称，点击后导航到对应测试页面。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef PAGE_MAIN_H
#define PAGE_MAIN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建并显示主菜单页面
 *
 * 生成网格布局的主菜单界面，包含所有已注册测试页面的按钮。
 * 每个按钮显示图标和名称，点击后通过页面管理器导航。
 * 同时显示页面标题"酷世DIY 测试"。
 */
void page_main_create(void);

/**
 * @brief 销毁主菜单页面
 *
 * 清理主菜单页面的所有 LVGL 控件和资源。
 */
void page_main_destroy(void);

/**
 * @brief 刷新主菜单内容
 *
 * 重新构建菜单网格，用于页面动态注册后更新显示。
 */
void page_main_refresh(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGE_MAIN_H */
