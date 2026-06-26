/**
 * @file page_about.h
 * @brief 系统信息页面头文件
 *
 * 提供系统信息展示页面的注册接口。
 * 显示芯片信息、内存使用、编译时间等系统参数。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef PAGE_ABOUT_H
#define PAGE_ABOUT_H

#include "page_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 系统信息页面实例（需注册到页面管理器） */
extern page_t page_about;

#ifdef __cplusplus
}
#endif

#endif /* PAGE_ABOUT_H */
