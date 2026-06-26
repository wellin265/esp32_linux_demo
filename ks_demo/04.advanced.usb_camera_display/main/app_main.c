/**
 * @file app_main.c
 * @brief USB 摄像头 LCD 实时显示示例
 *
 * USB UVC 摄像头实时视频流采集与 LCD 显示。
 * 支持 MJPEG 解码为 RGB565 并通过 SPI LCD 直接刷屏显示，
 * 支持通过按钮动态切换摄像头分辨率，分辨率设置保存在 NVS 中。
 *
 * 硬件连接：
 *   - USB 接口连接 UVC 摄像头
 *   - SPI 接口驱动 LCD 显示屏
 *   - GPIO5 连接分辨率切换按钮
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "usb_stream.h"
#include "iot_button.h"
#include "ksdiy_lvgl_port.h"
#include "app_jpeg_dec.h"
#include "app_nvs_storage.h"

static const char *TAG = "uvc_camera_lcd_demo";

/* ========== 配置常量 ========== */

#define DEMO_UVC_XFER_BUFFER_SIZE (88 * 1024)
#define DEMO_KEY_RESOLUTION       "resolution"
#define DEMO_SWITCH_BUTTON_IO     5
#define DEMO_MAX_H                1280
#define DEMO_MAX_V                720
#define BIT0_FRAME_START          (0x01 << 0)

/* ========== 类型定义 ========== */

typedef struct {
    uint16_t width;
    uint16_t height;
} camera_frame_size_t;

typedef struct {
    camera_frame_size_t camera_frame_size;
    uvc_frame_size_t   *camera_frame_list;
    size_t              camera_frame_list_num;
    size_t              camera_currect_frame_index;
} camera_resolution_info_t;

/* ========== 模块静态变量 ========== */

static EventGroupHandle_t  s_evt_handle;
static camera_resolution_info_t camera_resolution_info = {0};

static uint8_t *jpg_frame_buf = NULL;
static uint8_t *xfer_buffer_a = NULL;
static uint8_t *xfer_buffer_b = NULL;
static uint8_t *frame_buffer  = NULL;

static uint16_t current_width  = 0;
static uint16_t current_height = 0;

static lv_obj_t *camera_canvas = NULL;
static lv_obj_t *label         = NULL;

/* ========== 内部函数声明 ========== */

static void adaptive_jpg_frame_buffer(size_t length);
static void camera_display(uint8_t *lcd_buffer);
static void camera_frame_cb(uvc_frame_t *frame, void *ptr);
static esp_err_t display_init(void);
static esp_err_t usb_stream_init(void);
static size_t    find_current_resolution(camera_frame_size_t *camera_frame_size);
static void      switch_button_press_down_cb(void *arg, void *data);
static esp_err_t switch_button_init(void);
static void      stream_state_changed_cb(usb_stream_state_t event, void *arg);

/* ========== 显示相关函数 ========== */

/**
 * @brief 自适应调整 JPEG 帧缓冲区大小
 */
static void adaptive_jpg_frame_buffer(size_t length)
{
    if (jpg_frame_buf != NULL) {
        free(jpg_frame_buf);
    }
    jpg_frame_buf = (uint8_t *)heap_caps_aligned_alloc(16, length, MALLOC_CAP_SPIRAM);
    assert(jpg_frame_buf != NULL);
}

/**
 * @brief 将 RGB565 帧数据直接刷到 LCD
 *
 * 高速刷屏直接调用 esp_lcd_panel_draw_bitmap，不走 LVGL canvas（太慢）。
 * 注意：此方式仅支持固定尺寸的图像刷新。
 */
static void camera_display(uint8_t *lcd_buffer)
{
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                              current_width, current_height, lcd_buffer);
}

/**
 * @brief UVC 帧回调 —— JPEG 解码后显示
 *
 * 当 USB 摄像头产生新帧时调用。将 MJPEG 帧解码为 RGB565 并刷新到 LCD。
 * 帧缓冲区大小会根据实际分辨率自适应调整。
 */
static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    if (current_width != frame->width || current_height != frame->height) {
        current_width  = frame->width;
        current_height = frame->height;
        adaptive_jpg_frame_buffer(current_width * current_height * 2);
    }

    /* 使用模块化的 JPEG 解码器 */
    esp_err_t ret = app_jpeg_decode_frame(
        (uint8_t *)frame->data, frame->data_bytes, jpg_frame_buf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "JPEG 解码失败: 0x%x", ret);
        return;
    }

    camera_display(jpg_frame_buf);
}

/**
 * @brief 初始化 LVGL 显示界面
 *
 * 创建 canvas 和提示标签，用于在未连接摄像头时显示引导信息。
 * 实际摄像头显示使用直接刷屏，不走 canvas。
 */
