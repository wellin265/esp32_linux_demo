#ifndef __SPI_H
#define __SPI_H

#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "hal/spi_types.h"
#include "hal/gpio_types.h"
#include "esp_err.h"

/* ========== 配置结构体 ========== */

struct SpiConfig {
    spi_host_device_t host            = SPI2_HOST;
    gpio_num_t        pin_mosi        = GPIO_NUM_5;
    gpio_num_t        pin_miso        = GPIO_NUM_6;
    gpio_num_t        pin_sclk        = GPIO_NUM_4;
    gpio_num_t        pin_sd_cs       = GPIO_NUM_2;            /* SD 卡片选 */
    int               max_transfer_sz = 320 * 240 * 2;
    int               clock_speed_hz  = 20 * 1000 * 1000;
    int               queue_size      = 7;
};

/* ========== 类声明 ========== */

class Spi {
public:
    Spi(const SpiConfig &cfg = {});

    esp_err_t init();

    spi_host_device_t handle() const { return m_cfg.host; }

private:
    SpiConfig           m_cfg;
    bool                m_initialized = false;
    spi_device_handle_t m_sd_handle   = nullptr;
};

/* ========== C 兼容层声明 ========== */

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spi_init(void);

#ifdef __cplusplus
}
#endif

#endif
