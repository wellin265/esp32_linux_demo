/**
 * FreeRTOS 测试任务
 *
 * messageTask:   系统信息打印 (芯片型号/FLASH/PSRAM/任务列表)
 * lcdDemoTask:   LCD 图形演示 (色条/几何图形/触摸轨迹)
 * touchTask:     CST816S 触摸轮询 (每 50ms 读取坐标)
 */

#include "freertos_demo.hpp"
#include <stdio.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.hpp"
#include "cst816s.hpp"

/* 触摸数据 (任务间共享) */
static volatile int16_t  g_touch_x = -1;
static volatile int16_t  g_touch_y = -1;
static volatile bool     g_touch_detected = false;

/* message_task 配置 */
#define MESSAGE_PRIO       1
#define MESSAGE_STK_SIZE   3 * 1024

/* lcd_demo_task 配置 */
#define LCD_DEMO_PRIO      2
#define LCD_DEMO_STK_SIZE  4 * 1024

/* touch_task 配置 */
#define TOUCH_PRIO         3
#define TOUCH_STK_SIZE     2 * 1024

static TaskHandle_t MessageTask_Handler  = nullptr;
static TaskHandle_t LcdDemoTask_Handler  = nullptr;
static TaskHandle_t TouchTask_Handler    = nullptr;

/* 触摸轮询任务: 每 50ms 读取 CST816S */
static void touchTask(void *pvParameters) {
    const char *TAG = "touch";
    ESP_LOGI(TAG, "Touch task started");

    uint16_t x = 0, y = 0;
    uint8_t num = 0;

    while (1) {
        Cst816s::inst().readData();
        if (Cst816s::inst().getPoint(&x, &y, &num) == ESP_OK && num > 0) {
            g_touch_x = x;
            g_touch_y = y;
            g_touch_detected = true;
        } else {
            g_touch_detected = false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* LCD 演示任务: 显示图形 + 触摸坐标与轨迹 */
static void lcdDemoTask(void *pvParameters) {
    const char *TAG = "lcd_demo";
    ESP_LOGI(TAG, "LCD demo task started");

    /* ---- 清屏 ---- */
    lcd_clear(WHITE);

    /* ---- 顶部色条 (6 色) ---- */
    uint16_t colors[] = {RED, GREEN, BLUE, CYAN, MAGENTA, YELLOW};
    uint16_t bar_w = lcd_dev.width / 6;
    for (int i = 0; i < 6; i++) {
        lcd_fill(i * bar_w, 0, (i + 1) * bar_w - 1, 24, colors[i]);
    }

    /* ---- 标题 ---- */
    lcd_show_string(5, 30, lcd_dev.width - 10, 20,
                    12, (char *)"ST7789 SPI LCD", BLUE);
    lcd_show_string(5, 46, lcd_dev.width - 10, 20,
                    12, (char *)"ESP32-S3 + CST816S", GRAY);

    /* ---- 几何图形 ---- */
    lcd_draw_rectangle(4, 65, 70, 115, RED);
    lcd_draw_circle(130, 90, 25, GREEN);
    lcd_draw_line(175, 65, 270, 115, BLUE);
    lcd_show_string(12, 75, 55, 20, 12, (char *)"RECT", BLACK);

    /* ---- 触摸区域分隔线 ---- */
    lcd_draw_hline(0, 128, lcd_dev.width, GRAY);

    /* ---- 触摸状态提示 ---- */
    lcd_show_string(5, 135, lcd_dev.width - 10, 20,
                    12, (char *)"Touch: waiting...", DARKBLUE);

    char buf[48];
    uint16_t last_x = 0xFFFF, last_y = 0xFFFF;
    uint8_t refresh_cnt = 0;

    while (1) {
        if (g_touch_detected) {
            /* 更新坐标文字 */
            snprintf(buf, sizeof(buf), "X: %-3d  Y: %-3d    ", g_touch_x, g_touch_y);
            lcd_fill(5, 152, lcd_dev.width - 5, 170, WHITE);
            lcd_show_string(5, 152, lcd_dev.width - 10, 20, 16, buf, BLACK);

            /* 触摸轨迹绘制 */
            if (g_touch_x < lcd_dev.width && g_touch_y < lcd_dev.height) {
                lcd_draw_point(g_touch_x, g_touch_y, RED);
                if (last_x < lcd_dev.width && last_y < lcd_dev.height) {
                    lcd_draw_line(last_x, last_y, g_touch_x, g_touch_y, BLUE);
                }
                last_x = g_touch_x;
                last_y = g_touch_y;
            }
        } else {
            /* 手指抬起，重置轨迹 */
            last_x = 0xFFFF;
            last_y = 0xFFFF;

            /* 每 1s 刷新等待提示 */
            if (++refresh_cnt >= 50) {
                refresh_cnt = 0;
                lcd_fill(5, 135, lcd_dev.width - 5, 170, WHITE);
                lcd_show_string(5, 135, lcd_dev.width - 10, 20,
                                12, (char *)"Touch: waiting...", DARKBLUE);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

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

extern "C" {

void freertos_demo(void) {

    /* --- 创建任务 --- */
    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(messageTask), "message_task",
                            MESSAGE_STK_SIZE, nullptr, MESSAGE_PRIO, &MessageTask_Handler, 0);

    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(touchTask), "touch_task",
                            TOUCH_STK_SIZE, nullptr, TOUCH_PRIO, &TouchTask_Handler, 0);

    xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(lcdDemoTask), "lcd_demo_task",
                            LCD_DEMO_STK_SIZE, nullptr, LCD_DEMO_PRIO, &LcdDemoTask_Handler, 0);
}

}
