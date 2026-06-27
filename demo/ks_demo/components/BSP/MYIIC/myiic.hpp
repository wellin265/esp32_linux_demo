#ifndef __MYIIC_H
#define __MYIIC_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

/* I2C 总线: 共用 TCA9554 (0x20) + CST816S (0x15) */

/* 默认值已移至 I2cBusConfig 结构体:
 *   port=I2C_NUM_0,  sda=GPIO_NUM_17,  scl=GPIO_NUM_18
 *   clk_speed=400000, enable_internal_pullup=true, glitch_ignore_cnt=7
// #define IIC_NUM_PORT       I2C_NUM_0
// #define IIC_SPEED_CLK      400000
// #define IIC_SDA_GPIO_PIN   GPIO_NUM_17
// #define IIC_SCL_GPIO_PIN   GPIO_NUM_18
 */

struct I2cBusConfig {
    i2c_port_t port = I2C_NUM_0;
    gpio_num_t sda_io = GPIO_NUM_17;
    gpio_num_t scl_io = GPIO_NUM_18;
    uint32_t clk_speed = 400000;
    bool enable_internal_pullup = true;
    uint8_t glitch_ignore_cnt = 7;
};

class MyIic {
public:
    explicit MyIic(const I2cBusConfig &cfg = {});
    MyIic(gpio_num_t sda, gpio_num_t scl);
    MyIic(gpio_num_t sda, gpio_num_t scl, uint32_t clk_speed);
    esp_err_t init();
    i2c_master_bus_handle_t handle() const;

private:
    I2cBusConfig m_cfg;
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
