/**
 * @file page_button.h
 * @brief ADC按键测试页面头文件
 *
 * 提供ADC多路按键检测测试页面的注册接口。
 * 页面实时显示按键事件（按下、释放、单击、双击、长按等）。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef PAGE_BUTTON_H
#define PAGE_BUTTON_H

#include "page_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 按键测试页面实例（需注册到页面管理器） */
extern page_t page_button;

#ifdef __cplusplus
}
#endif

#endif /* PAGE_BUTTON_H */
