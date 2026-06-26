#ifndef __CST816S_H
#define __CST816S_H

/**
 * @brief CST816S 电容触摸驱动 (I2C 0x15, 复用 MyIic 总线)
 *
 * 使用 ESP-IDF esp_lcd_touch 框架。坐标系需与 LCD 方向匹配:
 *   横屏 (280x240, swap_xy=true,  mirror_y=true)  → swap_xy=1, mirror_y=1
 *   竖屏 (240x280, swap_xy=false, mirror_x=true)  → swap_xy=0, mirror_y=0
 */

#include "myiic.hpp"
#include "esp_lcd_touch.h"

class Cst816s : public MyIic {
public:
    static Cst816s& inst();
    esp_err_t init(uint16_t x_max, uint16_t y_max);
    esp_err_t readData();
    esp_err_t getPoint(uint16_t *x, uint16_t *y, uint8_t *num);
    void setRotation(uint8_t rot);

    esp_lcd_touch_handle_t handle() const { return tp_handle; }

private:
    Cst816s() = default;
    esp_lcd_touch_handle_t tp_handle = nullptr;
};

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cst816s_init(uint16_t x_max, uint16_t y_max);
esp_err_t cst816s_read_data(void);
esp_err_t cst816s_get_point(uint16_t *x, uint16_t *y, uint8_t *num);
void cst816s_set_rotation(uint8_t rot);

#ifdef __cplusplus
}
#endif

#endif
