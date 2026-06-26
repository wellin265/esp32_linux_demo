/**
 * I2C 总线 (GPIO 17=SDA, GPIO 18=SCL, I2C_NUM_0, 内部上拉)
 *
 * 共用设备: TCA9554@0x20 (IO 扩展) + CST816S@0x15 (触摸)
 * 单例模式: 多设备共享同一条 I2C 总线, s_initialized 防止重复初始化
 */

#include "myiic.hpp"

bool MyIic::s_initialized = false;
i2c_master_bus_handle_t MyIic::s_bus_handle = nullptr;

MyIic& MyIic::inst() { static MyIic i; return i; }

esp_err_t MyIic::init() {
    if (s_initialized) return ESP_OK;

    i2c_master_bus_config_t i2c_bus_config = {};
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = IIC_NUM_PORT;
    i2c_bus_config.scl_io_num = IIC_SCL_GPIO_PIN;
    i2c_bus_config.sda_io_num = IIC_SDA_GPIO_PIN;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &s_bus_handle));
    bus_handle = s_bus_handle;
    s_initialized = true;
    return ESP_OK;
}

i2c_master_bus_handle_t MyIic::handle() const { return s_bus_handle; }

i2c_master_bus_handle_t bus_handle = nullptr;

extern "C" {

esp_err_t myiic_init(void) {
    esp_err_t ret = MyIic::inst().init();
    bus_handle = MyIic::inst().handle();
    return ret;
}

}
