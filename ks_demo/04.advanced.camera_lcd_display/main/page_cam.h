/**
 * @file page_cam.h
 * @brief 摄像头图像显示页面头文件
 *
 * 本头文件定义摄像头显示页面的函数接口。
 * 不使用 LVGL，直接将图像数据写入 LCD 显存。
 *
 * @author Kevincoooool
 * @date 2021-06-05
 */

#ifndef _PAGE_CAM_
#define _PAGE_CAM_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 摄像头图像获取任务
 *
 * 在独立任务中持续获取摄像头图像并更新 LCD 显示。
 *
 * @param pvParameters 任务参数（未使用）
 */
void Cam_Task(void *pvParameters);

/**
 * @brief 加载摄像头显示页面
 *
 * 初始化摄像头硬件并创建图像获取任务。
 */
void page_cam_load(void);

/**
 * @brief 结束摄像头显示页面
 *
 * 释放摄像头资源并停止显示任务。
 */
void page_cam_end(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PAGE_CAM_ */
