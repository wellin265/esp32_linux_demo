/**
 * TCA9554 / TCA9554A IO 扩展驱动 (I2C, 兼容两种地址)
 *
 * 引脚分配:
 *   P1 = LCD 模组供电使能 (⚠️ 不是背光! 拉 LOW = LCD 断电黑屏)
 *   P2 = LCD+Touch 使能 (保持 HIGH, 不翻转)
 *
 * 初始化流程:
 *   1. 先尝试 TCA9554 (0x20)
 *   2. 失败则尝试 TCA9554A (0x38)
 *   3. 找到后设 P1+P2 为 OUTPUT 并直接拉 HIGH
 *   4. 两个地址都失败则跳过, LCD 靠默认上拉仍可工作
 *
 * 参考: ../03.development.audio_record_sdcard
 */

#include "tca9554.hpp"
#include "esp_log.h"

static const char *TAG = "TCA9554";

Tca9554& Tca9554::inst() { static Tca9554 t; return t; }

esp_err_t Tca9554::init() {
    if (expander_handle != nullptr) return ESP_OK;

    /* 确保 I2C 总线已初始化 */
    MyIic::init();

    /* 参考项目: 先试 TCA9554 (0x20), 再试 TCA9554A (0x38) */
    ESP_LOGI(TAG, "Trying TCA9554 at 0x%02X...", TCA9554_I2C_ADDR);
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(s_bus_handle, TCA9554_I2C_ADDR, &expander_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TCA9554 not found, trying TCA9554A at 0x%02X...", TCA9554A_I2C_ADDR);
        ret = esp_io_expander_new_i2c_tca9554(s_bus_handle, TCA9554A_I2C_ADDR, &expander_handle);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No IO expander found, continuing without it");
        return ESP_OK;
    }

    /* 参考项目: P1 + P2 都设为 OUTPUT 并直接 HIGH (电源/背光使能) */
    ret = esp_io_expander_set_dir(expander_handle,
        TCA9554_PIN_BLK | TCA9554_PIN_RST,
        IO_EXPANDER_OUTPUT);
    ESP_ERROR_CHECK(ret);

    ret = esp_io_expander_set_level(expander_handle,
        TCA9554_PIN_BLK | TCA9554_PIN_RST, 1);
    ESP_ERROR_CHECK(ret);

    tca9554_handle = expander_handle;
    ESP_LOGI(TAG, "IO expander initialized, P1+P2=HIGH");
    return ESP_OK;
}

esp_err_t Tca9554::pinSetLevel(uint32_t pin_mask, uint8_t level) {
    return esp_io_expander_set_level(expander_handle, pin_mask, level);
}

esp_err_t Tca9554::pinGetLevel(uint32_t pin_mask, uint32_t *level) {
    return esp_io_expander_get_level(expander_handle, pin_mask, level);
}

esp_err_t Tca9554::pinSetDir(uint32_t pin_mask, esp_io_expander_dir_t dir) {
    return esp_io_expander_set_dir(expander_handle, pin_mask, dir);
}

esp_io_expander_handle_t Tca9554::handle() const { return expander_handle; }

esp_io_expander_handle_t tca9554_handle = nullptr;

extern "C" {

esp_err_t tca9554_init(void) {
    return Tca9554::inst().init();
}

esp_err_t tca9554_pin_set_level(uint32_t pin_mask, uint8_t level) {
    return Tca9554::inst().pinSetLevel(pin_mask, level);
}

esp_err_t tca9554_pin_get_level(uint32_t pin_mask, uint32_t *level) {
    return Tca9554::inst().pinGetLevel(pin_mask, level);
}

}