static esp_err_t display_init(void)
{
    ksdiy_lvgl_port_init();
    ksdiy_lvgl_lock(0);

    camera_canvas = lv_canvas_create(lv_scr_act());
    assert(camera_canvas);
    lv_obj_center(camera_canvas);

    label = lv_label_create(lv_scr_act());
    lv_label_set_recolor(label, true);
    lv_obj_set_pos(label, 50, 10);
    lv_label_set_text(label, "Insert a camera\npress VOL+ for resolution.");

    ksdiy_lvgl_unlock();
    return ESP_OK;
}

/* ========== USB 流相关函数 ========== */

/**
 * @brief 配置 UVC USB 流参数
 */
static esp_err_t usb_stream_init(void)
{
    uvc_config_t uvc_config = {
        .frame_interval    = FRAME_INTERVAL_FPS_30,
        .xfer_buffer_size  = DEMO_UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a     = xfer_buffer_a,
        .xfer_buffer_b     = xfer_buffer_b,
        .frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .frame_buffer      = frame_buffer,
        .frame_cb          = &camera_frame_cb,
        .frame_cb_arg      = NULL,
        .frame_width       = FRAME_RESOLUTION_ANY,
        .frame_height      = FRAME_RESOLUTION_ANY,
        .flags             = FLAG_UVC_SUSPEND_AFTER_START,
    };

    esp_err_t ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UVC 流配置失败");
    }
    return ret;
}

/**
 * @brief 在分辨率列表中查找当前分辨率
 */
static size_t find_current_resolution(camera_frame_size_t *camera_frame_size)
{
    if (camera_resolution_info.camera_frame_list == NULL) {
        return (size_t)-1;
    }

    size_t i = 0;
    while (i < camera_resolution_info.camera_frame_list_num) {
        if (camera_frame_size->width  >= camera_resolution_info.camera_frame_list[i].width &&
            camera_frame_size->height >= camera_resolution_info.camera_frame_list[i].height) {
            camera_frame_size->width  = camera_resolution_info.camera_frame_list[i].width;
            camera_frame_size->height = camera_resolution_info.camera_frame_list[i].height;
            break;
        } else if (i == camera_resolution_info.camera_frame_list_num - 1) {
            camera_frame_size->width  = camera_resolution_info.camera_frame_list[i].width;
            camera_frame_size->height = camera_resolution_info.camera_frame_list[i].height;
            break;
        }
        i++;
    }
    ESP_LOGI(TAG, "当前分辨率: %dx%d", camera_frame_size->width, camera_frame_size->height);
    return i;
}

/**
 * @brief 分辨率切换按钮回调
 *
 * 循环切换到下一个支持的分辨率，暂停 USB 流后重置分辨率并保存到 NVS。
 */
static void switch_button_press_down_cb(void *arg, void *data)
{
    if (camera_resolution_info.camera_frame_list == NULL) {
        return;
    }
    if (xEventGroupWaitBits(s_evt_handle, BIT0_FRAME_START,
                            false, false, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    ESP_LOGI(TAG, "旧分辨率: %d*%d",
             camera_resolution_info.camera_frame_size.width,
             camera_resolution_info.camera_frame_size.height);

    if (++camera_resolution_info.camera_currect_frame_index >=
        camera_resolution_info.camera_frame_list_num) {
        camera_resolution_info.camera_currect_frame_index = 0;
    }
    camera_resolution_info.camera_frame_size.width  =
        camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width;
    camera_resolution_info.camera_frame_size.height =
        camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height;

    ESP_LOGI(TAG, "新分辨率: %d*%d",
             camera_resolution_info.camera_frame_size.width,
             camera_resolution_info.camera_frame_size.height);

    usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL);
    ESP_ERROR_CHECK(uvc_frame_size_reset(
        camera_resolution_info.camera_frame_size.width,
        camera_resolution_info.camera_frame_size.height,
        FPS2INTERVAL(30)));
    ESP_ERROR_CHECK(app_nvs_set_blob(DEMO_KEY_RESOLUTION,
                                     &camera_resolution_info.camera_frame_size,
                                     sizeof(camera_frame_size_t)));
    usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
}

/**
 * @brief 初始化分辨率切换按钮
 */
static esp_err_t switch_button_init(void)
{
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = DEMO_SWITCH_BUTTON_IO,
            .active_level = 0,
        },
    };

    button_handle_t button_handle = iot_button_create(&button_config);
    assert(button_handle != NULL);

    esp_err_t ret = iot_button_register_cb(button_handle, BUTTON_PRESS_DOWN,
                                            switch_button_press_down_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "按钮回调注册失败");
    }
    return ret;
}

/**
 * @brief USB 流状态变化回调
 *
 * 处理设备连接/断开事件。连接时获取支持的分辨率列表并恢复流。
 */
