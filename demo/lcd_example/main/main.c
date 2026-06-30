#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "spi.h"
#include "lcd.h"
#include <stdio.h>

#define TAG "main"

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();     /* 初始化NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    spi_init();              /* SPI初始化 */ 
    lcd_init();              /* LCD屏初始化 */

    lcd_clear(BLACK);
    lcd_show_string(10, 10, 320, 40, 24, "hello world", RED);
 
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
