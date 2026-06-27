/**
 * @file page_cam.c
 * @brief 摄像头图像显示页面
 *
 * 本文件实现摄像头图像的实时显示功能：
 * - 创建 LVGL 图像控件
 * - 在独立任务中获取摄像头图像
 * - 将图像数据显示到 LCD
 * - 计算并显示帧率
 *
 * @author Kevincoooool
 * @date 2021-06-05
 */

#include "page_cam.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "app_camera.h"
#include <esp_system.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "ksdiy_lvgl_port.h"

/** @brief 日志标签 */
#define TAG "PAGE_CAM"

/** @brief 摄像头帧缓冲区指针 */
camera_fb_t *fb;

/** @brief LVGL 图像控件指针 */
lv_obj_t *img_cam;

/** @brief LVGL 画布控件指针（备用） */
lv_obj_t *camera_canvas;

/**
 * @brief LVGL 图像描述符
 *
 * 用于描述图像的格式、尺寸和数据指针。
 * - 分辨率: 320x240
 * - 颜色格式: 真彩色 (RGB565)
 * - 数据大小: 240x240x2 字节
 */
lv_img_dsc_t img_dsc = {
    .header.reserved = 0,
    .header.always_zero = 0,
    .header.w = 320,               /* 图像宽度 */
    .header.h = 240,               /* 图像高度 */
    .data_size = 240 * 240 * 2,    /* 数据大小（字节） */
    .header.cf = LV_IMG_CF_TRUE_COLOR,  /* 真彩色格式 */
    .data = NULL,                    /* 图像数据指针（运行时设置） */
};

/**
 * @brief 摄像头图像获取任务
 *
 * 在独立任务中持续获取摄像头图像并更新显示：
 * 1. 获取摄像头帧缓冲区
 * 2. 获取 LVGL 互斥锁
 * 3. 更新图像控件显示
 * 4. 释放帧缓冲区
 * 5. 计算并打印帧率
 *
 * @param pvParameters 任务参数（未使用）
 */
void Cam_Task(void *pvParameters)
{
    static int64_t last_frame = 0;

    while (1)
    {
        /* 初始化帧计时 */
        if (!last_frame)
        {
            last_frame = esp_timer_get_time();
        }

        /* 获取摄像头帧缓冲区 */
        fb = esp_camera_fb_get();
        if (fb == NULL)
        {
            vTaskDelay(100);
            ESP_LOGE(TAG, "Get image failed!");
        }
        else
        {
            /* 获取 LVGL 互斥锁并更新显示 */
            if (ksdiy_lvgl_lock(0))
            {
                /* 设置图像数据指针 */
                img_dsc.data = fb->buf;
                /* 更新图像控件源 */
                lv_img_set_src(img_cam, &img_dsc);
                ksdiy_lvgl_unlock();
            }

            /* 释放帧缓冲区（必须调用） */
            esp_camera_fb_return(fb);

            /* 计算帧率 */
            int64_t fr_end = esp_timer_get_time();
            int64_t frame_time = fr_end - last_frame;
            last_frame = fr_end;
            frame_time /= 1000;  /* 转换为毫秒 */

            ESP_LOGI("esp", "MJPG:  %ums (%.1ffps)",
                     (uint32_t)frame_time,
                     1000.0 / (uint32_t)frame_time);
        }
    }

    /* 永远不会到达这里 */
    while (1)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief 初始化图像显示控件
 *
 * 创建 LVGL 图像控件并居中显示。
 */
void imgcam_init(void)
{
    /* 创建图像控件 */
    img_cam = lv_img_create(lv_scr_act());
    lv_obj_center(img_cam);

    /* 创建画布控件（备用） */
    camera_canvas = lv_canvas_create(lv_scr_act());
    assert(camera_canvas);
    lv_obj_center(camera_canvas);
}

/**
 * @brief 加载摄像头显示页面
 *
 * 初始化流程：
 * 1. 初始化摄像头硬件
 * 2. 创建图像显示控件
 * 3. 创建图像获取任务（绑定到 CPU 核心 0）
 */
void page_cam_load()
{
    app_camera_init();  /* 初始化摄像头 */
    imgcam_init();       /* 初始化图像控件 */

    /* 创建摄像头任务，绑定到 CPU 核心 0 */
    xTaskCreatePinnedToCore(&Cam_Task, "Cam_Task", 1024 * 5, NULL, 14, NULL, 0);
}
