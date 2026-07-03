#include "freertos_demo.hpp"
#include <cstring>
#include <stdio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "ws2812.hpp"
#include "ble.hpp"
#include "wifi.hpp"

/* message_task 配置 */
#define MESSAGE_PRIO       5
#define MESSAGE_STK_SIZE   5 * 1024

/* led_task 配置 */
#define LED_PRIO           1
#define LED_STK_SIZE       3 * 1024

/* ble_task 配置 */
#define BLE_PRIO           3
#define BLE_STK_SIZE       5 * 1024

/* wifi_task 配置 */
#define WIFI_PRIO          2
#define WIFI_STK_SIZE      5 * 1024

static TaskHandle_t MessageTask_Handler = nullptr;
static TaskHandle_t LedTask_Handler     = nullptr;
static TaskHandle_t BleTask_Handler     = nullptr;
static TaskHandle_t WifiTask_Handler    = nullptr;

Ws2812    g_ws2812;

/* 系统信息任务 */
static void messageTask(void *pvParameters) {
    const char *TAG = "message_task";
    char *buff = nullptr;
    uint8_t cnt = 0;
    uint32_t flash_size;
    esp_chip_info_t chip_info;
    float psram_used = 0.0;

    ESP_ERROR_CHECK(esp_flash_get_size(nullptr, &flash_size));
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "| %-12s | %-10s |", "describe", "explain");
    ESP_LOGI(TAG, "|--------------|------------|");

    if (chip_info.model == CHIP_ESP32S3) {
        ESP_LOGI(TAG, "| %-12s | %-10s |", "model", "ESP32S3");
    }

    ESP_LOGI(TAG, "| %-12s | %-d          |", "cores",      chip_info.cores);
    ESP_LOGI(TAG, "| %-12s | %-d          |", "revision",   chip_info.revision);
    ESP_LOGI(TAG, "| %-12s | %-ld         |", "FLASH size", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "| %-12s | %-2d         |", "PSRAM size", esp_psram_get_size() / (1024 * 1024));
    ESP_LOGI(TAG, "|--------------|------------|");

    while (1) {
        if (++cnt >= 10) {
            cnt = 0;
            if (esp_psram_get_size() > 0) {
                psram_used = static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / static_cast<float>(esp_psram_get_size());
                printf("PSRAM free:%.2f%%, usage:%.2f%%\r\n", psram_used * 100, (1.0f - psram_used) * 100);
            }
            printf("internal_free_heap = %lu\r\n", esp_get_free_heap_size());

            /* 每个任务约 40~60 字节, 多任务时 512 不够, 用 1024 */
            buff = static_cast<char *>(heap_caps_malloc(1024 * sizeof(char), MALLOC_CAP_SPIRAM));
            if (buff == nullptr) {
                /* PSRAM 分配失败则回退到内部 RAM */
                buff = static_cast<char *>(malloc(1024 * sizeof(char)));
            }
            if (buff == nullptr) {
                ESP_LOGE("PRINTF_TASK", "memory not enough");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            vTaskList(buff);
            printf("task name      state  prio  stack_left  task_num  core\r\n");
            printf("%s\r\n\r\n", buff);

            memset(buff, 0, 1024);
            vTaskGetRunTimeStats(buff);
            printf("task name         run_count    usage\r\n");
            printf("%s", buff);
            printf("---------------------------------------------\r\n\n");
            heap_caps_free(buff);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* LED 闪烁任务 */
static void ledTask(void *pvParameters) {
    const char *TAG = "led_task";
    g_ws2812.init();
   
    while (1) {
        g_ws2812.setPixel(0, 10, 0, 0);
        g_ws2812.refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));
        g_ws2812.setPixel(0, 0, 10, 0);
        g_ws2812.refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));
        g_ws2812.setPixel(0, 0, 0, 10);
        g_ws2812.refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (ble_is_connected())
        {
            const char *message = "Hello BLE!";
            ble_spp_send((uint8_t *)message, strlen(message));
        }
    }
}

extern "C" {

void freertos_demo(void) {
    /* --- 创建任务 --- */
    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(messageTask), "message_task",
                            MESSAGE_STK_SIZE, nullptr, MESSAGE_PRIO,
                            &MessageTask_Handler, 0);

    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(ledTask), "led_task",
                            LED_STK_SIZE, nullptr, LED_PRIO,
                            &LedTask_Handler, 0);

    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(bleTask), "ble_task",
                            BLE_STK_SIZE, nullptr, BLE_PRIO,
                            &BleTask_Handler, 0);
    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(wifiTask), "wifi_task",
                            WIFI_STK_SIZE, nullptr, WIFI_PRIO,
                            &WifiTask_Handler, 0);
}

}
