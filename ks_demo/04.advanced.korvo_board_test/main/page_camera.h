/**
 * @file page_camera.h
 * @brief 摄像头测试页面头文件
 *
 * 提供摄像头实时预览测试页面的注册接口。
 * 页面显示 OV2640 摄像头的实时 RGB565 图像预览。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#ifndef PAGE_CAMERA_H
#define PAGE_CAMERA_H

#include "page_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 摄像头测试页面实例（需注册到页面管理器） */
extern page_t page_camera;

#ifdef __cplusplus
}
#endif

#endif /* PAGE_CAMERA_H */
