/**
 * @file audio.h
 * @brief MP3 音频解码和播放控制模块头文件
 *
 * 本头文件定义了 MP3 播放器的核心功能接口：
 * - MP3 软件解码（使用 helix_mp3 库）
 * - 播放控制（播放/暂停/上一曲/下一曲）
 * - 文件扫描和索引管理
 * - 事件回调机制
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
#include <string.h>

#include <stdio.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mp3dec.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音频事件类型枚举
 *
 * 定义播放器可能产生的各种事件。
 */
typedef enum {
    AUDIO_EVENT_NONE = 0,          /*!< 无事件 */
    AUDIO_EVENT_PAUSE,             /*!< 暂停事件 */
    AUDIO_EVENT_PLAY,              /*!< 播放事件 */
    AUDIO_EVENT_CHANGE,            /*!< 歌曲切换事件 */
    AUDIO_EVENT_FILE_SCAN_DONE,    /*!< 文件扫描完成事件 */
    AUDIO_EVENT_MAX,               /*!< 事件类型最大值 */
} audio_event_t;

/**
 * @brief 音频回调上下文结构体
 *
 * 用于传递事件信息和用户上下文到回调函数。
 */
typedef struct {
    audio_event_t audio_event;     /*!< 音频事件类型 */
    void *user_ctx;                /*!< 用户上下文指针 */
} audio_cb_ctx_t;

/**
 * @brief 音频回调函数类型
 *
 * @param ctx 音频回调上下文指针
 */
typedef void (*audio_cb_t)(audio_cb_ctx_t *);

/**
 * @brief 启动 MP3 播放器
 *
 * 创建音频播放任务，扫描指定目录的 MP3 文件并开始播放。
 *
 * @param file_path 包含 MP3 文件的目录路径
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_NO_MEM: 内存不足，无法创建任务
 */
esp_err_t mp3_player_start(char *file_path);

/**
 * @brief 恢复播放
 *
 * 从暂停状态恢复播放。
 *
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_INVALID_STATE: 播放器未启动
 */
esp_err_t audio_play(void);

/**
 * @brief 暂停播放
 *
 * 暂停当前播放的歌曲。
 *
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_INVALID_STATE: 播放器未启动
 */
esp_err_t audio_pause(void);

/**
 * @brief 播放下一首
 *
 * 切换到播放列表中的下一首歌曲。
 *
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_INVALID_STATE: 播放器未启动
 */
esp_err_t audio_play_next(void);

/**
 * @brief 播放上一首
 *
 * 切换到播放列表中的上一首歌曲。
 *
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_INVALID_STATE: 播放器未启动
 */
esp_err_t audio_play_prev(void);

/**
 * @brief 播放指定索引的歌曲
 *
 * 直接跳转到播放列表中指定索引的歌曲。
 *
 * @param index 歌曲索引（从 0 开始）
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_INVALID_ARG: 索引超出范围
 *    - ESP_ERR_INVALID_STATE: 播放器未启动
 */
esp_err_t audio_play_index(size_t index);

/**
 * @brief 获取当前播放的歌曲索引
 *
 * @return 当前歌曲索引（从 0 开始）
 */
size_t audio_get_index(void);

/**
 * @brief 根据索引获取歌曲文件名
 *
 * 获取播放列表中指定索引的歌曲文件名。
 *
 * @param index 歌曲索引（从 0 开始）
 * @param base_path 基础路径（可选），如提供则会拼接到文件名前
 * @return 歌曲文件名指针，如索引无效则返回 NULL
 */
char *audio_get_name_from_index(size_t index, char *base_path);

/**
 * @brief 注册音频事件回调函数
 *
 * 注册一个回调函数，用于接收播放器的各种事件通知。
 *
 * @param call_back 回调函数指针
 * @param user_ctx 用户上下文指针，将传递给回调函数
 * @return
 *    - ESP_OK: 成功
 *    - ESP_ERR_INVALID_ARG: 回调函数无效
 */
esp_err_t audio_callback_register(audio_cb_t call_back, void *user_ctx);

#ifdef __cplusplus
}
#endif
