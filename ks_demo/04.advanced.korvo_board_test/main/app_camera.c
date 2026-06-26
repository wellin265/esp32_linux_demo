/**
 * @file app_camera.c
 * @brief 摄像头驱动模块
 *
 * 本模块实现 OV 系列（OV2640/OV5620）摄像头初始化和图像采集功能。
 * 配置摄像头接口为 DVP 并行模式，输出 RGB565 格式图像。
 *
 * 硬件连接（KSDIY Korvo ESP32-S3）:
 *   - XCLK:  GPIO40 (20MHz)
 *   - D0-D7: GPIO13,47,14,3,12,42,41,39
 *   - VSYNC: GPIO21, HREF: GPIO38, PCLK: GPIO11
 *   - SCCB(I2C): 通过 I2C 总线通信
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_camera.h"

static const char *TAG = "app_camera";
static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
//    .fb_location = CAMERA_FB_IN_PSRAM,//存放在外部PSRAM中
    .fb_location = CAMERA_FB_IN_DRAM,
    .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_96X96,     //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
                                      //     .frame_size = FRAMESIZE_240X240,
    .jpeg_quality = 12,               //0-63 lower number means higher quality
    .fb_count = 2,                    //if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY};
esp_err_t app_camera_init()
{
    // camera init
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 0);
    s->set_hmirror(s, 1);
    return err;
}
