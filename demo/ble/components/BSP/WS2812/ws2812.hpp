#ifndef __WS2812_H
#define __WS2812_H

#include "driver/rmt_types.h"
#include "esp_err.h"
#include "hal/gpio_types.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include <stdint.h>

/* ========== 配置结构体 ========== */

struct Ws2812Config {
    gpio_num_t                   pin            = GPIO_NUM_48;     /* 数据引脚 */
    uint32_t                     max_leds       = 1;               /* LED 数量 */
    led_model_t                  led_model      = LED_MODEL_WS2812;/* LED 型号 */
    led_color_component_format_t color_fmt      = LED_STRIP_COLOR_COMPONENT_FMT_GRB; /* 色彩顺序 */
    uint32_t                     resolution_hz  = 10 * 1000 * 1000;/* RMT 分辨率 */
    bool                         with_dma       = false;           /* 启用 DMA 批量传输 */
    bool                         invert_out     = false;           /* 反相输出 */
};

/* ========== 颜色结构体 ========== */

struct Ws2812Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    Ws2812Color() : r(0), g(0), b(0) {}
    Ws2812Color(uint8_t red, uint8_t green, uint8_t blue)
        : r(red), g(green), b(blue) {}
};

/* 常用颜色预设 */
#define WS2812_RED      Ws2812Color(255, 0, 0)
#define WS2812_GREEN    Ws2812Color(0, 255, 0)
#define WS2812_BLUE     Ws2812Color(0, 0, 255)
#define WS2812_WHITE    Ws2812Color(255, 255, 255)
#define WS2812_BLACK    Ws2812Color(0, 0, 0)
#define WS2812_YELLOW   Ws2812Color(255, 255, 0)
#define WS2812_CYAN     Ws2812Color(0, 255, 255)
#define WS2812_MAGENTA  Ws2812Color(255, 0, 255)

/* ========== 类声明 ========== */

class Ws2812 {
public:
    Ws2812(const Ws2812Config &cfg = {});

    esp_err_t init();

    /* 常规操作 (led_strip 组件) */
    esp_err_t setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);
    esp_err_t setPixel(uint32_t index, const Ws2812Color &color);
    esp_err_t refresh(uint32_t timeout_ms = 100);
    esp_err_t clear(uint32_t timeout_ms = 100);
    esp_err_t setAll(const Ws2812Color &color);
    esp_err_t setAll(uint8_t red, uint8_t green, uint8_t blue);

    /* DMA 批量传输 — 直接将像素缓冲区通过 RMT DMA 发出 */
    esp_err_t setAllDma(const uint8_t *pixel_data, uint32_t num_leds);
    esp_err_t setAllDma(const Ws2812Color &color);
    esp_err_t waitDone(uint32_t timeout_ms = 100);

    /* 获取内部句柄 */
    led_strip_handle_t  ledStripHandle() const { return m_handle; }
    uint32_t            ledCount()       const { return m_cfg.max_leds; }

    /* DMA 同步标志 — RMT 发送完成回调设置 */
    volatile bool refreshDone = false;

private:
    esp_err_t initRmtDma();
    esp_err_t transmitDma(const uint8_t *grb_data, uint32_t num_leds);

    Ws2812Config         m_cfg;
    led_strip_handle_t   m_handle       = nullptr;
    rmt_channel_handle_t m_tx_channel   = nullptr;   /* DMA 专用 RMT 通道 */
    rmt_encoder_handle_t m_encoder      = nullptr;   /* RMT bytes 编码器 */
    bool                 m_initialized  = false;
    bool                 m_dma_ready    = false;
};

/* ========== C 兼容层声明 ========== */

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ws2812_init(void);
esp_err_t ws2812_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t ws2812_refresh(uint32_t timeout_ms);
esp_err_t ws2812_clear(uint32_t timeout_ms);
esp_err_t ws2812_set_all(uint8_t red, uint8_t green, uint8_t blue);
esp_err_t ws2812_set_all_dma(const uint8_t *pixel_data, uint32_t num_leds);
esp_err_t ws2812_wait_done(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
