#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "lcd.hpp"
#include "tca9554.hpp"
#include "cst816s.hpp"
#include "freertos_demo.hpp"

extern "C" void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* 1. 初始化 TCA9554 IO 扩展 (I2C GPIO 17/18)
     *    内部自动将 P1(供电)+P2(使能) 设为 HIGH, LCD 模组上电 */
    Tca9554::inst().init();

    /* 2. 硬件复位 LCD + Touch (TCA9554 P2 翻转)
     *    P2 LOW → 200ms → P2 HIGH, 将 LCD 控制器恢复到已知状态 */
    Tca9554::inst().pinSetLevel(TCA9554_PIN_EN, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    Tca9554::inst().pinSetLevel(TCA9554_PIN_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* 3. 初始化 LCD — ST7789 (SPI GPIO 0/1, CS=46, DC=2)
     *    硬复位后, esp_lcd_panel_reset() 发送软件复位命令 */
    lcd_cfg_t lcd_config = {};
    Lcd::inst().init(lcd_config);

    /* 4. 初始化 CST816S 触摸 (复用 I2C, 地址 0x15) */
    Cst816s::inst().init(lcd_dev.pwidth, lcd_dev.pheight);

    /* 5. 启动测试任务 (系统信息 + LCD 演示 + 触摸) */
    freertos_demo();
}
