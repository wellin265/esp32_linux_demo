/**
 * @file app_main.c
 * @brief ESP32-S3 4G Cat.1 Wi-Fi 路由器主程序
 *
 * 本示例演示如何使用 ESP32-S3 配合 USB 4G 模组搭建一个 Wi-Fi 路由器：
 * - 通过 USB 连接 4G Cat.1 模组拨号上网
 * - 创建 Wi-Fi AP 热点，让其他设备通过 4G 网络上网
 * - 可选启用 Web 管理界面，通过浏览器配置 WiFi 参数
 * - LED 指示灯显示系统/WiFi/4G 状态
 * - 可选 Ping 网络连通性检测
 *
 * 硬件平台：KSDIY Korvo 开发板（ESP32-S3）
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "led_indicator.h"
#include "led_indicator_blink_default.h"
#include "usbh_modem_board.h"
#include "usbh_modem_wifi.h"

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
#include "modem_http_config.h"
#endif

#ifdef CONFIG_EXAMPLE_PING_NETWORK
#include "ping/ping_sock.h"
#endif

/* ===== 常量定义 ===== */

/** @brief 日志标签 */
static const char *TAG = "4g_main";

/** @brief LED GPIO 配置（通过 menuconfig 配置） */
#define LED_RED_SYSTEM_GPIO     CONFIG_EXAMPLE_LED_RED_SYSTEM_GPIO      /**< 系统状态指示灯（红色） */
#define LED_BLUE_WIFI_GPIO      CONFIG_EXAMPLE_LED_BLUE_WIFI_GPIO       /**< WiFi 状态指示灯（蓝色） */
#define LED_GREEN_4GMODEM_GPIO  CONFIG_EXAMPLE_LED_GREEN_4GMODEM_GPIO   /**< 4G 模组状态指示灯（绿色） */
#define LED_ACTIVE_LEVEL        1                                        /**< LED 高电平有效 */

/** @brief 主循环间隔（毫秒） */
#define MAIN_LOOP_INTERVAL_MS  10000

/* ===== 全局变量 ===== */

/** @brief WiFi 配置（使用默认值初始化） */
static modem_wifi_config_t s_modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();

/** @brief LED 指示灯句柄 */
static led_indicator_handle_t s_led_system = NULL;   /**< 系统状态 LED */
static led_indicator_handle_t s_led_wifi = NULL;     /**< WiFi 状态 LED */
static led_indicator_handle_t s_led_4g = NULL;       /**< 4G 模组状态 LED */

/* ===== 系统调试：任务 dump（可选） ===== */

#ifdef CONFIG_DUMP_SYSTEM_STATUS
#define TASK_MAX_COUNT 32

/** @brief 任务快照数据结构 */
typedef struct {
    uint32_t ulRunTimeCounter;   /**< 累计运行时间 */
    uint32_t xTaskNumber;        /**< 任务编号 */
} task_data_t;

static task_data_t s_prev_snapshot[TASK_MAX_COUNT];  /**< 上次快照 */
static int s_task_top_index = 0;                       /**< 快照索引 */
static uint32_t s_prev_total_run_time = 0;             /**< 上次总运行时间 */

/**
 * @brief 查找或分配任务快照条目
 */
static task_data_t *get_prev_task_data(uint32_t task_num)
{
    for (int i = 0; i < s_task_top_index; i++) {
        if (s_prev_snapshot[i].xTaskNumber == task_num) {
            return &s_prev_snapshot[i];
        }
    }
    /* 分配新条目 */
    assert(s_task_top_index < TASK_MAX_COUNT);
    task_data_t *result = &s_prev_snapshot[s_task_top_index];
    result->xTaskNumber = task_num;
    s_task_top_index++;
    return result;
}

/**
 * @brief 打印系统任务状态和堆内存信息
 *
 * 输出所有 FreeRTOS 任务的 CPU 占用率和栈剩余空间，
 * 以及堆内存使用情况。
 */
static void system_dump(void)
{
    uint32_t total_run_time;
    TaskStatus_t task_stats[TASK_MAX_COUNT];
    uint32_t task_count = uxTaskGetSystemState(task_stats, TASK_MAX_COUNT, &total_run_time);
    assert(s_task_top_index < TASK_MAX_COUNT);

    uint32_t total_delta = total_run_time - s_prev_total_run_time;
    float factor = 100.0f / total_delta;

    ESP_LOGI(TAG, "=== 系统任务状态 ===");
    ESP_LOGI(TAG, "CPU占用\t栈剩余\t名称\t优先级");

    for (uint32_t i = 0; i < task_count; i++) {
        task_data_t *prev = get_prev_task_data(task_stats[i].xTaskNumber);
        float load = factor * (task_stats[i].ulRunTimeCounter - prev->ulRunTimeCounter);
        ESP_LOGI(TAG, "%.2f%%\t%" PRIu32 "\t%s\t%" PRIu32,
                 load, task_stats[i].usStackHighWaterMark,
                 task_stats[i].pcTaskName, (uint32_t)task_stats[i].uxBasePriority);
        prev->ulRunTimeCounter = task_stats[i].ulRunTimeCounter;
    }

    ESP_LOGI(TAG, "堆内存: 可用=%d 最大块=%d 最小历史=%d, 内部: 可用=%d 最大块=%d",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));

    s_prev_total_run_time = total_run_time;
}
#endif /* CONFIG_DUMP_SYSTEM_STATUS */

