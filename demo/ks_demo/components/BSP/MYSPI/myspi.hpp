#ifndef __MY_SPI_H
#define __MY_SPI_H

#include <unistd.h>
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"

/* SPI 总线: 驱动 ST7789 LCD (280x240, 16bit RGB565) */
#define SPI_SCLK_PIN        GPIO_NUM_1       /* SCLK */
#define SPI_MOSI_PIN        GPIO_NUM_0       /* MOSI */
#define SPI_MISO_PIN        GPIO_NUM_NC      /* MISO (未使用) */

/* SPI 主机端口 */
#define MY_SPI_HOST         SPI2_HOST

class MySpi {
public:
    static MySpi& inst();
    esp_err_t init();

protected:
    MySpi() = default;
    static bool s_initialized;
    static constexpr spi_host_device_t s_host = MY_SPI_HOST;
};

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t my_spi_init(void);

#ifdef __cplusplus
}
#endif

#endif
