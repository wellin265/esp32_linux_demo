#include "ui_record.h"
#include "app_speech.h"
#include "wav_handler.h"
#include "esp_log.h"
#include <dirent.h>     // 用于目录操作
#include <sys/types.h>  // 用于DIR类型定义
#include <string.h>     // 用于字符串操作
#include <stdlib.h>     // 用于malloc函数
#include <stdio.h>      // 用于文件操作

#define TAG "UI_RECORD"

#define COLOR_BAR   lv_color_make(86, 94, 102)
#define COLOR_THEME lv_color_make(252, 199, 0)
#define COLOR_DEEP  lv_color_make(246, 174, 61)
#define COLOR_TEXT  lv_color_make(56, 56, 56)
#define COLOR_BG    lv_color_make(238, 241, 245)

static lv_obj_t *slider_label;

// 添加文件列表相关UI组件
static lv_obj_t *file_list;
static char *file_paths[50];  // 存储文件路径
static int file_count = 0;

static void btn_cb(lv_event_t *event);
static void volume_slider_cb(lv_event_t *event);
static void refresh_file_list(void);
static void play_file_cb(lv_event_t *e);

void ui_record(void)
{
    static lv_obj_t *default_src;
    default_src = lv_scr_act();		

    lv_obj_t *btn = lv_btn_create(default_src);
    lv_obj_set_size(btn, 100, 100);
    lv_obj_set_style_radius(btn, 50, LV_STATE_DEFAULT);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 30, 30);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *label = lv_label_create(default_src);
    lv_label_set_text(label, "Press to Record & Play");
    lv_obj_set_style_text_font(label,   &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label,  lv_palette_main(LV_PALETTE_RED), LV_STATE_DEFAULT);
    lv_obj_align_to(label, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);

    lv_obj_t *volume_slider = lv_slider_create(default_src);
    lv_obj_set_width(volume_slider, 150);
    // lv_obj_set_size(volume_slider, 250, 10);
    lv_obj_set_ext_click_area(volume_slider, 20);
    lv_slider_set_range(volume_slider, 0, 90);
    lv_slider_set_value(volume_slider, 85, LV_ANIM_ON);
    lv_obj_align(volume_slider, LV_ALIGN_CENTER, 0, 70);
    lv_obj_add_event_cb(volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    slider_label = lv_label_create(default_src);
    lv_label_set_text(slider_label, "85%");
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_align_to(slider_label, volume_slider, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    // 创建文件列表
    file_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(file_list, 120, 220);
    lv_obj_align(file_list, LV_ALIGN_TOP_RIGHT, -10, 10);
    
    // 初始刷新文件列表
    refresh_file_list();
    
    // 添加列表项点击事件
    lv_obj_add_event_cb(file_list, play_file_cb, LV_EVENT_CLICKED, NULL);
}

static void btn_cb(lv_event_t *event)
{
    lv_obj_t *btn = (lv_obj_t *) event->target;

    if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
        // audio_record_start();
        // ESP_LOGI("main", "audio_record_start");
        mode = record;
    } else {
        // audio_play_start();
        // ESP_LOGI("main", "audio_play_start");
        mode = play;
    }
}

static void volume_slider_cb(lv_event_t *event)
{
    static char buf[8];

    lv_obj_t *slider = (lv_obj_t *) event->target;
    int volume = lv_slider_get_value(slider);
    esp_codec_dev_set_out_vol(output_dev_, volume);
    sprintf(buf, "%d%%", volume);
    lv_label_set_text(slider_label, buf);
}
static void restore_btn_color(lv_timer_t *timer)
{
    lv_obj_t *btn = (lv_obj_t *)timer->user_data;
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x808080), LV_STATE_DEFAULT);
    lv_timer_del(timer);  // 删除定时器
}
static void refresh_file_list(void)
{
    // 清除现有列表
    lv_obj_clean(file_list);
    
    // 释放之前分配的内存
    for(int i = 0; i < file_count; i++) {
        if(file_paths[i]) {
            free(file_paths[i]);
            file_paths[i] = NULL;
        }
    }
    file_count = 0;
    
    // 列出/sdcard目录下的所有.wav文件
    DIR *dir = opendir("/sdcard");
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory /sdcard");
        return;
    }

    struct dirent *ent;
    
    while ((ent = readdir(dir)) != NULL && file_count < 50) {
        if (strstr(ent->d_name, ".wav") || strstr(ent->d_name, ".WAV")) {
            ESP_LOGI(TAG, "Found WAV file: %s", ent->d_name);
            
            // 创建一个列表按钮作为容器
            lv_obj_t *obj = lv_list_add_btn(file_list, NULL, ent->d_name);
            if (!obj) {
                ESP_LOGE(TAG, "Failed to create list button");
                continue;
            }
            
            // 设置按钮样式
            lv_obj_set_height(obj, 40);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x808080), LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
            
            // 保存文件路径
            char *path = malloc(strlen("/sdcard/") + strlen(ent->d_name) + 1);
            if (!path) {
                ESP_LOGE(TAG, "Failed to allocate memory for file path");
                continue;
            }
            sprintf(path, "/sdcard/%s", ent->d_name);
            file_paths[file_count] = path;
            
            // 设置用户数据(文件索引)
            lv_obj_set_user_data(obj, (void*)(intptr_t)file_count);
            
            // 为每个按钮添加点击事件
            lv_obj_add_event_cb(obj, play_file_cb, LV_EVENT_CLICKED, NULL);
            
            file_count++;
            ESP_LOGI(TAG, "Added file to list: %s, index: %d", path, file_count-1);
        }
    }
    closedir(dir);
    
    ESP_LOGI(TAG, "Total WAV files found: %d", file_count);
}
static void play_file_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (!btn) {
        ESP_LOGE(TAG, "Invalid button object");
        return;
    }

    // 获取文件索引
    intptr_t idx = (intptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || idx >= file_count) {
        ESP_LOGE(TAG, "Invalid file index: %d", (int)idx);
        return;
    }

    ESP_LOGI(TAG, "Attempting to play file: %s", file_paths[idx]);
    
    // 确保音频缓冲区已初始化
    if (!audio_buffer) {
        audio_buffer = heap_caps_malloc(BUF_SIZE, MALLOC_CAP_SPIRAM);
        if (!audio_buffer) {
            ESP_LOGE(TAG, "Failed to allocate audio buffer");
            return;
        }
    }
    
    // 先将模式切换为idle，停止当前播放
    mode = idle;
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待播放停止

    // 清空缓冲区
    memset(audio_buffer, 0, BUF_SIZE);
    
    // 加载WAV文件
    size_t loaded_size;
    esp_err_t ret = wav_load_file(file_paths[idx], audio_buffer, BUF_SIZE, &loaded_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load WAV file: %d", ret);
        return;
    }
    
    // 更新音频索引和总长度
    audio_index = 0;
    audio_total = loaded_size;
    
    ESP_LOGI(TAG, "Successfully loaded WAV file: %s, size: %d bytes", file_paths[idx], loaded_size);
    
    // 设置播放模式
    mode = play;
    
    // 给用户视觉反馈
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_DEFAULT);
    
    // 创建定时器恢复按钮颜色
    lv_timer_t *timer = lv_timer_create(restore_btn_color, 500, btn);
    lv_timer_set_repeat_count(timer, 1);
}