/* ===== LED 指示灯初始化 ===== */

/**
 * @brief 初始化三色 LED 指示灯
 *
 * 创建红（系统）、蓝（WiFi）、绿（4G）三个 LED 指示灯。
 * WiFi 和 4G LED 默认显示"连接中"闪烁模式。
 */
static void led_indicator_init(void)
{
    led_indicator_gpio_config_t gpio_cfg = {
        .is_active_level_high = LED_ACTIVE_LEVEL,
    };

    led_indicator_config_t led_cfg = {
        .led_indicator_gpio_config = &gpio_cfg,
        .mode = LED_GPIO_MODE,
    };

    /* 红色系统状态灯 */
    if (LED_RED_SYSTEM_GPIO) {
        gpio_cfg.gpio_num = LED_RED_SYSTEM_GPIO;
        s_led_system = led_indicator_create(&led_cfg);
        assert(s_led_system != NULL);
    }

    /* 蓝色 WiFi 状态灯 */
    if (LED_BLUE_WIFI_GPIO) {
        gpio_cfg.gpio_num = LED_BLUE_WIFI_GPIO;
        s_led_wifi = led_indicator_create(&led_cfg);
        assert(s_led_wifi != NULL);
        led_indicator_stop(s_led_wifi, BLINK_CONNECTED);
        led_indicator_start(s_led_wifi, BLINK_CONNECTING);
    }

    /* 绿色 4G 模组状态灯 */
    if (LED_GREEN_4GMODEM_GPIO) {
        gpio_cfg.gpio_num = LED_GREEN_4GMODEM_GPIO;
        s_led_4g = led_indicator_create(&led_cfg);
        assert(s_led_4g != NULL);
        led_indicator_stop(s_led_4g, BLINK_CONNECTED);
        led_indicator_start(s_led_4g, BLINK_CONNECTING);
    }
}

/* ===== 4G 模组事件处理 ===== */

/**
 * @brief 4G 模组事件回调函数
 *
 * 处理 SIM 卡插拔、USB 连接/断开、网络连接/断开、
 * WiFi STA 连接/断开等事件，并更新 LED 指示状态。
 *
 * @param arg        用户参数（未使用）
 * @param event_base 事件基类
 * @param event_id   事件 ID
 * @param event_data 事件数据
 */
static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base != MODEM_BOARD_EVENT) {
        return;
    }

    switch (event_id) {
    /* SIM 卡事件 */
    case MODEM_EVENT_SIMCARD_DISCONN:
        ESP_LOGW(TAG, "SIM 卡已拔出");
        led_indicator_start(s_led_system, BLINK_CONNECTED);
        break;
    case MODEM_EVENT_SIMCARD_CONN:
        ESP_LOGI(TAG, "SIM 卡已插入");
        led_indicator_stop(s_led_system, BLINK_CONNECTED);
        break;

    /* USB 连接事件 */
    case MODEM_EVENT_DTE_DISCONN:
        ESP_LOGW(TAG, "USB 已断开");
        led_indicator_start(s_led_system, BLINK_CONNECTING);
        break;
    case MODEM_EVENT_DTE_CONN:
        ESP_LOGI(TAG, "USB 已连接");
        led_indicator_stop(s_led_system, BLINK_CONNECTED);
        led_indicator_stop(s_led_system, BLINK_CONNECTING);
        break;
    case MODEM_EVENT_DTE_RESTART:
        ESP_LOGW(TAG, "4G 模组硬件重启中");
        led_indicator_start(s_led_system, BLINK_CONNECTED);
        break;
    case MODEM_EVENT_DTE_RESTART_DONE:
        ESP_LOGI(TAG, "4G 模组重启完成");
        led_indicator_stop(s_led_system, BLINK_CONNECTED);
        break;

    /* 网络连接事件 */
    case MODEM_EVENT_NET_CONN:
        ESP_LOGI(TAG, "4G 网络已连接");
        led_indicator_start(s_led_4g, BLINK_CONNECTED);
        break;
    case MODEM_EVENT_NET_DISCONN:
        ESP_LOGW(TAG, "4G 网络已断开");
        led_indicator_stop(s_led_4g, BLINK_CONNECTED);
        break;

    /* WiFi STA 事件 */
    case MODEM_EVENT_WIFI_STA_CONN:
        ESP_LOGI(TAG, "WiFi 设备已连接");
        led_indicator_start(s_led_wifi, BLINK_CONNECTED);
        break;
    case MODEM_EVENT_WIFI_STA_DISCONN:
        ESP_LOGW(TAG, "所有 WiFi 设备已断开");
        led_indicator_stop(s_led_wifi, BLINK_CONNECTED);
        break;

    default:
        break;
    }
}

