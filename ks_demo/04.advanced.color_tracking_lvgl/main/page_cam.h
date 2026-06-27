/**
 * @file page_cam.h
 * @brief 颜色追踪显示页面头文件
 *
 * 本头文件定义颜色追踪显示页面的函数接口。
 * 使用 EasyTracer 算法进行实时颜色识别和追踪。
 *
 * @author Kevincoooool
 * @date 2021-06-05
 */

#ifndef _PAGE_CAM_
#define _PAGE_CAM_

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 摄像头图像获取和颜色追踪任务
 *
 * 在独立任务中持续获取摄像头图像并执行颜色追踪。
 *
 * @param pvParameters 任务参数（未使用）
 */
void Cam_Task(void *pvParameters);

/**
 * @brief 加载颜色追踪显示页面
 *
 * 初始化摄像头硬件并创建图像获取任务。
 */
void page_cam_load(void);

/**
 * @brief 结束颜色追踪显示页面
 *
 * 释放摄像头资源并停止显示任务。
 */
void page_cam_end(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PAGE_CAM_ */
