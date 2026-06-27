/**
 * @file ui_audio.c
 * @brief MP3 播放器 LVGL 用户界面
 *
 * 本文件实现 MP3 播放器的图形用户界面：
 * - 播放/暂停按钮
 * - 上一曲/下一曲按钮
 * - 音量滑块
 * - 歌曲列表下拉框
 * - 歌曲标题显示
 *
 * 界面布局：
 * ┌────────────────────────┐
 * │      歌曲标题          │
 * │    [歌曲选择下拉框]    │
 * │                        │
 * │   ◀◀   ▶/⏸   ▶▶      │
 * │                        │
 * │  🔈 ───────●──── 🔊   │
 * └────────────────────────┘
 *
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
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

#include "ui_audio.h"
#include "app_speech.h"

LV_FONT_DECLARE(myFont)

/**
 * @brief 播放/暂停按钮回调函数
 *
 * 当播放/暂停按钮状态改变时调用：
 * - 选中状态：显示播放图标，暂停音频
 * - 未选中状态：显示暂停图标，恢复播放
 *
 * @param event 事件对象指针
 */
static void btn_play_pause_cb(lv_event_t *event)
{
    lv_obj_t *btn = (lv_obj_t *)event->target;
    lv_obj_t *lab = (lv_obj_t *)btn->user_data;

    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
    {
        /* 选中状态：暂停 */
        lv_label_set_text_static(lab, LV_SYMBOL_PLAY);
        audio_pause();
    }
    else
    {
        /* 未选中状态：播放 */
        lv_label_set_text_static(lab, LV_SYMBOL_PAUSE);
        audio_play();
    }
}

/**
 * @brief 上一曲/下一曲按钮回调函数
 *
 * @param event 事件对象指针
 *             user_data 为 true 表示下一曲，false 表示上一曲
 */
static void btn_prev_next_cb(lv_event_t *event)
{
    bool is_next = (bool)event->user_data;

    if (is_next)
    {
        audio_play_next();
    }
    else
    {
        audio_play_prev();
    }
}

/**
 * @brief 音量滑块回调函数
 *
 * 当滑块值改变时调用，实时调整音量。
 *
 * @param event 事件对象指针
 */
static void volume_slider_cb(lv_event_t *event)
{
    lv_obj_t *slider = (lv_obj_t *)event->target;
    int volume = lv_slider_get_value(slider);
    esp_codec_dev_set_out_vol(output_dev_, volume);
}

/**
 * @brief 音频事件回调函数
 *
 * 处理来自音频任务的事件：
 * - AUDIO_EVENT_FILE_SCAN_DONE：更新歌曲列表
 * - AUDIO_EVENT_CHANGE：更新当前播放歌曲显示
 *
 * @param ctx 音频回调上下文指针
 */
static void audio_callback(audio_cb_ctx_t *ctx)
{
    audio_event_t event = ctx->audio_event;

    lv_obj_t *music_list = (lv_obj_t *)ctx->user_ctx;
    lv_obj_t *label_title = (lv_obj_t *)music_list->user_data;
    lv_obj_t *btn_play_pause = (lv_obj_t *)label_title->user_data;
    lv_obj_t *label_play_pause = (lv_obj_t *)btn_play_pause->user_data;

    if (AUDIO_EVENT_FILE_SCAN_DONE == event)
    {
        /* 文件扫描完成，更新下拉列表 */
        lv_dropdown_clear_options(music_list);

        size_t i = 0;
        while (true)
        {
            char *file_name = audio_get_name_from_index(i, NULL);
            if (NULL != file_name)
            {
                lv_dropdown_add_option(music_list, file_name, i);
            }
            else
            {
                lv_dropdown_set_selected(music_list, 0);
                lv_label_set_text_static(label_title, audio_get_name_from_index(0, NULL));
                break;
            }
            i++;
        }
    }

    if (AUDIO_EVENT_CHANGE == event)
    {
        /* 歌曲切换，更新显示 */
        lv_dropdown_set_selected(music_list, audio_get_index());
        lv_label_set_text_static(label_title,
                                 audio_get_name_from_index(audio_get_index(), NULL));

        /* 更新播放按钮状态 */
        if (lv_obj_has_state(btn_play_pause, LV_STATE_CHECKED))
        {
            lv_obj_clear_state(btn_play_pause, LV_STATE_CHECKED);
            lv_label_set_text_static(label_play_pause, LV_SYMBOL_PAUSE);
            lv_obj_invalidate(btn_play_pause);
        }
    }
}

/**
 * @brief 歌曲列表下拉框回调函数
 *
 * 当用户选择下拉列表中的歌曲时调用。
 *
 * @param event 事件对象指针
 */
