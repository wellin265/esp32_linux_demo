/**
 * @file page_camera.c
 * @brief 摄像头测试页面实现
 *
 * 实现摄像头实时预览功能，使用 esp_timer 定时获取摄像头帧数据
 * 并更新 LVGL 图像控件显示。支持 OV2640 摄像头 96x96 RGB565 格式。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "page_camera.h"
#include "page_manager.h"
#include "app_camera.h"
#include "ksdiy_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

/** @brief 日志标签 */
static const char *TAG = "page_camera";

/** @brief 声明自定义中文字体 */
LV_FONT_DECLARE(myFont);

/** @brief 摄像头图像显示控件 */
static lv_obj_t *s_img_cam = NULL;

/** @brief 状态标签控件 */
static lv_obj_t *s_label_status = NULL;

/** @brief 图像描述符 */
static lv_img_dsc_t s_img_dsc = {0};

/** @brief 摄像头刷新定时器 */
static esp_timer_handle_t s_cam_timer = NULL;

/** @brief 黑色填充数据（摄像头未就绪时使用） */
static uint8_t s_black_frame[96 * 96 * 2];

/**
 * @brief 摄像头帧刷新定时器回调
 *
 * 周期性获取摄像头帧并更新LVGL图像控件。
 * 使用 ksdiy_lvgl_lock/unlock 保证线程安全。
 *
 * @param arg 未使用
 */
static void cam_timer_cb(void *arg)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb != NULL && s_img_cam != NULL) {
        if (ksdiy_lvgl_lock(100)) {
            s_img_dsc.data = fb->buf;
            lv_img_cache_invalidate_src(s_img_cam);
            lv_img_set_src(s_img_cam, &s_img_dsc);
            ksdiy_lvgl_unlock();
        }
        esp_camera_fb_return(fb);
    }
}

/**
 * @brief 返回按钮点击事件回调
 *
 * 停止摄像头定时器并返回主菜单。
 *
 * @param e LVGL事件对象
 */
static void back_btn_cb(lv_event_t *e)
{
    if (s_cam_timer != NULL) {
        esp_timer_stop(s_cam_timer);
        esp_timer_delete(s_cam_timer);
        s_cam_timer = NULL;
    }
    page_manager_go_back();
}

/**
 * @brief 创建摄像头测试页面
 *
 * 构建摄像头预览界面，包括：
 * - 蓝色标题栏（摄像头测试）
 * - 96x96 摄像头图像预览区
 * - 状态文字标签
 * - 返回按钮
 * - 100ms 周期定时器用于刷新摄像头帧
 */
static void page_camera_create(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* ===== 顶部标题栏 ===== */
    lv_obj_t *title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 280, 36);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1E88E5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, " 摄像头测试");
    lv_obj_set_style_text_font(title_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* ===== 初始化图像描述符 ===== */
    memset(s_black_frame, 0, sizeof(s_black_frame));
    s_img_dsc.header.w = 96;
    s_img_dsc.header.h = 96;
    s_img_dsc.data_size = 96 * 96 * 2;
    s_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_img_dsc.data = s_black_frame;

    /* 尝试获取一帧图像 */
    camera_fb_t *fb = esp_camera_fb_get();
    bool cam_ok = false;
    if (fb != NULL) {
        s_img_dsc.data = fb->buf;
        cam_ok = true;
    }

    /* ===== 摄像头图像显示控件 ===== */
    s_img_cam = lv_img_create(scr);
    lv_img_set_src(s_img_cam, &s_img_dsc);
    lv_obj_align(s_img_cam, LV_ALIGN_CENTER, 0, -15);

    if (fb != NULL) {
        esp_camera_fb_return(fb);
    }

    /* ===== 状态标签 ===== */
    s_label_status = lv_label_create(scr);
    lv_label_set_text(s_label_status, cam_ok ? "摄像头正常 96x96 RGB565" : "摄像头未初始化");
    lv_obj_set_style_text_font(s_label_status, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_status,
        cam_ok ? lv_color_hex(0x2E7D32) : lv_color_hex(0xC62828), LV_PART_MAIN);
    lv_obj_align(s_label_status, LV_ALIGN_CENTER, 0, 55);

    /* ===== 返回按钮 ===== */
    lv_obj_t *btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, 80, 32);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_back, 8, LV_PART_MAIN);

    lv_obj_t *btn_label = lv_label_create(btn_back);
    lv_label_set_text(btn_label, "返回");
    lv_obj_set_style_text_font(btn_label, &myFont, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn_back, back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* ===== 启动摄像头刷新定时器 ===== */
    if (cam_ok) {
        esp_timer_create_args_t timer_args = {
            .callback = cam_timer_cb,
            .name = "cam_timer"
        };
        esp_timer_create(&timer_args, &s_cam_timer);
        esp_timer_start_periodic(s_cam_timer, 100000); /* 100ms 刷新 */
    }

    ESP_LOGI(TAG, "摄像头测试页面已创建");
}

/**
 * @brief 销毁摄像头测试页面
 *
 * 停止定时器，清理页面资源。
 */
static void page_camera_destroy(void)
{
    if (s_cam_timer != NULL) {
        esp_timer_stop(s_cam_timer);
        esp_timer_delete(s_cam_timer);
        s_cam_timer = NULL;
    }
    s_img_cam = NULL;
    s_label_status = NULL;
    ESP_LOGI(TAG, "摄像头测试页面已销毁");
}

/** @brief 摄像头测试页面实例 */
page_t page_camera = {
    .create = page_camera_create,
    .destroy = page_camera_destroy,
    .name = "摄像头",
    .icon = LV_SYMBOL_IMAGE,
};
