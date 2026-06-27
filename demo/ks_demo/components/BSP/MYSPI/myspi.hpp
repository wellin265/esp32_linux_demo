#ifndef __MY_SPI_H
#define __MY_SPI_H

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"

/* SPI 总线: 驱动 ST7789 LCD (280x240, 16bit RGB565) */

struct SpiBusConfig {
    spi_host_device_t host = SPI2_HOST;
    gpio_num_t mosi_io = GPIO_NUM_0;
    gpio_num_t sclk_io = GPIO_NUM_1;
    gpio_num_t miso_io = GPIO_NUM_NC;
    int max_transfer_size = 320 * 240 * sizeof(uint16_t);
};

class MySpi {
public:
    explicit MySpi(const SpiBusConfig &cfg = {});
    MySpi(gpio_num_t mosi, gpio_num_t sclk);
    MySpi(gpio_num_t mosi, gpio_num_t sclk, spi_host_device_t host);
    esp_err_t init();
    spi_host_device_t host() const { return m_cfg.host; }

protected:
    SpiBusConfig m_cfg;
    static bool s_initialized;
};

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t my_spi_init(void);

#ifdef __cplusplus
}
#endif

#endif
