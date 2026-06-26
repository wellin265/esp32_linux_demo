/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"

#define GPIO_OUTPUT_IO_0      19
#define GPIO_OUTPUT_IO_1      20
#define GPIO_OUTPUT_PIN_SEL   ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))

#define GPIO_INPUT_IO_0       5
#define GPIO_INPUT_PIN_SEL    (1ULL << GPIO_INPUT_IO_0)

#define ESP_INTR_FLAG_DEFAULT 0

typedef struct {
    uint32_t gpio_num;
    int level;
} gpio_event_t;

static QueueHandle_t gpio_evt_queue;
static volatile uint32_t s_interrupt_count;
static volatile uint32_t s_last_interrupt_gpio = UINT32_MAX;
static volatile int s_last_interrupt_level = -1;
static volatile int s_output_level;

static void update_display(void)
{
    char line1[64];
    char line2[64];
    char line3[64];

    const int out0 = s_output_level;
    const int out1 = s_output_level;
    const int in0 = gpio_get_level(GPIO_INPUT_IO_0);
    const uint32_t count = s_interrupt_count;
    const uint32_t last_gpio = s_last_interrupt_gpio;
    const int last_level = s_last_interrupt_level;

    snprintf(line1, sizeof(line1), "OUT%d=%d OUT%d=%d",
             GPIO_OUTPUT_IO_0, out0, GPIO_OUTPUT_IO_1, out1);
    snprintf(line2, sizeof(line2), "IN%d=%d", GPIO_INPUT_IO_0, in0);

    if (last_gpio == UINT32_MAX) {
        snprintf(line3, sizeof(line3), "irq count=%" PRIu32 " waiting...", count);
    } else {
        snprintf(line3, sizeof(line3), "irq=%" PRIu32 " GPIO%" PRIu32 "=%d",
                 count, last_gpio, last_level);
    }

    ksdiy_example_display_set_lines(line1, line2, line3);
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    gpio_event_t event = {
        .gpio_num = (uint32_t) arg,
        .level = gpio_get_level((gpio_num_t) arg),
    };
    xQueueSendFromISR(gpio_evt_queue, &event, NULL);
}

static void gpio_task_example(void *arg)
{
    gpio_event_t event;

    while (1) {
        if (xQueueReceive(gpio_evt_queue, &event, portMAX_DELAY)) {
            s_interrupt_count++;
            s_last_interrupt_gpio = event.gpio_num;
            s_last_interrupt_level = event.level;

            printf("GPIO[%" PRIu32 "] intr, val: %d, count: %" PRIu32 "\n",
                   event.gpio_num, event.level, s_interrupt_count);
            update_display();
        }
    }
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.gpio", "GPIO state monitor");
    ksdiy_example_display_set_lines("Configuring GPIO...", "", "");

    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_event_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 3072, NULL, 10, NULL);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *) GPIO_INPUT_IO_0);
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *) GPIO_INPUT_IO_0);

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    update_display();

    int cnt = 0;
    while (1) {
        s_output_level = cnt & 1;
        gpio_set_level(GPIO_OUTPUT_IO_0, s_output_level);
        gpio_set_level(GPIO_OUTPUT_IO_1, s_output_level);

        printf("cnt: %d, out level: %d\n", cnt, s_output_level);
        update_display();

        cnt++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
