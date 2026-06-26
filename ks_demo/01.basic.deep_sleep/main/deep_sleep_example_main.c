/*
 * 深度睡眠示例 (Deep Sleep Example)
 *
 * 本示例演示如何使用 ESP32 的深度睡眠功能来节省功耗
 *
 * 深度睡眠简介：
 * - CPU 和大部分 RAM 断电
 * - 只有 RTC（实时时钟）保持工作
 * - 功耗可降至 ~10μA（取决于芯片和配置）
 * - 适合电池供电的 IoT 设备
 *
 * 唤醒源（Wake-up Sources）：
 * 1. 定时器唤醒：RTC 定时器到期后唤醒
 * 2. GPIO 唤醒：指定 GPIO 电平变化时唤醒
 * 3. EXT0 唤醒：单个 RTC GPIO 触发唤醒
 * 4. EXT1 唤醒：多个 RTC GPIO 组合触发唤醒
 * 5. 触摸唤醒：触摸传感器触发（部分芯片支持）
 * 6. ULP 唤醒：协处理器触发唤醒
 *
 * 本示例使用：
 * - 定时器唤醒（默认启用）
 * - GPIO 唤醒（可选，通过 menuconfig 配置）
 * - EXT0/EXT1 唤醒（可选，通过 menuconfig 配置）
 *
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"          // 睡眠模式 API
#include "esp_log.h"
#include "driver/rtc_io.h"     // RTC GPIO 操作
#include "nvs_flash.h"
#include "nvs.h"
#include "deep_sleep_example.h"
#include "ksdiy_example_display.h"

/*
 * RTC_SLOW_ATTR 宏：将变量存放在 RTC 慢速内存中
 *
 * RTC 内存的特点：
 * - 在深度睡眠期间保持数据
 * - 唤醒后数据仍然有效
 * - 容量有限（几 KB）
 *
 * sleep_enter_time 用于记录进入睡眠的时间
 * 唤醒后可以计算睡眠时长
 */
RTC_SLOW_ATTR static struct timeval sleep_enter_time;

/* ==================== 深度睡眠任务 ==================== */

/**
 * @brief 深度睡眠主任务
 *
 * 这个任务负责：
 * 1. 检测唤醒原因
 * 2. 打印睡眠时长
 * 3. 配置唤醒源
 * 4. 进入深度睡眠
 *
 * @param args 任务参数（未使用）
 */
static void deep_sleep_task(void *args)
{
    /*
     * 对于不支持 RTC 内存的芯片（如 ESP32-C2）
     * 使用 NVS 存储睡眠进入时间
     * NVS 的缺点是读写较慢，时间不够精确
     */
#if !SOC_RTC_FAST_MEM_SUPPORTED
    /* 初始化 NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* 打开 NVS 句柄 */
    nvs_handle_t nvs_handle;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Open NVS done\n");
    }

    /* 从 NVS 读取睡眠进入时间 */
    nvs_get_i32(nvs_handle, "slp_enter_sec", (int32_t *)&sleep_enter_time.tv_sec);
    nvs_get_i32(nvs_handle, "slp_enter_usec", (int32_t *)&sleep_enter_time.tv_usec);
#endif

    /* 计算睡眠时长 */
    struct timeval now;
    gettimeofday(&now, NULL);  // 获取当前时间

    /*
     * 计算毫秒级睡眠时长
     * tv_sec: 秒
     * tv_usec: 微秒
     */
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 +
                        (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    /* -------------------- 检测唤醒原因 -------------------- */

    /*
     * esp_sleep_get_wakeup_cause(): 获取唤醒原因
     *
     * 返回值：
     * - ESP_SLEEP_WAKEUP_UNDEFINED: 不是从深度睡眠唤醒（正常启动）
     * - ESP_SLEEP_WAKEUP_TIMER: 定时器唤醒
     * - ESP_SLEEP_WAKEUP_GPIO: GPIO 唤醒
     * - ESP_SLEEP_WAKEUP_EXT0: EXT0 唤醒
     * - ESP_SLEEP_WAKEUP_EXT1: EXT1 唤醒
     * - ESP_SLEEP_WAKEUP_TOUCHPAD: 触摸唤醒
     * - ESP_SLEEP_WAKEUP_ULP: ULP 协处理器唤醒
     */
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);
            break;
        }

