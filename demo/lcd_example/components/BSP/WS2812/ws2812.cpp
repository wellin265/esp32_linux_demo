/**
 * WS2812 可寻址 RGB LED 驱动
 *
 * 关键信息:
 *   - 总线: RMT 单线协议 (常规操作) + RMT DMA (批量传输)
 *   - 依赖: espressif/led_strip 组件 (v3.x)
 *   - DMA:  需要 Ws2812Config::with_dma = true, 独立的 RMT TX 通道
 *   - 可移植性: 更换引脚只需改 Ws2812Config::pin
 */

#include "ws2812.hpp"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char *TAG = "WS2812";

/* ========== 时序计算 (tick @ resolution_hz) ========== */

#define NS_TO_TICK(ns, hz)  (((uint64_t)(ns) * (uint64_t)(hz)) / 1000000000ULL)

/* WS2812 标准时序 (ns) */
#define WS2812_T0H_NS   400
#define WS2812_T0L_NS   850
#define WS2812_T1H_NS   800
#define WS2812_T1L_NS   450

/* ========== DMA 传输完成回调 ========== */

static bool onTxDone(rmt_channel_handle_t tx_chan,
                     const rmt_tx_done_event_data_t *edata, void *user_ctx)
{
    Ws2812 *ws2812 = static_cast<Ws2812 *>(user_ctx);
    ws2812->refreshDone = true;
    return false;
}

/* ========== 构造 ========== */

Ws2812::Ws2812(const Ws2812Config &cfg)
    : m_cfg(cfg) {}

/* ========== 硬件初始化 ========== */

esp_err_t Ws2812::init() {
    if (m_initialized) return ESP_OK;

    /* ---- 1. led_strip 组件 (常规操作) ---- */
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num         = m_cfg.pin;
    strip_config.max_leds               = m_cfg.max_leds;
    strip_config.led_model              = m_cfg.led_model;
    strip_config.color_component_format = m_cfg.color_fmt;
    strip_config.flags.invert_out       = m_cfg.invert_out;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src        = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz  = m_cfg.resolution_hz;
    rmt_config.flags.with_dma = m_cfg.with_dma;

    ESP_RETURN_ON_ERROR(
        led_strip_new_rmt_device(&strip_config, &rmt_config, &m_handle),
        TAG, "Failed to create led_strip device");

    ESP_RETURN_ON_ERROR(
        led_strip_clear(m_handle),
        TAG, "Failed to clear LEDs");

    /* ---- 2. DMA 专用 RMT 通道 (批量传输) ---- */
    if (m_cfg.with_dma) {
        ESP_RETURN_ON_ERROR(initRmtDma(), TAG, "Failed to init RMT DMA");
    }

    m_initialized = true;
    ESP_LOGI(TAG, "WS2812 initialized (pin=%d, leds=%lu, dma=%s)",
             m_cfg.pin, m_cfg.max_leds, m_cfg.with_dma ? "on" : "off");
    return ESP_OK;
}

/* ========== DMA RMT 通道初始化 ========== */

esp_err_t Ws2812::initRmtDma() {
    /* 1. 创建 RMT TX 通道 (独立于 led_strip, 共享 GPIO) */
    rmt_tx_channel_config_t tx_cfg = {};
    tx_cfg.gpio_num          = m_cfg.pin;
    tx_cfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    tx_cfg.resolution_hz     = m_cfg.resolution_hz;
    tx_cfg.mem_block_symbols = 64;
    tx_cfg.trans_queue_depth = 1;
    tx_cfg.flags.with_dma    = true;

    ESP_RETURN_ON_ERROR(
        rmt_new_tx_channel(&tx_cfg, &m_tx_channel),
        TAG, "Failed to create RMT TX DMA channel");

    /* 2. 注册 DMA 完成回调 */
    rmt_tx_event_callbacks_t cbs = {};
    cbs.on_trans_done = onTxDone;
    ESP_RETURN_ON_ERROR(
        rmt_tx_register_event_callbacks(m_tx_channel, &cbs, this),
        TAG, "Failed to register TX callback");

    /* 3. 创建 bytes 编码器 (WS2812 时序) */
    rmt_bytes_encoder_config_t enc_cfg = {};
    enc_cfg.bit0.level0    = 1;
    enc_cfg.bit0.duration0 = NS_TO_TICK(WS2812_T0H_NS, m_cfg.resolution_hz);
    enc_cfg.bit0.level1    = 0;
    enc_cfg.bit0.duration1 = NS_TO_TICK(WS2812_T0L_NS, m_cfg.resolution_hz);
    enc_cfg.bit1.level0    = 1;
    enc_cfg.bit1.duration0 = NS_TO_TICK(WS2812_T1H_NS, m_cfg.resolution_hz);
    enc_cfg.bit1.level1    = 0;
    enc_cfg.bit1.duration1 = NS_TO_TICK(WS2812_T1L_NS, m_cfg.resolution_hz);
    enc_cfg.flags.msb_first = true;

    ESP_RETURN_ON_ERROR(
        rmt_new_bytes_encoder(&enc_cfg, &m_encoder),
        TAG, "Failed to create bytes encoder");

    /* 4. 使能 RMT 通道 */
    ESP_RETURN_ON_ERROR(
        rmt_enable(m_tx_channel),
        TAG, "Failed to enable RMT TX channel");

    m_dma_ready = true;
    ESP_LOGI(TAG, "RMT DMA channel ready (resolution=%lu Hz)", m_cfg.resolution_hz);
    return ESP_OK;
}

