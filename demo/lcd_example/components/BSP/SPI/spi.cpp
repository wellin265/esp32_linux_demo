/**
 * SPI2 总线驱动
 *
 * 关键信息:
 *   - 总线: SPI2_HOST
 *   - 引脚: MOSI=5, MISO=6, SCLK=4, SD_CS=2
 *   - LCD 通过 esp_lcd_new_panel_io_spi 内部管理自己设备 (CS=17)
 */

#include "spi.hpp"
#include "esp_log.h"

static const char *TAG = "SPI";

/* ========== 构造 ========== */

Spi::Spi(const SpiConfig &cfg)
    : m_cfg(cfg) {}

/* ========== 硬件初始化 ========== */

esp_err_t Spi::init() {
    if (m_initialized) return ESP_OK;

    /* 1. 总线配置 */
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num     = m_cfg.pin_sclk;
    buscfg.mosi_io_num     = m_cfg.pin_mosi;
    buscfg.miso_io_num     = m_cfg.pin_miso;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = m_cfg.max_transfer_sz;

    ESP_ERROR_CHECK(spi_bus_initialize(m_cfg.host, &buscfg, SPI_DMA_CH_AUTO));

    /* 2. SD 卡设备 (保留原有配置) */
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = m_cfg.clock_speed_hz;
    devcfg.mode           = 0;
    devcfg.spics_io_num   = m_cfg.pin_sd_cs;
    devcfg.queue_size     = m_cfg.queue_size;

    ESP_ERROR_CHECK(spi_bus_add_device(m_cfg.host, &devcfg, &m_sd_handle));

    m_initialized = true;
    ESP_LOGI(TAG, "SPI initialized");
    return ESP_OK;
}

/* ========== C 兼容层 ========== */

extern "C" {

static SpiConfig s_spi_cfg;
static Spi      s_spi(s_spi_cfg);

esp_err_t spi_init(void) {
    return s_spi.init();
}

}