static void stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    switch (event) {
    case STREAM_CONNECTED: {
        size_t size = sizeof(camera_frame_size_t);
        app_nvs_get_blob(DEMO_KEY_RESOLUTION,
                         &camera_resolution_info.camera_frame_size, &size);

        size_t frame_index = 0;
        uvc_frame_size_list_get(NULL, &camera_resolution_info.camera_frame_list_num, NULL);

        if (camera_resolution_info.camera_frame_list_num) {
            ESP_LOGI(TAG, "UVC: 检测到 %u 种分辨率",
                     camera_resolution_info.camera_frame_list_num);

            uvc_frame_size_t *_frame_list = malloc(
                camera_resolution_info.camera_frame_list_num * sizeof(uvc_frame_size_t));

            camera_resolution_info.camera_frame_list = realloc(
                camera_resolution_info.camera_frame_list,
                camera_resolution_info.camera_frame_list_num * sizeof(uvc_frame_size_t));

            if (camera_resolution_info.camera_frame_list == NULL) {
                ESP_LOGE(TAG, "分辨率列表内存分配失败");
                break;
            }

            uvc_frame_size_list_get(_frame_list, NULL, NULL);

            /* 过滤掉超出最大分辨率的选项 */
            for (size_t i = 0; i < camera_resolution_info.camera_frame_list_num; i++) {
                if (_frame_list[i].width <= DEMO_MAX_H && _frame_list[i].height <= DEMO_MAX_V) {
                    camera_resolution_info.camera_frame_list[frame_index++] = _frame_list[i];
                    ESP_LOGI(TAG, "  保留[%u]: %ux%u", i, _frame_list[i].width, _frame_list[i].height);
                } else {
                    ESP_LOGI(TAG, "  跳过[%u]: %ux%u (超出限制)", i, _frame_list[i].width, _frame_list[i].height);
                }
            }
            camera_resolution_info.camera_frame_list_num = frame_index;

            if (camera_resolution_info.camera_frame_size.width != 0 &&
                camera_resolution_info.camera_frame_size.height != 0) {
                camera_resolution_info.camera_currect_frame_index =
                    find_current_resolution(&camera_resolution_info.camera_frame_size);
            } else {
                camera_resolution_info.camera_currect_frame_index = 0;
            }

            if ((size_t)-1 == camera_resolution_info.camera_currect_frame_index) {
                ESP_LOGE(TAG, "查找当前分辨率失败");
                break;
            }

            ESP_ERROR_CHECK(uvc_frame_size_reset(
                camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width,
                camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height,
                FPS2INTERVAL(30)));

            camera_frame_size_t camera_frame_size = {
                .width  = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width,
                .height = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height,
            };
            ESP_ERROR_CHECK(app_nvs_set_blob(DEMO_KEY_RESOLUTION,
                                             &camera_frame_size,
                                             sizeof(camera_frame_size_t)));

            free(_frame_list);

            usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
            xEventGroupSetBits(s_evt_handle, BIT0_FRAME_START);
        } else {
            ESP_LOGW(TAG, "UVC: 未获取到分辨率列表");
        }
        ESP_LOGI(TAG, "设备已连接");
        break;
    }

    case STREAM_DISCONNECTED:
        xEventGroupClearBits(s_evt_handle, BIT0_FRAME_START);
        ESP_LOGI(TAG, "设备已断开");
        break;

    default:
        ESP_LOGE(TAG, "未知 USB 事件: %d", event);
        break;
    }
}

/* ========== 主函数 ========== */

/**
 * @brief 应用程序入口
 *
 * 初始化流程：
 * 1. NVS 非易失性存储
 * 2. LVGL 显示界面
 * 3. 分辨率切换按钮
 * 4. USB UVC 视频流
 * 5. USB 流状态回调
 * 6. 启动 USB 流并等待设备连接
 */
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 创建事件组 */
    s_evt_handle = xEventGroupCreate();
    assert(s_evt_handle != NULL);

    /* 分配 USB 传输双缓冲区（SPIRAM） */
    xfer_buffer_a = (uint8_t *)heap_caps_malloc(DEMO_UVC_XFER_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    assert(xfer_buffer_a != NULL);
    xfer_buffer_b = (uint8_t *)heap_caps_malloc(DEMO_UVC_XFER_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    assert(xfer_buffer_b != NULL);

    /* 分配 JPEG 帧缓冲区（SPIRAM） */
    frame_buffer = (uint8_t *)heap_caps_malloc(DEMO_UVC_XFER_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    assert(frame_buffer != NULL);

    /* 初始化 LCD 显示 */
    ESP_ERROR_CHECK(display_init());

    /* 初始化分辨率切换按钮 */
    ESP_ERROR_CHECK(switch_button_init());

    /* 配置 USB UVC 视频流 */
    ESP_ERROR_CHECK(usb_stream_init());

    /* 注册 USB 流状态变化回调 */
    ESP_ERROR_CHECK(usb_streaming_state_register(&stream_state_changed_cb, NULL));

    /* 启动 USB 流并等待设备连接 */
    ESP_ERROR_CHECK(usb_streaming_start());
    ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));
}