/* ========== DMA 传输核心 ========== */

esp_err_t Ws2812::transmitDma(const uint8_t *grb_data, uint32_t num_leds) {
    if (!m_dma_ready) {
        ESP_LOGE(TAG, "DMA not initialized, set with_dma=true in config");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (num_leds > m_cfg.max_leds) {
        ESP_LOGE(TAG, "num_leds %lu exceeds max %lu", num_leds, m_cfg.max_leds);
        return ESP_ERR_INVALID_ARG;
    }

    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count      = 0;
    tx_cfg.flags.eot_level = 0;  /* 传输结束后保持低电平 → ≥50μs 复位码 */

    refreshDone = false;

    ESP_RETURN_ON_ERROR(
        rmt_transmit(m_tx_channel, m_encoder, grb_data,
                     num_leds * 3, &tx_cfg),
        TAG, "DMA transmit failed");

    return ESP_OK;
}

/* ========== setAllDma: 原始 GRB 缓冲区 ========== */

esp_err_t Ws2812::setAllDma(const uint8_t *pixel_data, uint32_t num_leds) {
    if (!pixel_data) return ESP_ERR_INVALID_ARG;

    size_t buf_size = num_leds * 3;
    uint8_t *dma_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA);

    if (!dma_buf) {
        ESP_LOGE(TAG, "DMA buffer alloc failed (%u bytes)", buf_size);
        return ESP_ERR_NO_MEM;
    }

    memcpy(dma_buf, pixel_data, buf_size);

    /* 异步 DMA 传输 — rmt_transmit 内部拷贝到 RMT DMA buffer */
    esp_err_t ret = transmitDma(dma_buf, num_leds);

    /* 缓冲区可以立即释放: rmt_transmit 内部已拷贝数据到 RMT 的编码器/DMA 缓存 */
    heap_caps_free(dma_buf);

    return ret;
}

/* ========== setAllDma: 统一颜色 ========== */

esp_err_t Ws2812::setAllDma(const Ws2812Color &color) {
    size_t buf_size = m_cfg.max_leds * 3;
    uint8_t *dma_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA);

    if (!dma_buf) {
        ESP_LOGE(TAG, "DMA buffer alloc failed (%u bytes)", buf_size);
        return ESP_ERR_NO_MEM;
    }

    /* 填充 GRB 数据 (WS2812 原生顺序) */
    for (uint32_t i = 0; i < m_cfg.max_leds; i++) {
        dma_buf[i * 3 + 0] = color.g;
        dma_buf[i * 3 + 1] = color.r;
        dma_buf[i * 3 + 2] = color.b;
    }

    esp_err_t ret = transmitDma(dma_buf, m_cfg.max_leds);
    heap_caps_free(dma_buf);

    return ret;
}

/* ========== 等待 DMA 完成 ========== */

esp_err_t Ws2812::waitDone(uint32_t timeout_ms) {
    if (!m_dma_ready) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    TickType_t start = xTaskGetTickCount();

    while (!refreshDone) {
        if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(timeout_ms)) {
            ESP_LOGW(TAG, "DMA wait timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    return ESP_OK;
}

/* ================================================================
 * 常规操作 (委托给 led_strip 组件)
 * ================================================================ */

esp_err_t Ws2812::setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue) {
    if (index >= m_cfg.max_leds) {
        ESP_LOGE(TAG, "Index %lu out of range (max %lu)", index, m_cfg.max_leds);
        return ESP_ERR_INVALID_ARG;
    }
    return led_strip_set_pixel(m_handle, index, red, green, blue);
}

esp_err_t Ws2812::setPixel(uint32_t index, const Ws2812Color &color) {
    return setPixel(index, color.r, color.g, color.b);
}

esp_err_t Ws2812::refresh(uint32_t timeout_ms) {
    return led_strip_refresh(m_handle);
}

esp_err_t Ws2812::clear(uint32_t timeout_ms) {
    return led_strip_clear(m_handle);
}

esp_err_t Ws2812::setAll(const Ws2812Color &color) {
    for (uint32_t i = 0; i < m_cfg.max_leds; i++) {
        ESP_RETURN_ON_ERROR(
            led_strip_set_pixel(m_handle, i, color.r, color.g, color.b),
            TAG, "Failed to set pixel %lu", i);
    }
    return led_strip_refresh(m_handle);
}

esp_err_t Ws2812::setAll(uint8_t red, uint8_t green, uint8_t blue) {
    return setAll(Ws2812Color(red, green, blue));
}

/* ================================================================
 * C 兼容层
 * ================================================================ */

extern "C" {

static Ws2812Config s_ws2812_cfg;
static Ws2812      s_ws2812(s_ws2812_cfg);

esp_err_t ws2812_init(void)                                          { return s_ws2812.init(); }
esp_err_t ws2812_set_pixel(uint32_t i, uint8_t r, uint8_t g, uint8_t b) { return s_ws2812.setPixel(i, r, g, b); }
esp_err_t ws2812_refresh(uint32_t ms)                                 { return s_ws2812.refresh(ms); }
esp_err_t ws2812_clear(uint32_t ms)                                   { return s_ws2812.clear(ms); }
esp_err_t ws2812_set_all(uint8_t r, uint8_t g, uint8_t b)             { return s_ws2812.setAll(r, g, b); }
esp_err_t ws2812_set_all_dma(const uint8_t *buf, uint32_t n)          { return s_ws2812.setAllDma(buf, n); }
esp_err_t ws2812_wait_done(uint32_t ms)                               { return s_ws2812.waitDone(ms); }

}
