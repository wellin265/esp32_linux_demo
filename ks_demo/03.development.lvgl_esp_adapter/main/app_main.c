

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "ksdiy_lvgl_port.h"
static const char *TAG = "main";

void app_main(void)
{
    ksdiy_lvgl_port_init();
    ksdiy_lvgl_lock(10);
    lv_demo_music();
    ksdiy_lvgl_unlock();
}