/**
 * FreeRTOS 测试任务
 *
 * messageTask:   系统信息打印 (芯片型号/FLASH/PSRAM/任务列表)
 * ledTask:       LED 闪烁 (TCA9554 P6/P7, 1s 周期)
 */

#include "freertos_demo.hpp"
#include <stdio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_io_expander.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.hpp"
#include "tca9554.hpp"
#include "cst816s.hpp"

/* message_task 配置 */
#define MESSAGE_PRIO       1
#define MESSAGE_STK_SIZE   3 * 1024

/* led_task 配置 */
#define LED_PRIO           1
#define LED_STK_SIZE       3 * 1024

/* 文件作用域: 设备对象 (多任务共享, freertos_demo 返回后仍存活) */
static MyIic     g_i2c(GPIO_NUM_17, GPIO_NUM_18);
static MySpi     g_spi(GPIO_NUM_0, GPIO_NUM_1);
static Tca9554  *g_tca9554 = nullptr;   /* 延迟构造 (需要 g_i2c 先 init) */
static Lcd      *g_lcd = nullptr;
static Cst816s  *g_touch = nullptr;

static TaskHandle_t MessageTask_Handler = nullptr;
static TaskHandle_t LedTask_Handler     = nullptr;

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
            psram_used = static_cast<float>(esp_get_free_heap_size()) / static_cast<float>(esp_psram_get_size());
            printf("free_heap_size = %lu\r\n", esp_get_free_heap_size());
            printf("PSRAM usage rate:%0.2f%%\r\n", psram_used * 100);

            buff = static_cast<char *>(heap_caps_malloc(512 * sizeof(char), MALLOC_CAP_SPIRAM));
            if (buff == nullptr) {
                ESP_LOGE("PRINTF_TASK", "memory not enough");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            vTaskList(buff);
            printf("task name      state  prio  stack_left  task_num  core\r\n");
            printf("%s\r\n\r\n", buff);

            memset(buff, 0, 512);
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
    ESP_LOGI(TAG, "LED task started, P6+P7 blinking 1s");

    g_tca9554->pinSetDir(TCA9554_PIN_6, IO_EXPANDER_OUTPUT);
    g_tca9554->pinSetDir(TCA9554_PIN_7, IO_EXPANDER_OUTPUT);
    while (1) {
        g_tca9554->pinSetLevel(TCA9554_PIN_6, 0);
        g_tca9554->pinSetLevel(TCA9554_PIN_7, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        g_tca9554->pinSetLevel(TCA9554_PIN_6, 1);
        g_tca9554->pinSetLevel(TCA9554_PIN_7, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" {

void freertos_demo(void) {
    /* 1. 初始化 I2C / SPI 总线 */
    g_i2c.init();
    g_spi.init();

    /* 2. 构造设备对象 (需要总线先 init) */
    static Tca9554 tca(g_i2c);
    static Lcd      lcd(g_spi);
    static Cst816s  touch(g_i2c);
    g_tca9554 = &tca;
    g_lcd     = &lcd;
    g_touch   = &touch;

    /* 3. 初始化 TCA9554 */
    g_tca9554->init();

    /* 4. 硬件复位 LCD + Touch */
    g_tca9554->pinSetLevel(TCA9554_PIN_EN, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    g_tca9554->pinSetLevel(TCA9554_PIN_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* 5. 初始化 LCD + 触摸 */
    lcd_cfg_t lcd_config = {};
    g_lcd->init(lcd_config);

    g_touch->setConfig(Cst816sConfig{
        .x_max = lcd_dev.pwidth,
        .y_max = lcd_dev.pheight,
    });
    g_touch->init();

    g_lcd->showString(10, 10, 280, 30, 32, "hello world", RED);

    /* --- 创建任务 --- */
    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(messageTask), "message_task",
                            MESSAGE_STK_SIZE, nullptr, MESSAGE_PRIO,
                            &MessageTask_Handler, 0);

    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(ledTask), "led_task",
                            LED_STK_SIZE, nullptr, LED_PRIO,
                            &LedTask_Handler, 0);
}

}
