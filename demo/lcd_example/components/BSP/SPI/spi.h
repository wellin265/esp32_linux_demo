#ifndef __SPI_H
#define __SPI_H

#include <unistd.h>
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"

#define SPI_MOSI_GPIO_PIN   GPIO_NUM_5         /* SPI2_MOSI */
#define SPI_CLK_GPIO_PIN    GPIO_NUM_4         /* SPI2_CLK */
#define SPI_MISO_GPIO_PIN   GPIO_NUM_6         /* SPI2_MISO */
/* 总线设备引脚定义 */
#define SD_CS_PIN           GPIO_NUM_2
/* SPI端口 */
#define SPI_HOST         SPI2_HOST

/* 函数声明 */
esp_err_t spi_init(void);    /* SPI初始化 */

#endif
