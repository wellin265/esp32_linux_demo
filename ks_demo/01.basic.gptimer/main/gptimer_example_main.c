/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>

#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"

static const char *TAG = "example";

typedef struct {
    uint64_t event_count;
} example_queue_element_t;

static void update_display(const char *mode, uint32_t event_index, uint64_t count_value)
{
    char line1[64];
    char line2[64];
    char line3[64];

    snprintf(line1, sizeof(line1), "mode: %s", mode);
    snprintf(line2, sizeof(line2), "events=%u", (unsigned) event_index);
    snprintf(line3, sizeof(line3), "count=%llu", count_value);
    ksdiy_example_display_set_lines(line1, line2, line3);
}

static bool IRAM_ATTR example_timer_on_alarm_cb_v1(gptimer_handle_t timer,
                                                    const gptimer_alarm_event_data_t *edata,
                                                    void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t) user_data;
    gptimer_stop(timer);

    example_queue_element_t ele = {
        .event_count = edata->count_value,
    };
    xQueueSendFromISR(queue, &ele, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

static bool IRAM_ATTR example_timer_on_alarm_cb_v2(gptimer_handle_t timer,
                                                    const gptimer_alarm_event_data_t *edata,
                                                    void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t) user_data;
    example_queue_element_t ele = {
        .event_count = edata->count_value,
    };
    xQueueSendFromISR(queue, &ele, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

static bool IRAM_ATTR example_timer_on_alarm_cb_v3(gptimer_handle_t timer,
                                                    const gptimer_alarm_event_data_t *edata,
                                                    void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t) user_data;
    example_queue_element_t ele = {
        .event_count = edata->count_value,
    };
    xQueueSendFromISR(queue, &ele, &high_task_awoken);

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = edata->alarm_value + 1000000,
    };
    gptimer_set_alarm_action(timer, &alarm_config);
    return high_task_awoken == pdTRUE;
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.gptimer", "timer event monitor");
    update_display("create", 0, 0);

    QueueHandle_t queue = xQueueCreate(10, sizeof(example_queue_element_t));
    if (queue == NULL) {
        update_display("queue failed", 0, 0);
        return;
    }

    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = example_timer_on_alarm_cb_v1,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, queue));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    gptimer_alarm_config_t alarm_config1 = {
        .alarm_count = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config1));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    example_queue_element_t ele;
    uint32_t event_index = 0;

    if (xQueueReceive(queue, &ele, pdMS_TO_TICKS(2000))) {
        event_index++;
        ESP_LOGI(TAG, "Timer stopped, count=%llu", ele.event_count);
        update_display("one-shot", event_index, ele.event_count);
    }

    uint64_t count = 0;
    ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer, 100));
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &count));
    ESP_LOGI(TAG, "Timer count value=%llu", count);

    ESP_ERROR_CHECK(gptimer_disable(gptimer));
    cbs.on_alarm = example_timer_on_alarm_cb_v2;
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, queue));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    gptimer_alarm_config_t alarm_config2 = {
        .reload_count = 0,
        .alarm_count = 1000000,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config2));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    for (int i = 0; i < 4; i++) {
        if (xQueueReceive(queue, &ele, pdMS_TO_TICKS(2000))) {
            event_index++;
            ESP_LOGI(TAG, "Timer reloaded, count=%llu", ele.event_count);
            update_display("auto reload", event_index, ele.event_count);
        }
    }
    ESP_ERROR_CHECK(gptimer_stop(gptimer));

    ESP_ERROR_CHECK(gptimer_disable(gptimer));
    cbs.on_alarm = example_timer_on_alarm_cb_v3;
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, queue));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    gptimer_alarm_config_t alarm_config3 = {
        .alarm_count = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config3));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    for (int i = 0; i < 4; i++) {
        if (xQueueReceive(queue, &ele, pdMS_TO_TICKS(2000))) {
            event_index++;
            ESP_LOGI(TAG, "Timer alarmed, count=%llu", ele.event_count);
            update_display("dynamic alarm", event_index, ele.event_count);
        }
    }

    ESP_ERROR_CHECK(gptimer_stop(gptimer));
    ESP_ERROR_CHECK(gptimer_disable(gptimer));
    ESP_ERROR_CHECK(gptimer_del_timer(gptimer));
    vQueueDelete(queue);
}
