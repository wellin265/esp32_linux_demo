#include <inttypes.h>
#include <stdio.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "sdkconfig.h"
#include "usbd_core.h"
#include "usbh_core.h"
#include "usbx_cdc.h"

void app_main(void)
{
    printf("Hello world!\n");
    ksdiy_example_display_bootstrap("02.beginner.usb_cdc", "CherryUSB CDC ACM");
    ksdiy_example_display_set_lines("USB device init", "waiting for host", "CDC echo ready");

    cdc_acm_init1(0, 0x60080000);
    ksdiy_example_display_set_lines("USB configured", "connect PC by USB", "CDC echo ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
