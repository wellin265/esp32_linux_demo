/**
 * @file app_camera.c
 * @brief 摄像头硬件配置与初始化实现
 *
 * OV2640 摄像头的引脚配置和驱动初始化。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_camera.h"
#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

/* ========== 摄像头引脚配置（KSDIY Korvo 开发板） ========== */

#define CAM_PIN_PWDN   -1    /**< 电源引脚（未使用） */
#define CAM_PIN_RESET  -1    /**< 复位引脚（软件复位） */
#define CAM_PIN_XCLK   40    /**< 系统时钟 */
#define CAM_PIN_SIOD   17    /**< I2C SDA (SCCB) */
#define CAM_PIN_SIOC   18    /**< I2C SCL (SCCB) */

/* 数据引脚 D0-D7 */
#define CAM_PIN_D7     39
#define CAM_PIN_D6     41
#define CAM_PIN_D5     42
#define CAM_PIN_D4     12
#define CAM_PIN_D3      3
#define CAM_PIN_D2     14
#define CAM_PIN_D1     47
#define CAM_PIN_D0     13

/* 同步信号引脚 */
#define CAM_PIN_VSYNC  21    /**< 垂直同步 */
#define CAM_PIN_HREF   38    /**< 水平参考 */
#define CAM_PIN_PCLK   11    /**< 像素时钟 */

#if ESP_CAMERA_SUPPORTED

/**
 * @brief 摄像头硬件配置（静态）
 */
static camera_config_t s_camera_config = {
    .pin_pwdn     = CAM_PIN_PWDN,
    .pin_reset    = CAM_PIN_RESET,
    .pin_xclk     = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7    = CAM_PIN_D7,
    .pin_d6    = CAM_PIN_D6,
    .pin_d5    = CAM_PIN_D5,
    .pin_d4    = CAM_PIN_D4,
    .pin_d3    = CAM_PIN_D3,
    .pin_d2    = CAM_PIN_D2,
    .pin_d1    = CAM_PIN_D1,
    .pin_d0    = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href  = CAM_PIN_HREF,
    .pin_pclk  = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,         /**< 20MHz 时钟 */
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,   /**< JPEG 格式传输效率高 */
    .frame_size   = FRAMESIZE_VGA,    /**< VGA (640x480) */
    .jpeg_quality = 12,               /**< JPEG 质量 0-63，越小越高 */
    .fb_count     = 2,                /**< 2 个帧缓冲区（连续模式） */
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

#endif /* ESP_CAMERA_SUPPORTED */

esp_err_t app_camera_init(void)
{
#if ESP_CAMERA_SUPPORTED
    esp_err_t err = esp_camera_init(&s_camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败");
        return err;
    }
    return ESP_OK;
#else
    ESP_LOGE(TAG, "当前芯片不支持摄像头");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