static void music_list_cb(lv_event_t *event)
{
    lv_obj_t *music_list = (lv_obj_t *)event->target;
    audio_play_index(lv_dropdown_get_selected(music_list));
}

/**
 * @brief 启动音频用户界面
 *
 * 创建完整的 MP3 播放器界面：
 * 1. 播放/暂停按钮（圆形可切换按钮）
 * 2. 上一曲/下一曲按钮
 * 3. 音量滑块
 * 4. 歌曲标题标签
 * 5. 歌曲选择下拉框
 *
 * 界面元素使用 user_data 存储关联对象指针，
 * 形成链式引用以便在回调中访问。
 */
void ui_audio_start(void)
{
    /* ===== 创建播放/暂停按钮 ===== */
    lv_obj_t *btn_play_pause = lv_btn_create(lv_scr_act());
    lv_obj_align(btn_play_pause, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_size(btn_play_pause, 50, 50);
    lv_obj_set_style_radius(btn_play_pause, 25, LV_STATE_DEFAULT);  /* 圆形按钮 */
    lv_obj_add_flag(btn_play_pause, LV_OBJ_FLAG_CHECKABLE);         /* 可切换状态 */

    /* 播放/暂停图标 */
    lv_obj_t *label_play_pause = lv_label_create(btn_play_pause);
    lv_label_set_text_static(label_play_pause, LV_SYMBOL_PAUSE);
    lv_obj_center(label_play_pause);
    lv_obj_set_user_data(btn_play_pause, (void *)label_play_pause);
    lv_obj_add_event_cb(btn_play_pause, btn_play_pause_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ===== 创建上一曲按钮 ===== */
    lv_obj_t *label_prev = lv_label_create(lv_scr_act());
    lv_obj_set_user_data(label_prev, btn_play_pause);
    lv_obj_add_flag(label_prev, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text_static(label_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(label_prev, &lv_font_montserrat_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_prev, lv_color_make(255, 0, 0), LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(label_prev, 10);
    lv_obj_align_to(label_prev, btn_play_pause, LV_ALIGN_OUT_LEFT_MID, -40, 0);
    lv_obj_add_event_cb(label_prev, btn_prev_next_cb, LV_EVENT_CLICKED, (void *)false);

    /* ===== 创建下一曲按钮 ===== */
    lv_obj_t *label_next = lv_label_create(lv_scr_act());
    lv_obj_set_user_data(label_next, btn_play_pause);
    lv_obj_add_flag(label_next, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text_static(label_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(label_next, &myFont, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_next, lv_color_make(255, 0, 0), LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(label_next, 10);
    lv_obj_align_to(label_next, btn_play_pause, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_obj_add_event_cb(label_next, btn_prev_next_cb, LV_EVENT_CLICKED, (void *)true);

    /* ===== 创建音量滑块 ===== */
    lv_obj_t *volume_slider = lv_slider_create(lv_scr_act());
    lv_obj_set_size(volume_slider, 150, 6);
    lv_obj_set_ext_click_area(volume_slider, 15);
    lv_obj_align(volume_slider, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_slider_set_range(volume_slider, 0, 90);
    lv_slider_set_value(volume_slider, 70, LV_ANIM_ON);
    lv_obj_add_event_cb(volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 音量最小图标 */
    lv_obj_t *lab_vol_min = lv_label_create(lv_scr_act());
    lv_label_set_text_static(lab_vol_min, LV_SYMBOL_VOLUME_MID);
    lv_obj_set_style_text_font(lab_vol_min, &myFont, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_vol_min, volume_slider, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    /* 音量最大图标 */
    lv_obj_t *lab_vol_max = lv_label_create(lv_scr_act());
    lv_label_set_text_static(lab_vol_max, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(lab_vol_max, &myFont, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_vol_max, volume_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    /* ===== 创建歌曲标题 ===== */
    lv_obj_t *lab_title = lv_label_create(lv_scr_act());
    lv_obj_set_user_data(lab_title, (void *)btn_play_pause);
    lv_label_set_text_static(lab_title, "Scaning Files...");
    lv_obj_set_style_text_font(lab_title, &myFont, LV_STATE_DEFAULT);
    lv_obj_align(lab_title, LV_ALIGN_TOP_MID, 0, 20);

    /* ===== 创建歌曲选择下拉框 ===== */
    lv_obj_t *music_list = lv_dropdown_create(lv_scr_act());
    lv_dropdown_clear_options(music_list);
    lv_dropdown_set_options_static(music_list, "Scaning...");
    lv_obj_set_width(music_list, 200);
    lv_obj_align_to(music_list, lab_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_user_data(music_list, (void *)lab_title);
    lv_obj_add_event_cb(music_list, music_list_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 注册音频回调 */
    audio_callback_register(audio_callback, (void *)music_list);
}
