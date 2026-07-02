#ifndef __BLE_H
#define __BLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bleTask(void *pvParameters);

/* 通过蓝牙 SPP Notify 发送数据 — 可在任意任务中调用 */
bool ble_spp_send(const uint8_t *data, size_t len);

/* 查询蓝牙连接状态 */
bool ble_is_connected(void);

/* 获取最近收到的蓝牙数据 (非阻塞, 有新数据返回 true) */
bool ble_get_received(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif
