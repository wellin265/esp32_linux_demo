/**
 * @file page_wakeup.h
 * @brief 语音唤醒 LVGL 页面模块头文件
 *
 * 提供语音唤醒页面的创建、销毁和文本更新接口。
 * 使用 LVGL 标签控件显示识别结果文本。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */


#ifndef _page_wakeup_
#define _page_wakeup_

#ifdef __cplusplus
extern "C" {
#endif
/*********************
* INCLUDES
*********************/
#include "lvgl.h"
void wakeup_set_text(char *text);

void page_wakeup_start(void);
void page_wakeup_end(void);
#ifdef __cplusplus
} /* extern "C" */
#endif




#endif // _TEST_


