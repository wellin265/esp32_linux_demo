#include "cst816s.hpp"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"

/**
 * CST816S 触摸驱动实现
 *
 * 关键:
 *   - getPoint() 调用 esp_lcd_touch_get_data() 后必须手动提取 data.x/data.y
 *   - 触摸坐标系 flags (swap_xy/mirror_x/mirror_y) 需与 LCD 方向匹配
 */

static const char *TAG = "CST816S";

Cst816s& Cst816s::inst() { static Cst816s c; return c; }

esp_err_t Cst816s::init(uint16_t x_max, uint16_t y_max) {
    if (tp_handle != nullptr) return ESP_OK;

    /* 确保 I2C 总线已初始化 */
    MyIic::init();

    /* I2C 设备配置: CST816S 地址 0x15, 时钟 100kHz */
    esp_lcd_panel_io_i2c_config_t io_config = {};
    io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
    io_config.scl_speed_hz = 100000;
    io_config.control_phase_bytes = 1;
    io_config.dc_bit_offset = 0;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 0;
    io_config.flags.dc_low_on_data = 0;
    io_config.flags.disable_control_phase = 1;

    esp_lcd_panel_io_handle_t io_handle = nullptr;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_bus_handle, &io_config, &io_handle));

    /* 触摸面板配置 */
    esp_lcd_touch_config_t tp_config = {};
    tp_config.x_max = x_max;
    tp_config.y_max = y_max;
    tp_config.rst_gpio_num = GPIO_NUM_NC;
    tp_config.int_gpio_num = GPIO_NUM_NC;
    tp_config.levels.reset = 0;
    tp_config.levels.interrupt = 0;

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(io_handle, &tp_config, &tp_handle));

    /* 初始方向: 横屏 */
    setRotation(1);

    ESP_LOGI(TAG, "CST816S initialized");
    return ESP_OK;
}

void Cst816s::setRotation(uint8_t rot) {
    if (tp_handle == nullptr) return;

    switch (rot) {
    case 0: /* 竖屏 240x280 */
        esp_lcd_touch_set_swap_xy(tp_handle, false);
        esp_lcd_touch_set_mirror_x(tp_handle, false);
        esp_lcd_touch_set_mirror_y(tp_handle, false);
        break;
    case 1: /* 横屏 280x240 */
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_x(tp_handle, false);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
        break;
    case 2: /* 竖屏倒置 240x280 */
        esp_lcd_touch_set_swap_xy(tp_handle, false);
        esp_lcd_touch_set_mirror_x(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
        break;
    case 3: /* 横屏倒置 280x240 */
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_x(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, false);
        break;
    }
}

esp_err_t Cst816s::readData() {
    return esp_lcd_touch_read_data(tp_handle);
}

esp_err_t Cst816s::getPoint(uint16_t *x, uint16_t *y, uint8_t *num) {
    esp_lcd_touch_point_data_t data;
    esp_err_t ret = esp_lcd_touch_get_data(tp_handle, &data, num, 1);
    if (ret == ESP_OK && *num > 0) {
        *x = data.x;
        *y = data.y;
    }
    return ret;
}

extern "C" {

esp_err_t cst816s_init(uint16_t x_max, uint16_t y_max) {
    return Cst816s::inst().init(x_max, y_max);
}

esp_err_t cst816s_read_data(void) {
    return Cst816s::inst().readData();
}

esp_err_t cst816s_get_point(uint16_t *x, uint16_t *y, uint8_t *num) {
    return Cst816s::inst().getPoint(x, y, num);
}

void cst816s_set_rotation(uint8_t rot) {
    Cst816s::inst().setRotation(rot);
}

}
