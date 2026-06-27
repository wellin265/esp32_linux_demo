/**
 * @file page_cam.c
 * @brief 颜色追踪显示页面
 *
 * 本文件实现摄像头图像的实时颜色追踪功能：
 * - 创建 LVGL 图像控件
 * - 在独立任务中获取摄像头图像
 * - 使用 EasyTracer 算法识别颜色
 * - 在识别到的目标周围绘制边框
 * - 将图像数据显示到 LCD
 *
 * 支持追踪的颜色：
 * - 绿色
 * - 红色
 * - 蓝色
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
#include "fb_gfx.h"
#include "EasyTracer.h"

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

/** @brief 颜色识别结果 */
RESULT Resu;

/** @brief 当前识别颜色种类（1=绿色, 2=红色, 3=蓝色） */
uint8_t color_type = 1;

/**
 * @brief 目标颜色条件配置数组
 *
 * 每个颜色包含以下参数：
 * - H_MIN/H_MAX: 色调范围
 * - S_MIN/S_MAX: 饱和度范围
 * - L_MIN/L_MAX: 亮度范围
 * - WIDTH_MIN/HIGHT_MIN: 目标最小尺寸
 * - WIDTH_MAX/HIGHT_MAX: 目标最大尺寸
 *
 * @note 需要根据实际环境调整 HSL 值，可用画图工具获取
 */
TARGET_CONDI Condition[3] = {
    /* 目标最小色调, 目标最大色调, 目标最小饱和度, 目标最大饱和度,
       目标最小亮度, 目标最大亮度, 目标最小宽度, 目标最小高度,
       目标最大宽度, 目标最大高度 */
    {10, 120, 70, 250, 10, 180, 40, 40, 320, 240},   /* 绿色 */
    {180, 255, 70, 250, 10, 180, 40, 40, 320, 240}, /* 红色 */
    {130, 170, 70, 250, 10, 180, 40, 40, 320, 240}, /* 蓝色 */
};

/**
 * @brief 在图像上绘制矩形边框
 *
 * 在识别到的目标周围绘制黄色边框。
 *
 * @param image_matrix 图像帧缓冲区
 * @param x 矩形左上角 X 坐标
 * @param y 矩形左上角 Y 坐标
 * @param w 矩形宽度
 * @param h 矩形高度
 */
void draw_fillRect(camera_fb_t *image_matrix, int32_t x, int32_t y, int32_t w, int32_t h)
{
    /* 边框颜色：黄色 */
    uint32_t color = 0xFFFFFF00;

    /* 配置帧缓冲区信息 */
    fb_data_t fb;
    fb.width = image_matrix->width;
    fb.height = image_matrix->height;
    fb.data = image_matrix->buf;
    fb.bytes_per_pixel = 2;
    fb.format = FB_RGB565;

    /* 绘制四条边 */
    fb_gfx_drawFastHLine(&fb, x, y, w, color);          /* 上边 */
    fb_gfx_drawFastHLine(&fb, x, y + h - 1, w, color);  /* 下边 */
    fb_gfx_drawFastVLine(&fb, x, y, h, color);          /* 左边 */
    fb_gfx_drawFastVLine(&fb, x + w - 1, y, h, color);  /* 右边 */
}

/**
 * @brief 读取指定坐标的像素值（16位 RGB565）
 *
 * 从摄像头帧缓冲区读取指定位置的像素颜色值。
 *
 * @param x X 坐标
 * @param y Y 坐标
 * @return 16位 RGB565 颜色值
 */
uint16_t RGB_ReadBit16Point(unsigned short x, unsigned short y)
{
    if (fb != NULL)
        return fb->buf[2 * x + 320 * 2 * y] << 8 | fb->buf[2 * x + 240 * 2 * y + 1];
    else
        return 0;
}

/**
 * @brief 摄像头图像获取和颜色追踪任务
 *
 * 在独立任务中持续执行：
 * 1. 获取摄像头帧缓冲区
 * 2. 执行颜色识别（依次检测绿色、红色、蓝色）
 * 3. 在识别到的目标上绘制边框
 * 4. 更新 LVGL 图像控件显示
 * 5. 释放帧缓冲区
 * 6. 计算并打印帧率
 *
 * @param pvParameters 任务参数（未使用）
 */
void Cam_Task(void *pvParameters)
{
    while (1)
    {
        static int64_t last_frame = 0;

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
            /* 尝试识别绿色 */
            if (Trace(&Condition[0], &Resu))
            {
                printf("识别到绿色 x:%d y:%d w:%d h:%d ", Resu.x, Resu.y, Resu.w, Resu.h);
                if (Resu.x > 0 && Resu.y > 0)
                {
                    /* 在目标周围绘制边框 */
                    draw_fillRect(fb, Resu.x - Resu.w / 2, Resu.y - Resu.h / 2, Resu.w, Resu.h);
                }
            }
            /* 尝试识别红色 */
            else if (Trace(&Condition[1], &Resu))
            {
                printf("识别到红色 x:%d y:%d w:%d h:%d ", Resu.x, Resu.y, Resu.w, Resu.h);
                if (Resu.x > 0 && Resu.y > 0)
                {
                    draw_fillRect(fb, Resu.x - Resu.w / 2, Resu.y - Resu.h / 2, Resu.w, Resu.h);
                }
            }
            /* 尝试识别蓝色 */
            else if (Trace(&Condition[2], &Resu))
            {
                printf("识别到蓝色 x:%d y:%d w:%d h:%d ", Resu.x, Resu.y, Resu.w, Resu.h);
                if (Resu.x > 0 && Resu.y > 0)
                {
                    draw_fillRect(fb, Resu.x - Resu.w / 2, Resu.y - Resu.h / 2, Resu.w, Resu.h);
                }
            }

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
 * @brief 加载颜色追踪显示页面
 *
 * 初始化流程：
 * 1. 初始化摄像头硬件
 * 2. 创建图像显示控件
 * 3. 创建图像获取和颜色追踪任务（绑定到 CPU 核心 0）
 */
void page_cam_load()
{
    app_camera_init();  /* 初始化摄像头 */
    imgcam_init();       /* 初始化图像控件 */

    /* 创建摄像头任务，绑定到 CPU 核心 0 */
    xTaskCreatePinnedToCore(&Cam_Task, "Cam_Task", 1024 * 5, NULL, 14, NULL, 0);
}
