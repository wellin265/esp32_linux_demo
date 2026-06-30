#include "lcd_example.hpp"
#include "lcd.hpp"
#include "spi.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ========== 任务配置常量 ========== */

#define LCD_TASK_PRIO      1
#define LCD_TASK_STK_SIZE  4 * 1024

/* ========== 文件作用域: 设备对象 ========== */

static Spi       g_spi;
static Lcd      *g_lcd = nullptr;

/* ========== 任务函数 ========== */

static void lcdTask(void *pvParams)
{
    const char *TAG = "lcd_task";
    ESP_LOGI(TAG, "Task started");

    while (1) {
        g_lcd->fill(0, 0, 160, 120, RED);
        g_lcd->fill(160, 0, 320, 120, GREEN);
        g_lcd->fill(0, 120, 160, 240, BLUE);
        g_lcd->fill(160, 120, 320, 240, WHITE);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ========== 初始化入口 ========== */

extern "C" void lcd_example(void)
{
    /* 步骤 1: 初始化总线 */
    g_spi.init();

    /* 步骤 2: 构造设备 (static 确保生命周期超出函数返回) */
    static Lcd lcd(g_spi);

    /* 步骤 3: 指针绑定 */
    g_lcd = &lcd;

    /* 步骤 4: 初始化设备 */
    g_lcd->init();

    /* 步骤 5: 创建 FreeRTOS 任务 */
    xTaskCreatePinnedToCore(
        (TaskFunction_t)lcdTask,
        "lcd_task",
        LCD_TASK_STK_SIZE,
        NULL,
        LCD_TASK_PRIO,
        NULL,
        0
    );
}
