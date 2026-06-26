/*
 * Public Domain (or CC0 licensed, at your option.)
 */

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "example";

#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state;

#ifdef CONFIG_BLINK_LED_STRIP
static led_strip_handle_t led_strip;

static void blink_led(void)
{
    if (s_led_state) {
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
    };

#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#endif
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}
#endif

static void update_display(uint32_t blink_count)
{
    char line1[64];
    char line2[64];
    char line3[64];

#ifdef CONFIG_BLINK_LED_STRIP
    snprintf(line1, sizeof(line1), "addressable LED GPIO%d", BLINK_GPIO);
#else
    snprintf(line1, sizeof(line1), "GPIO LED GPIO%d", BLINK_GPIO);
#endif
    snprintf(line2, sizeof(line2), "state=%s count=%u",
             s_led_state ? "ON" : "OFF", (unsigned) blink_count);
    snprintf(line3, sizeof(line3), "period=%d ms", CONFIG_BLINK_PERIOD);
    ksdiy_example_display_set_lines(line1, line2, line3);
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.blink", "LED blink monitor");
    ksdiy_example_display_set_lines("configuring LED...", "", "");

    configure_led();

    uint32_t blink_count = 0;
    while (1) {
        blink_led();
        update_display(blink_count);
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state ? "ON" : "OFF");

        s_led_state = !s_led_state;
        blink_count++;
        vTaskDelay(pdMS_TO_TICKS(CONFIG_BLINK_PERIOD));
    }
}
