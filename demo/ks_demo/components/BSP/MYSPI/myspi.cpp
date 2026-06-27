/**
 * SPI 总线 (MOSI=0, SCLK=1, MISO=NC, SPI2_HOST)
 *
 * 驱动 ST7789 LCD, 80MHz, DMA 自动分配.
 * max_transfer_sz = 320×240×2 = 全屏一帧数据量
 */

#include "myspi.hpp"

bool MySpi::s_initialized = false;

MySpi::MySpi(const SpiBusConfig &cfg)
    : m_cfg(cfg) {}

MySpi::MySpi(gpio_num_t mosi, gpio_num_t sclk)
    : MySpi(SpiBusConfig{.mosi_io = mosi, .sclk_io = sclk}) {}

MySpi::MySpi(gpio_num_t mosi, gpio_num_t sclk, spi_host_device_t host)
    : MySpi(SpiBusConfig{.host = host, .mosi_io = mosi, .sclk_io = sclk}) {}

esp_err_t MySpi::init() {
    if (s_initialized) return ESP_OK;

    /* 按 IDF v5.5 spi_bus_config_t 字段声明顺序初始化 */
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = m_cfg.mosi_io;
    buscfg.miso_io_num = m_cfg.miso_io;
    buscfg.sclk_io_num = m_cfg.sclk_io;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.data4_io_num = -1;
    buscfg.data5_io_num = -1;
    buscfg.data6_io_num = -1;
    buscfg.data7_io_num = -1;
    buscfg.max_transfer_sz = m_cfg.max_transfer_size;

    ESP_ERROR_CHECK(spi_bus_initialize(m_cfg.host, &buscfg, SPI_DMA_CH_AUTO));

    s_initialized = true;
    return ESP_OK;
}

extern "C" {

esp_err_t my_spi_init(void) {
    static MySpi spi;
    return spi.init();
}

}
