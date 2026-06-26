/**
 * @file sta_manager.c
 * @brief WiFi STA（站点）设备管理模块实现
 *
 * 使用单向链表管理连接到 AP 的 WiFi 客户端，
 * 通过互斥信号量保证线程安全。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sta_manager.h"
#include "nvs_storage.h"

/** @brief 日志标签 */
static const char *TAG = "sta_mgr";

/** @brief 互斥信号量等待超时（ ticks ） */
#define STA_MUTEX_TIMEOUT_TICKS 200

/** @brief STA 链表互斥信号量 */
static SemaphoreHandle_t s_sta_mutex = NULL;

/** @brief STA 链表头 */
static modem_http_list_head_t s_sta_list = SLIST_HEAD_INITIALIZER(s_sta_list);

/**
 * @brief 将 MAC 地址格式化为十六进制字符串（无分隔符）
 *
 * 例如: {0x00,0x01,0x02,0x03,0x04,0x05} -> "000102030405"
 */
static void mac_to_hex_str(const uint8_t mac[6], char *out, size_t out_size)
{
    snprintf(out, out_size, "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief 通过 MAC 地址查询 DHCP 分配的 IP
 */
static void resolve_sta_ip(uint8_t mac[6], esp_ip4_addr_t *ip)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_netif_pair_mac_ip_t pair = {0};
    memcpy(pair.mac, mac, 6);
    esp_netif_dhcps_get_clients_by_mac(
        esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), 1, &pair);
    *ip = pair.ip;
#else
    dhcp_search_ip_on_mac(mac, (ip4_addr_t *)ip);
#endif
}

/* ===== 公共 API 实现 ===== */

esp_err_t sta_manager_init(void)
{
    SLIST_INIT(&s_sta_list);
    s_sta_mutex = xSemaphoreCreateMutex();
    if (!s_sta_mutex) {
        ESP_LOGE(TAG, "创建互斥信号量失败");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t sta_manager_add_node(uint8_t mac[6])
{
    struct modem_netif_sta_info *node = calloc(1, sizeof(struct modem_netif_sta_info));
    if (!node) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(s_sta_mutex, STA_MUTEX_TIMEOUT_TICKS) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥信号量超时");
        free(node);
        return ESP_FAIL;
    }

    /* 填充节点信息 */
    node->start_time = esp_timer_get_time();
    memcpy(node->mac, mac, 6);

    /* 从 NVS 读取设备名称，未找到则用 MAC 作为默认名称 */
    char mac_hex[13] = {0};
    mac_to_hex_str(mac, mac_hex, sizeof(mac_hex));
    size_t name_size = sizeof(node->name);
    if (nvs_storage_get_str(mac_hex, node->name, &name_size) != ESP_OK) {
        memcpy(node->name, mac_hex, 12);
    }

    /* 查询 DHCP 分配的 IP */
    resolve_sta_ip(mac, &node->ip);

    SLIST_INSERT_HEAD(&s_sta_list, node, field);

    if (xSemaphoreGive(s_sta_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "释放互斥信号量失败");
    }
    return ESP_OK;
}

esp_err_t sta_manager_remove_node(uint8_t mac[6])
{
    if (xSemaphoreTake(s_sta_mutex, STA_MUTEX_TIMEOUT_TICKS) != pdTRUE) {
        ESP_LOGE(TAG, "获取互斥信号量超时");
        return ESP_ERR_TIMEOUT;
    }

    struct modem_netif_sta_info *node;
    SLIST_FOREACH(node, &s_sta_list, field) {
        if (memcmp(node->mac, mac, 6) == 0) {
            ESP_LOGI(TAG, "移除设备: MAC=" MACSTR ", IP=" IPSTR,
                     MAC2STR(node->mac), IP2STR(&node->ip));
            SLIST_REMOVE(&s_sta_list, node, modem_netif_sta_info, field);
            free(node);
            break;
        }
    }

    if (xSemaphoreGive(s_sta_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "释放互斥信号量失败");
    }
    return ESP_OK;
}

esp_err_t sta_manager_update_all(void)
{
    if (xSemaphoreTake(s_sta_mutex, STA_MUTEX_TIMEOUT_TICKS) != pdTRUE) {
        return ESP_FAIL;
    }

    struct modem_netif_sta_info *node;
    SLIST_FOREACH(node, &s_sta_list, field) {
        /* 如果 IP 为 0，尝试重新查询 */
        if (node->ip.addr == 0) {
            resolve_sta_ip(node->mac, &node->ip);
        }
        /* 从 NVS 读取设备名称 */
        char mac_hex[13] = {0};
        mac_to_hex_str(node->mac, mac_hex, sizeof(mac_hex));
        size_t name_size = sizeof(node->name);
        nvs_storage_get_str(mac_hex, node->name, &name_size);
    }

    if (xSemaphoreGive(s_sta_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "释放互斥信号量失败");
    }

    sta_manager_print_all();
    return ESP_OK;
}

modem_http_list_head_t *sta_manager_get_list(void)
{
    return &s_sta_list;
}

esp_err_t sta_manager_print_all(void)
{
    struct modem_netif_sta_info *node;
    SLIST_FOREACH(node, &s_sta_list, field) {
        ESP_LOGI(TAG, "设备: MAC=" MACSTR ", IP=" IPSTR ", 连接时间=%lld us",
                 MAC2STR(node->mac), IP2STR(&node->ip), node->start_time);
    }
    return ESP_OK;
}
