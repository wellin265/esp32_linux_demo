#ifndef __MYIIC_H
#define __MYIIC_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

/* I2C 总线: 共用 TCA9554 (0x20) + CST816S (0x15) */
#define IIC_NUM_PORT       I2C_NUM_0        /* I2C 端口 0 */
#define IIC_SPEED_CLK      400000           /* 总线速率 400kHz */
#define IIC_SDA_GPIO_PIN   GPIO_NUM_17      /* SDA */
#define IIC_SCL_GPIO_PIN   GPIO_NUM_18      /* SCL */

class MyIic {
public:
    static MyIic& inst();
    esp_err_t init();
    i2c_master_bus_handle_t handle() const;

protected:
    MyIic() = default;
    static bool s_initialized;
    static i2c_master_bus_handle_t s_bus_handle;
};

extern i2c_master_bus_handle_t bus_handle;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t myiic_init(void);

#ifdef __cplusplus
}
#endif

#endif
