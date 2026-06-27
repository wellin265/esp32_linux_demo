/*
 * @Author       : 陈科进
 * @Date         : 2023-05-18 13:27:40
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2023-08-01 18:31:39
 * @FilePath: \cherryusb_esp32\main\main.c
 * @Description  : ESP32 candlelight firmware object
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "soc/usb_periph.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include <stdio.h>
#include "lwip/pbuf.h"
#include "lwip/lwip_napt.h"
// #include "esp_wifi_netif.h"
#include "rndis_protocol.h"
#include "esp_netif.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
// #include "esp_private/wifi.h"
#include "usbx_net.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"

extern void check_usb_task(void *arg);
void app_main()
{
    extern void cdc_acm_init00();
    extern void cdc_rndis_init();

    printf("Hello cherry!\n");
    // cdc_acm_init00();
    static esp_netif_t *usb_netif;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI("TAG", "esp_netif init success.");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // ESP_ERROR_CHECK(usbx_netif_init(usb_netif));
    // ESP_LOGI("TAG", "usbx_netif init success.");
    xTaskCreatePinnedToCore(check_usb_task, "check_usb_task", 4096, NULL, 8, NULL, 1);

    cdc_rndis_init(0, 0x60080000);

    while (1)
    {
        // extern void cdc_acm_data_send_with_dtr_test();

        // cdc_acm_data_send_with_dtr_test();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}