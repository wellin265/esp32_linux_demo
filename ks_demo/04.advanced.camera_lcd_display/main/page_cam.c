/**
 * @file page_cam.c
 * @brief 摄像头图像显示页面（无 LVGL 版本）
 *
 * 本文件实现摄像头图像的实时显示功能，不使用 LVGL 图形库：
 * - 直接将摄像头 RGB565 数据写入 LCD 显存
 * - 获得比 LVGL 版本更高的帧率
 * - 计算并显示帧率
 *
 * @author Kevincoooool
 * @date 2021-06-05
 */

#include "page_cam.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "app_camera.h"
#include <esp_system.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "ksdiy_lcd_port.h"

/** @brief 日志标签 */
#define TAG "PAGE_CAM"

/** @brief 摄像头帧缓冲区指针 */
camera_fb_t *fb;

/**
 * @brief 摄像头图像获取任务
 *
 * 在独立任务中持续获取摄像头图像并更新显示：
 * 1. 获取摄像头帧缓冲区
 * 2. 直接将 RGB565 数据写入 LCD 显存
 * 3. 释放帧缓冲区
 * 4. 计算并打印帧率
 *
 * 注意：此版本不使用 LVGL，直接操作 LCD，可获得更高帧率
 *
 * @param pvParameters 任务参数（未使用）
 */
void Cam_Task(void *pvParameters)
{
    static int64_t last_frame = 0;

    while (1)
    {
        /* 记录帧开始时间 */
        last_frame = esp_timer_get_time();

        /* 获取摄像头帧缓冲区 */
        fb = esp_camera_fb_get();
        if (fb != NULL)
        {
            /*
             * 直接将摄像头获取到的 RGB 原始图像刷屏
             * 这是无 LVGL 版本的核心优势：
             * - 跳过 LVGL 渲染层
             * - 直接 DMA 传输到 LCD
             * - 获得更高的帧率
             */
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, fb->width, fb->height, fb->buf);

            /* 刷完屏需要释放当前获取的图像缓冲区（必须调用） */
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

        /* 清空帧缓冲区指针 */
        fb = NULL;
    }
}

/**
 * @brief 加载摄像头显示页面
 *
 * 初始化流程：
 * 1. 初始化摄像头硬件
 * 2. 创建图像获取任务（绑定到 CPU 核心 0）
 */
void page_cam_load()
{
    app_camera_init();  /* 初始化摄像头 */

    /* 创建摄像头任务，绑定到 CPU 核心 0，优先级 14 */
    xTaskCreatePinnedToCore(&Cam_Task, "Cam_Task", 1024 * 5, NULL, 14, NULL, 0);
}
