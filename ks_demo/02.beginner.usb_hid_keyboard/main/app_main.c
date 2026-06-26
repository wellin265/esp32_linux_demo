#include <inttypes.h>
#include <stdio.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "sdkconfig.h"
#include "usbd_core.h"
#include "usbh_core.h"
#include "usbx_demo.h"

void app_main(void)
{
    printf("Hello world!\n");
    ksdiy_example_display_bootstrap("02.beginner.usb_hid_keyboard", "USB HID keyboard");
    ksdiy_example_display_set_lines("USB device init", "keyboard emulation", "waiting for host");

    hid_keyboard_init1(0, 0x60080000);
    ksdiy_example_display_set_lines("USB configured", "sending key demo", "host can enumerate");
    hid_keyboard_test(0);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