#if CONFIG_EXAMPLE_GPIO_WAKEUP
        case ESP_SLEEP_WAKEUP_GPIO: {
            /* 获取触发唤醒的 GPIO 引脚掩码 */
            uint64_t wakeup_pin_mask = esp_sleep_get_gpio_wakeup_status();
            if (wakeup_pin_mask != 0) {
                /* __builtin_ffsll: 找到最低位的 1 的位置 */
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }
            break;
        }
#endif

#if CONFIG_EXAMPLE_EXT0_WAKEUP
        case ESP_SLEEP_WAKEUP_EXT0: {
            printf("Wake up from ext0\n");
            break;
        }
#endif

#ifdef CONFIG_EXAMPLE_EXT1_WAKEUP
        case ESP_SLEEP_WAKEUP_EXT1: {
            /* 获取触发 EXT1 唤醒的 GPIO 引脚 */
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }
            break;
        }
#endif

        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");  // 正常启动，非深度睡眠唤醒
    }

    /* 等待 1 秒，让串口有时间输出 */
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* -------------------- 特殊处理：隔离 GPIO -------------------- */

#if CONFIG_IDF_TARGET_ESP32
    /*
     * 对于 ESP32-WROVER 模组
     * GPIO12 上有外部上拉电阻
     * 隔离这个引脚可以降低功耗
     */
    rtc_gpio_isolate(GPIO_NUM_12);
#endif

    printf("Entering deep sleep\n");

    /* 记录进入睡眠的时间 */
    gettimeofday(&sleep_enter_time, NULL);

#if !SOC_RTC_FAST_MEM_SUPPORTED
    /* 对于不支持 RTC 内存的芯片，将时间保存到 NVS */
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "slp_enter_sec", sleep_enter_time.tv_sec));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "slp_enter_usec", sleep_enter_time.tv_usec));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
#endif

    /* -------------------- 进入深度睡眠 -------------------- */

    /*
     * esp_deep_sleep_start(): 进入深度睡眠
     *
     * 此函数不会返回！
     * 唤醒后芯片会重新启动，从头执行程序
     *
     * 在调用此函数之前，必须配置至少一个唤醒源
     * 否则芯片将永远无法唤醒！
     */
    esp_deep_sleep_start();
}

/* ==================== 配置唤醒源函数 ==================== */

/**
 * @brief 配置定时器唤醒
 *
 * 定时器是最简单的唤醒源
 * 设置一个时间后，RTC 会自动唤醒芯片
 */
static void example_deep_sleep_register_rtc_timer_wakeup(void)
{
    const int wakeup_time_sec = 20;  // 20 秒后唤醒

    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);

    /*
     * esp_sleep_enable_timer_wakeup(): 启用定时器唤醒
     *
     * 参数：唤醒时间（微秒）
     * 20秒 = 20 * 1000 * 1000 微秒
     */
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));
}

/* ==================== 程序入口 ==================== */

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.deep_sleep", "LVGL display ready");
    ksdiy_example_display_set_lines("Example started", "Watch serial for details", "Touch is enabled");
    /* 配置定时器唤醒（默认启用） */
    example_deep_sleep_register_rtc_timer_wakeup();

#if CONFIG_EXAMPLE_GPIO_WAKEUP
    /* 配置 GPIO 唤醒（如果启用） */
    example_deep_sleep_register_gpio_wakeup();
#endif

#if CONFIG_EXAMPLE_EXT0_WAKEUP
    /* 配置 EXT0 唤醒（如果启用） */
    example_deep_sleep_register_ext0_wakeup();
#endif

#if CONFIG_EXAMPLE_EXT1_WAKEUP
    /* 配置 EXT1 唤醒（如果启用） */
    example_deep_sleep_register_ext1_wakeup();
#endif

    /* 创建深度睡眠任务 */
    xTaskCreate(
        deep_sleep_task,    // 任务函数
        "deep_sleep_task",  // 任务名称
        4096,               // 栈大小（字节）
        NULL,               // 任务参数
        6,                  // 优先级
        NULL                // 任务句柄
    );

    /*
     * ===== 深度睡眠功耗优化提示 =====
     *
     * 1. 隔离未使用的 GPIO
     *    - rtc_gpio_isolate() 可以隔离引脚
     *    - 减少漏电流
     *
     * 2. 关闭未使用的外设
     *    - WiFi、蓝牙等
     *
     * 3. 使用正确的唤醒源
     *    - 定时器最省电
     *    - GPIO 唤醒需要保持 GPIO 供电
     *
     * 4. 减少 RTC 内存使用
     *    - 只保存必要的数据
     *
     * 5. 优化唤醒后的启动时间
     *    - CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP
     */
}
