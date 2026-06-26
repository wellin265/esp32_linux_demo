/**
 * @file app_camera.h
 * @brief 摄像头初始化和配置头文件
 *
 * 本头文件定义摄像头引脚配置和初始化函数接口。
 * 适用于 ESP32-S3 Korvo 开发板。
 *
 * @author Kevincoooool
 * @date 2021-05-25
 */

#ifndef MATERIALS_CLASSIFIER_ESP_APP_CAMERA_ESP_H_
#define MATERIALS_CLASSIFIER_ESP_APP_CAMERA_ESP_H_

#include "esp_camera.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sensor.h"

/**
 * @brief 摄像头像素格式选项
 *
 * - PIXFORMAT_RGB565:    2BPP/RGB565，适合直接显示
 * - PIXFORMAT_YUV422:    2BPP/YUV422
 * - PIXFORMAT_GRAYSCALE: 1BPP/灰度图
 * - PIXFORMAT_JPEG:      JPEG/压缩格式
 * - PIXFORMAT_RGB888:    3BPP/RGB888
 */
#define CAMERA_PIXEL_FORMAT PIXFORMAT_RGB565

/**
 * @brief 摄像头帧大小选项
 *
 * - FRAMESIZE_96X96:     96x96
 * - FRAMESIZE_QQVGA:     160x120
 * - FRAMESIZE_QQVGA2:    128x160
 * - FRAMESIZE_QCIF:      176x144
 * - FRAMESIZE_HQVGA:     240x176
 * - FRAMESIZE_240X240:   240x240
 * - FRAMESIZE_QVGA:      320x240（本示例使用）
 * - FRAMESIZE_CIF:       400x296
 * - FRAMESIZE_VGA:       640x480
 * - FRAMESIZE_SVGA:      800x600
 * - FRAMESIZE_XGA:       1024x768
 * - FRAMESIZE_SXGA:      1280x1024
 * - FRAMESIZE_UXGA:      1600x1200
 */
#define CAMERA_FRAME_SIZE FRAMESIZE_QVGA

/* ===== 摄像头引脚配置（ESP32-S3 Korvo 开发板） ===== */
#define CAM_PIN_PWDN -1    /**< 电源down引脚（未使用） */
#define CAM_PIN_RESET -1   /**< 复位引脚（未使用） */
#define CAM_PIN_XCLK 40    /**< 系统时钟引脚 */
#define CAM_PIN_SIOD 17    /**< I2C 数据引脚 */
#define CAM_PIN_SIOC 18    /**< I2C 时钟引脚 */

#define CAM_PIN_D7 39      /**< 数据位 D7 */
#define CAM_PIN_D6 41      /**< 数据位 D6 */
#define CAM_PIN_D5 42      /**< 数据位 D5 */
#define CAM_PIN_D4 12      /**< 数据位 D4 */
#define CAM_PIN_D3 3       /**< 数据位 D3 */
#define CAM_PIN_D2 14      /**< 数据位 D2 */
#define CAM_PIN_D1 47      /**< 数据位 D1 */
#define CAM_PIN_D0 13      /**< 数据位 D0 */
#define CAM_PIN_VSYNC 21   /**< 垂直同步引脚 */
#define CAM_PIN_HREF 38    /**< 水平参考引脚 */
#define CAM_PIN_PCLK 11    /**< 像素时钟引脚 */

/** @brief 摄像头翻转标志 */
#define FLIP_CAMERA 1

/** @brief XCLK 时钟频率 (20MHz) */
#define XCLK_FREQ 20000000

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 初始化摄像头
 *
 * 配置并初始化摄像头模块，设置引脚、时钟、图像格式等参数。
 */
void app_camera_init(void);

/**
 * @brief 初始化网络摄像头模式
 *
 * 用于网络摄像头应用的初始化配置。
 */
void web_camera_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MATERIALS_CLASSIFIER_ESP_APP_CAMERA_ESP_H_ */
