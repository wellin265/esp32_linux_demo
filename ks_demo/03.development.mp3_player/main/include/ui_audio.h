/**
 * @file ui_audio.h
 * @brief MP3 播放器 LVGL 用户界面头文件
 *
 * 本头文件定义了 MP3 播放器图形界面的接口：
 * - 播放控制按钮
 * - 音量调节滑块
 * - 歌曲列表选择器
 *
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "audio.h"

/**
 * @brief 启动音频播放器用户界面
 *
 * 创建完整的 MP3 播放器界面，包括：
 * - 播放/暂停按钮（圆形可切换按钮）
 * - 上一曲/下一曲控制按钮
 * - 音量调节滑块
 * - 歌曲标题显示
 * - 歌曲选择下拉框
 *
 * @note 此函数应在 LVGL 初始化完成后调用
 */
void ui_audio_start(void);

#ifdef __cplusplus
}
#endif
