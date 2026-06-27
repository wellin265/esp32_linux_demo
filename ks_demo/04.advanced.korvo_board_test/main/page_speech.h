/**
 * @file page_speech.h
 * @brief 语音识别页面头文件
 *
 * 提供语音识别测试页面的注册接口。
 * 页面显示语音唤醒词和可用命令词列表。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef PAGE_SPEECH_H
#define PAGE_SPEECH_H

#include "page_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 语音识别页面实例（需注册到页面管理器） */
extern page_t page_speech;

#ifdef __cplusplus
}
#endif

#endif /* PAGE_SPEECH_H */
