#ifndef __TCA9554_H
#define __TCA9554_H

#include "myiic.hpp"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
#include "esp_err.h"

/* TCA9554(A) I2C 地址 (A0/A1/A2 全接 GND) — 先试 TCA9554 再试 TCA9554A
 * 默认值已移至 Tca9554Config 结构体:
 *   i2c_addr=ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000
 *   i2c_addr_fallback=ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000
 *   power_pins=IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2
// #define TCA9554_I2C_ADDR        ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000
// #define TCA9554A_I2C_ADDR       ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000
 */

struct Tca9554Config {
    uint8_t i2c_addr = ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000;
    uint8_t i2c_addr_fallback = ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000;
    uint32_t power_pins = IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2;
};

/* TCA9554 引脚定义 */
#define TCA9554_PIN_0           IO_EXPANDER_PIN_NUM_0
#define TCA9554_PIN_1           IO_EXPANDER_PIN_NUM_1
#define TCA9554_PIN_2           IO_EXPANDER_PIN_NUM_2
#define TCA9554_PIN_3           IO_EXPANDER_PIN_NUM_3
#define TCA9554_PIN_4           IO_EXPANDER_PIN_NUM_4
#define TCA9554_PIN_5           IO_EXPANDER_PIN_NUM_5
#define TCA9554_PIN_6           IO_EXPANDER_PIN_NUM_6
#define TCA9554_PIN_7           IO_EXPANDER_PIN_NUM_7

/* 项目引脚分配

   重要: P1 是 LCD 模组供电使能 (不是背光!) — 拉低 = LCD 断电黑屏
         P2 是 LCD + Touch 使能信号 — 保持 HIGH 即可, 不要翻转做硬复位
         P3 未使用 (CS 由 GPIO 46 直连控制)
         P4 未使用 (Touch 中断未接)
*/
#define TCA9554_PIN_PWR         TCA9554_PIN_1   /* LCD 供电使能 (P1=LOW → LCD 断电) */
#define TCA9554_PIN_EN          TCA9554_PIN_2   /* LCD+Touch 使能 */
#define TCA9554_PIN_BLK         TCA9554_PIN_PWR /* 兼容旧名: 供电使能 */
#define TCA9554_PIN_RST         TCA9554_PIN_EN  /* 兼容旧名: 使能信号 */

class Tca9554 {
public:
    Tca9554(MyIic &i2c, const Tca9554Config &dev_cfg = {});
    esp_err_t init();
    esp_err_t pinSetLevel(uint32_t pin_mask, uint8_t level);
    esp_err_t pinGetLevel(uint32_t pin_mask, uint32_t *level);
    esp_err_t pinSetDir(uint32_t pin_mask, esp_io_expander_dir_t dir);
    esp_io_expander_handle_t handle() const;

private:
    MyIic &m_i2c;
    Tca9554Config m_dev_cfg;
    esp_io_expander_handle_t expander_handle = nullptr;
};

extern esp_io_expander_handle_t tca9554_handle;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tca9554_init(void);
esp_err_t tca9554_pin_set_level(uint32_t pin_mask, uint8_t level);
esp_err_t tca9554_pin_get_level(uint32_t pin_mask, uint32_t *level);

#ifdef __cplusplus
}
#endif

#endif
