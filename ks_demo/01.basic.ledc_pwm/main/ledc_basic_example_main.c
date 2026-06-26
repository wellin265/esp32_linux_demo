/*
 * Public Domain (or CC0 licensed, at your option.)
 */

#include <stdio.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"

#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO    19
#define LEDC_CHANNEL      LEDC_CHANNEL_0
#define LEDC_DUTY_RES     LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY    4000
#define LEDC_MAX_DUTY     ((1 << 13) - 1)

static void update_display(uint32_t duty_percent)
{
    char line1[64];
    char line2[64];
    char line3[64];

    snprintf(line1, sizeof(line1), "PWM GPIO%d %dHz", LEDC_OUTPUT_IO, LEDC_FREQUENCY);
    snprintf(line2, sizeof(line2), "resolution=13-bit");
    snprintf(line3, sizeof(line3), "duty=%u%%", (unsigned) duty_percent);
    ksdiy_example_display_set_lines(line1, line2, line3);
}

static void example_ledc_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.ledc_pwm", "PWM duty sweep");
    update_display(0);

    example_ledc_init();

    while (1) {
        for (uint32_t duty_percent = 0; duty_percent <= 100; duty_percent += 25) {
            uint32_t duty = (LEDC_MAX_DUTY * duty_percent) / 100;
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
            update_display(duty_percent);
            vTaskDelay(pdMS_TO_TICKS(800));
        }
    }
}