/* ===== Ping 网络检测（可选） ===== */

#ifdef CONFIG_EXAMPLE_PING_NETWORK

/**
 * @brief Ping 成功回调
 */
static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));

    ESP_LOGI(TAG, "%" PRIu32 " bytes from %s icmp_seq=%u ttl=%u time=%" PRIu32 " ms",
             recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

/**
 * @brief Ping 超时回调
 */
static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));

    ESP_LOGW(TAG, "From %s icmp_seq=%u 超时", ipaddr_ntoa(&target_addr), seqno);
}
#endif /* CONFIG_EXAMPLE_PING_NETWORK */

/* ===== 主函数 ===== */

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 LED 指示灯
 * 2. 初始化 NVS 非易失性存储
 * 3. 初始化 TCP/IP 协议栈和事件循环
 * 4. 初始化 4G 模组并拨号上网
 * 5. （可选）启动 Web 管理服务器
 * 6. 创建 WiFi AP 热点
 * 7. 进入主循环（信号质量监测、DNS 更新、Ping 等）
 */
void app_main(void)
{
    /* 步骤 1：初始化 LED 指示灯 */
    led_indicator_init();

    /* 步骤 2：初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 分区需要擦除，重新初始化");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* 步骤 3：初始化网络协议栈 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "     ESP 4G Cat.1 Wi-Fi 路由器");
    ESP_LOGI(TAG, "====================================");

    /* 步骤 4：初始化 4G 模组 */
    modem_config_t modem_config = MODEM_DEFAULT_CONFIG();
#ifndef CONFIG_EXAMPLE_ENTER_PPP_DURING_INIT
    modem_config.flags |= MODEM_FLAGS_INIT_NOT_ENTER_PPP;
    modem_config.flags |= MODEM_FLAGS_INIT_NOT_BLOCK;
#endif
    modem_config.handler = on_modem_event;
    modem_board_init(&modem_config);

    /* 步骤 5：启动 Web 管理服务器（可选） */
#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    modem_http_get_nvs_wifi_config(&s_modem_wifi_config);
    modem_http_init(&s_modem_wifi_config);
#endif

    /* 步骤 6：创建 WiFi AP 热点 */
    esp_netif_t *ap_netif = modem_wifi_ap_init();
    assert(ap_netif != NULL);
    ESP_ERROR_CHECK(modem_wifi_set(&s_modem_wifi_config));

    /* 步骤 7：配置 Ping 检测（可选） */
#ifdef CONFIG_EXAMPLE_PING_NETWORK
    ip_addr_t target_addr = {0};
    char *ping_addr_s = NULL;
#ifdef CONFIG_EXAMPLE_PING_MANUAL_ADDR
    ping_addr_s = CONFIG_EXAMPLE_PING_MANUAL_ADDR;
#else
    esp_netif_dns_info_t dns_info;
    modem_board_get_dns_info(ESP_NETIF_DNS_MAIN, &dns_info);
    ping_addr_s = ip4addr_ntoa((ip4_addr_t *)(&dns_info.ip.u_addr.ip4));
#endif
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ipaddr_aton(ping_addr_s, &target_addr);
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = CONFIG_EXAMPLE_PING_TIMEOUT;
    ping_config.task_stack_size = 4096;
    ping_config.count = 1;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,
        .cb_args = NULL,
    };
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
#endif

    /* ===== 主循环 ===== */
    uint32_t ap_dns_addr = 0;
    while (1) {
        /* 查询 4G 信号质量 */
#if !defined(CONFIG_EXAMPLE_ENTER_PPP_DURING_INIT) || defined(CONFIG_MODEM_SUPPORT_SECONDARY_AT_PORT)
        int rssi = 0, ber = 0;
        modem_board_get_signal_quality(&rssi, &ber);
        ESP_LOGI(TAG, "4G 信号: rssi=%d, ber=%d", rssi, ber);
#endif

        /* 自动更新 DNS 地址 */
#ifdef CONFIG_EXAMPLE_AUTO_UPDATE_DNS
        esp_netif_dns_info_t dns;
        modem_board_get_dns_info(ESP_NETIF_DNS_MAIN, &dns);
        uint32_t new_dns = dns.ip.u_addr.ip4.addr;
        if (new_dns != ap_dns_addr) {
            modem_wifi_set_dns(ap_netif, new_dns);
            ap_dns_addr = new_dns;
            ESP_LOGI(TAG, "DNS 地址已更新: %s", inet_ntoa(ap_dns_addr));
        }
#endif

        /* 执行 Ping 检测 */
#ifdef CONFIG_EXAMPLE_PING_NETWORK
        ESP_LOGI(TAG, "Ping %s ...", ping_addr_s);
        esp_ping_start(ping);
#endif

        /* 打印系统状态 */
#ifdef CONFIG_DUMP_SYSTEM_STATUS
        system_dump();
#endif

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_INTERVAL_MS));
    }
}
