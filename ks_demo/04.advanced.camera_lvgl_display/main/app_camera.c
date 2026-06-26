/**
 * @file app_camera.c
 * @brief 摄像头初始化和配置
 *
 * 本文件实现 ESP32-S3 摄像头的初始化和配置。
 * 支持 OV 系列摄像头模块（如 OV2640、OV3660 等）。
 *
 * @author Kevincoooool
 * @date 2021-05-25
 */

#include "app_camera.h"

/** @brief 日志标签 */
static const char *TAG = "app_camera";

/**
 * @brief 摄像头配置结构体
 *
 * 配置摄像头的引脚、时钟、图像格式等参数。
 * - 像素格式: RGB565（适合直接显示）
 * - 帧大小: QVGA (320x240)
 * - 帧缓冲区: 存放在 PSRAM 中
 * - 帧缓冲区数量: 2（双缓冲）
 */
static camera_config_t camera_config = {
    /* ===== 引脚配置 ===== */
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    /* 数据引脚 D0-D7 */
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,

    /* 同步信号引脚 */
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    /* ===== 时钟配置 ===== */
    .xclk_freq_hz = 10000000,    /* XCLK 10MHz */
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    /* ===== 帧缓冲区配置 ===== */
    .fb_location = CAMERA_FB_IN_PSRAM,  /* 存放在外部 PSRAM 中 */

    /* ===== 图像格式配置 ===== */
    .pixel_format = PIXFORMAT_RGB565,  /* RGB565 格式，适合直接显示 */
    .frame_size = FRAMESIZE_QVGA,      /* QVGA (320x240) 分辨率 */

    /* ===== JPEG 配置 ===== */
    .jpeg_quality = 12,  /* JPEG 质量 0-63，数值越小质量越高 */

    /* ===== 缓冲区配置 ===== */
    .fb_count = 2,              /* 双缓冲，提高帧率 */
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY  /* 缓冲区为空时抓取 */
};

/**
 * @brief 初始化摄像头
 *
 * 使用配置好的参数初始化摄像头模块。
 * 初始化成功后设置像素格式和彩色条纹测试模式。
 */
void app_camera_init()
{
    /* 初始化摄像头 */
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    /* 获取摄像头传感器配置 */
    sensor_t *s = esp_camera_sensor_get();
    s->set_pixformat(s, PIXFORMAT_RGB565);  /* 设置 RGB565 像素格式 */
    s->set_colorbar(s, 1);                  /* 启用彩色条纹测试模式 */
}
