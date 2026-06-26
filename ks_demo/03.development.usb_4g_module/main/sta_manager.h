/**
 * @file sta_manager.h
 * @brief WiFi STA（站点）设备管理模块
 *
 * 管理连接到 AP 的 WiFi 客户端设备列表，
 * 提供设备添加、删除、查询和重命名功能。
 * 使用信号量保护链表操作的线程安全。
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/queue.h>
#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief STA 设备信息结构体
 */
struct modem_netif_sta_info {
    SLIST_ENTRY(modem_netif_sta_info) field;  /**< 链表链接字段 */
    uint8_t mac[6];                           /**< MAC 地址 */
    char name[32];                            /**< 设备名称 */
    esp_ip4_addr_t ip;                        /**< IP 地址 */
    int64_t start_time;                       /**< 连接开始时间（微秒） */
};

/** @brief STA 链表头类型 */
typedef SLIST_HEAD(modem_http_list_head, modem_netif_sta_info) modem_http_list_head_t;

/**
 * @brief 初始化 STA 管理器
 *
 * 创建互斥信号量，初始化链表。必须在其他 STA 操作之前调用。
 *
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t sta_manager_init(void);

/**
 * @brief 添加一个 STA 设备到管理列表
 *
 * 根据 MAC 地址创建新节点，自动查询 IP 和设备名称。
 *
 * @param mac 设备 MAC 地址（6 字节）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t sta_manager_add_node(uint8_t mac[6]);

/**
 * @brief 从管理列表中移除一个 STA 设备
 *
 * @param mac 设备 MAC 地址（6 字节）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t sta_manager_remove_node(uint8_t mac[6]);

/**
 * @brief 更新所有 STA 设备的 IP 和名称信息
 *
 * 遍历链表，更新 IP 为 0 的设备的地址，
 * 并从 NVS 读取设备名称。
 *
 * @return ESP_OK 成功
 */
esp_err_t sta_manager_update_all(void);

/**
 * @brief 获取 STA 链表头指针
 *
 * @return STA 链表头指针
 */
modem_http_list_head_t *sta_manager_get_list(void);

/**
 * @brief 打印所有 STA 设备信息（调试用）
 *
 * @return ESP_OK 成功
 */
esp_err_t sta_manager_print_all(void);

#ifdef __cplusplus
}
#endif
