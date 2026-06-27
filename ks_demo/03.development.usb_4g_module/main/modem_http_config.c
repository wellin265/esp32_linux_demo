/**
 * @file modem_http_config.c
 * @brief 4G 路由器 Web 管理服务器
 *
 * 提供 WiFi 配置管理、STA 设备管理等 RESTful API 接口。
 * 通过 HTTP Basic Auth 进行身份验证。
 *
 * API 接口：
 * - GET  /wlan_general           获取 WiFi 基本信息（SSID/密码/加密模式）
 * - POST /wlan_general           修改 WiFi 基本信息
 * - GET  /wlan_advance           获取 WiFi 高级设置（带宽/信道）
 * - POST /wlan_advance           修改 WiFi 高级设置
 * - GET  /system/station_state   获取已连接设备列表
 * - POST /system/station_state/delete_device   踢出指定设备
 * - POST /system/station_state/change_name     修改设备备注名
 * - GET  /star                   静态文件服务（SPIFFS，通配符路由）
 *
 * @copyright SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "esp_eth.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "json_parser.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* 内部模块头文件 */
#include "modem_http_config.h"
#include "http_auth.h"
#include "http_helper.h"
#include "nvs_storage.h"
#include "sta_manager.h"
#include "usbh_modem_wifi.h"

/** @brief 日志标签 */
static const char *TAG = "4g_router";

/** @brief SPIFFS 最大文件路径长度 */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)

/** @brief HTTP 临时缓冲区大小 */
#define SCRATCH_BUFSIZE (10240)

/* ===== 请求上下文结构体 ===== */

/** @brief REST 服务器上下文 */
typedef struct {
    char base_path[ESP_VFS_PATH_MAX + 1];  /**< SPIFFS 挂载路径 */
    char scratch[SCRATCH_BUFSIZE];          /**< 文件传输缓冲区 */
} rest_server_context_t;

/** @brief 合并上下文（REST + 认证信息） */
typedef struct {
    rest_server_context_t *rest_context;    /**< REST 上下文 */
    basic_auth_info_t *basic_auth_info;     /**< 认证信息 */
} ctx_info_t;

/* ===== 模块级全局变量 ===== */

static modem_wifi_config_t *s_modem_wifi_config = NULL;  /**< WiFi 配置指针 */
static httpd_handle_t s_server = NULL;                    /**< HTTP 服务器句柄 */
static bool s_initialized = false;                        /**< 初始化标志 */

/* ===== WiFi 认证模式工具函数 ===== */

/**
 * @brief 将 WiFi 认证模式枚举转换为字符串
 */
static const char *authmode_to_str(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN:        return "OPEN";
    case WIFI_AUTH_WEP:         return "WEP";
    case WIFI_AUTH_WPA2_PSK:    return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:return "WPA_WPA2_PSK";
    default:                    return "WPA_WPA2_PSK";
    }
}

/**
 * @brief 将字符串转换为 WiFi 认证模式枚举
 */
static wifi_auth_mode_t str_to_authmode(const char *str)
{
    if (!strcmp(str, "OPEN"))          return WIFI_AUTH_OPEN;
    if (!strcmp(str, "WEP"))           return WIFI_AUTH_WEP;
    if (!strcmp(str, "WPA2_PSK"))      return WIFI_AUTH_WPA2_PSK;
    if (!strcmp(str, "WPA_WPA2_PSK"))  return WIFI_AUTH_WPA_WPA2_PSK;
    ESP_LOGW(TAG, "未知认证模式: %s, 使用默认 WPA_WPA2_PSK", str);
    return WIFI_AUTH_WPA_WPA2_PSK;
}

/* ===== REST API 路由处理函数 ===== */

/**
 * @brief GET /wlan_general - 获取 WiFi 基本信息
 */
static esp_err_t wlan_general_get_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    const char *hide_ssid = (s_modem_wifi_config->ssid_hidden == 0) ? "false" : "true";
    const char *auth_mode = authmode_to_str(s_modem_wifi_config->authmode);

    char *json_str = NULL;
    size_t size = asprintf(&json_str,
        "{\"status\":\"200\", \"ssid\":\"%s\", \"if_hide_ssid\":\"%s\", "
        "\"auth_mode\":\"%s\", \"password\":\"%s\"}",
        s_modem_wifi_config->ssid, hide_ssid, auth_mode, s_modem_wifi_config->password);

    esp_err_t ret = http_helper_send_json_response(req, json_str, size);
    free(json_str);
    return ret;
}

/**
 * @brief POST /wlan_general - 修改 WiFi 基本信息
 */
static esp_err_t wlan_general_post_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    char buf[256] = {0};
    if (http_helper_read_post_body(req, buf, sizeof(buf)) < 0) {
        return ESP_FAIL;
    }

    /* 解析 JSON 请求体 */
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, buf, strlen(buf)) != OS_SUCCESS) {
        ESP_LOGE(TAG, "JSON 解析失败");
        return ESP_FAIL;
    }

    char str_val[64];
    char ssid[64] = "", password[64] = "", hide_ssid[64] = "", auth_mode[64] = "";

    if (json_obj_get_string(&jctx, "ssid", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(ssid, sizeof(ssid), "%s", str_val);
    if (json_obj_get_string(&jctx, "if_hide_ssid", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(hide_ssid, sizeof(hide_ssid), "%s", str_val);
    if (json_obj_get_string(&jctx, "auth_mode", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(auth_mode, sizeof(auth_mode), "%s", str_val);
    if (json_obj_get_string(&jctx, "password", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(password, sizeof(password), "%s", str_val);

    /* 保存到 NVS 并重启生效 */
    nvs_storage_set_str("ssid", ssid);
    nvs_storage_set_str("hide_ssid", hide_ssid);
    nvs_storage_set_str("auth_mode", auth_mode);
    nvs_storage_set_str("password", password);

    /* 发送响应后重启 */
    char *json_str = NULL;
    size_t size = asprintf(&json_str,
        "{\"status\":\"200\", \"ssid\":\"%s\", \"if_hide_ssid\":\"%s\", "
        "\"auth_mode\":\"%s\", \"password\":\"%s\"}",
        ssid, hide_ssid, auth_mode, password);
    http_helper_send_json_response(req, json_str, size);
    free(json_str);

    ESP_LOGI(TAG, "WiFi 配置已更新，即将重启...");
    fflush(stdout);
    esp_restart();
    return ESP_OK;
}

/**
 * @brief GET /wlan_advance - 获取 WiFi 高级设置
 */
static esp_err_t wlan_advance_get_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    size_t bandwidth = (s_modem_wifi_config->bandwidth == WIFI_BW_HT20) ? 20 : 40;
    char *json_str = NULL;
    size_t size = asprintf(&json_str,
        "{\"status\":\"200\", \"bandwidth\":\"%d\", \"channel\":\"%d\"}",
        bandwidth, s_modem_wifi_config->channel);

    esp_err_t ret = http_helper_send_json_response(req, json_str, size);
    free(json_str);
    return ret;
}

/**
 * @brief POST /wlan_advance - 修改 WiFi 高级设置
 */
static esp_err_t wlan_advance_post_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    char buf[256] = {0};
    if (http_helper_read_post_body(req, buf, sizeof(buf)) < 0) {
        return ESP_FAIL;
    }

    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, buf, strlen(buf)) != OS_SUCCESS) {
        ESP_LOGE(TAG, "JSON 解析失败");
        return ESP_FAIL;
    }

    char str_val[64];
    char bandwidth[64] = "", channel[64] = "";

    if (json_obj_get_string(&jctx, "bandwidth", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(bandwidth, sizeof(bandwidth), "%s", str_val);
    if (json_obj_get_string(&jctx, "channel", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(channel, sizeof(channel), "%s", str_val);

    nvs_storage_set_str("bandwidth", bandwidth);
    nvs_storage_set_str("channel", channel);

    char *json_str = NULL;
    size_t size = asprintf(&json_str,
        "{\"status\":\"200\", \"bandwidth\":\"%s\", \"channel\":\"%s\"}",
        bandwidth, channel);
    http_helper_send_json_response(req, json_str, size);
    free(json_str);

    ESP_LOGI(TAG, "WiFi 高级设置已更新，即将重启...");
    fflush(stdout);
    esp_restart();
    return ESP_OK;
}

/**
 * @brief GET /system/station_state - 获取已连接 STA 设备列表
 */
static esp_err_t system_station_get_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    /* 构建 JSON 数组 */
    char *json_str = NULL;
    char *json_old = NULL;
    size_t size = asprintf(&json_str, "{\"station_list\":[");

    modem_http_list_head_t *head = sta_manager_get_list();
    struct modem_netif_sta_info *node;
    SLIST_FOREACH(node, head, field) {
        json_old = strdup(json_str);
        free(json_str);
        size = asprintf(&json_str,
            "%s{\"name_str\":\"%s\",\"mac_str\":\"" MACSTR "\",\"ip_str\":\"" IPSTR
            "\",\"online_time_s\":\"%" PRId64 "\"}%c",
            json_old, node->name, MAC2STR(node->mac), IP2STR(&node->ip),
            (int64_t)node->start_time, node->field.sle_next ? ',' : '\0');
        free(json_old);
    }

    json_old = json_str;
    size = asprintf(&json_str, "%s],\"now_time\":\"%" PRId64 "\"}", json_old, (int64_t)esp_timer_get_time());
    free(json_old);

    esp_err_t ret = http_helper_send_json_response(req, json_str, size);
    free(json_str);
    return ret;
}

/**
 * @brief POST /system/station_state/delete_device - 踢出指定 STA 设备
 */
static esp_err_t system_station_delete_device_post_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    char buf[256] = {0};
    if (http_helper_read_post_body(req, buf, sizeof(buf)) < 0) {
        return ESP_FAIL;
    }

    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, buf, strlen(buf)) != OS_SUCCESS) {
        return ESP_FAIL;
    }

    char str_val[64], mac_str[64] = "";
    if (json_obj_get_string(&jctx, "mac_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(mac_str, sizeof(mac_str), "%s", str_val);
    }

    /* 解析 MAC 地址字符串为字节数组 */
    int val[6] = {0};
    uint8_t mac_byte[6] = {0};
    if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x%*c", &val[0], &val[1], &val[2],
               &val[3], &val[4], &val[5]) == 6) {
        for (int i = 0; i < 6; i++) {
            mac_byte[i] = (uint8_t)val[i];
        }
    }

    /* 断开指定设备 */
    uint16_t aid = 0;
    esp_wifi_ap_get_sta_aid(mac_byte, &aid);
    ESP_LOGI(TAG, "踢出设备 AID=%d", aid);
    esp_wifi_deauth_sta(aid);

    return http_helper_send_empty_ok(req);
}

/**
 * @brief POST /system/station_state/change_name - 修改 STA 设备备注名
 */
static esp_err_t system_station_change_name_post_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    char buf[256] = {0};
    if (http_helper_read_post_body(req, buf, sizeof(buf)) < 0) {
        return ESP_FAIL;
    }

    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, buf, strlen(buf)) != OS_SUCCESS) {
        return ESP_FAIL;
    }

    char str_val[64], name_str[64] = "", mac_str[64] = "";

    if (json_obj_get_string(&jctx, "name_str", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(name_str, sizeof(name_str), "%s", str_val);
    if (json_obj_get_string(&jctx, "mac_str", str_val, sizeof(str_val)) == OS_SUCCESS)
        snprintf(mac_str, sizeof(mac_str), "%s", str_val);

    /* MAC 格式转换: "00:01:02:03:04:05" -> "000102030405" (NVS 键名最大 15 字节) */
    str_delete_char(mac_str, ':');
    nvs_storage_set_str(mac_str, name_str);

    /* 刷新设备列表 */
    sta_manager_update_all();
    return http_helper_send_empty_ok(req);
}

/**
 * @brief GET /login - 登录验证接口
 */
static esp_err_t login_get_handler(httpd_req_t *req)
{
    http_helper_set_cors_headers(req);
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief POST /login - 登录提交（预留）
 */
static esp_err_t login_post_handler(httpd_req_t *req)
{
    return ESP_OK;
}

/**
 * @brief GET 通配符路由 - 静态文件服务（SPIFFS）
 */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    ctx_info_t *ctx = (ctx_info_t *)req->user_ctx;
    if (http_auth_basic_check(req, ctx->basic_auth_info) != ESP_OK) {
        return ESP_OK;
    }

    /* 构建文件路径 */
    char filepath[FILE_PATH_MAX];
    strlcpy(filepath, ctx->rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    /* 打开文件 */
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "无法打开文件: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "文件读取失败");
        return ESP_FAIL;
    }

    http_helper_set_content_type_from_ext(req, filepath);

    /* 分块读取并发送 */
    char *chunk = ctx->rest_context->scratch;
    ssize_t read_bytes;
    do {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "读取文件失败: %s", filepath);
        } else if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "发送文件失败");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "文件发送失败");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    close(fd);
    httpd_resp_send_chunk(req, NULL, 0);  /* 发送结束标志 */
    return ESP_OK;
}

/* ===== WiFi 事件处理 ===== */

/**
 * @brief WiFi/IP 事件处理器
 *
 * 监听 STA 连接/断开事件，同步更新设备管理列表。
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "STA 加入: " MACSTR ", AID=%d", MAC2STR(event->mac), event->aid);
        sta_manager_add_node(event->mac);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "STA 离开: " MACSTR ", AID=%d", MAC2STR(event->mac), event->aid);
        sta_manager_remove_node(event->mac);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        sta_manager_update_all();
    }
}

/* ===== SPIFFS 文件系统初始化 ===== */

/**
 * @brief 初始化 SPIFFS 文件系统
 */
static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 1,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "SPIFFS 挂载或格式化失败");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "未找到 SPIFFS 分区");
        } else {
            ESP_LOGE(TAG, "SPIFFS 初始化失败: %s", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS 分区: 总容量=%d, 已使用=%d", total, used);
    }
    return ESP_OK;
}

/* ===== HTTP 服务器启动/停止 ===== */

/**
 * @brief 注册所有 URI 处理器
 */
static void register_uri_handlers(httpd_handle_t server, ctx_info_t *ctx_info)
{
    /* WiFi 基本配置 API */
    httpd_uri_t wlan_general_get = { .uri="/wlan_general", .method=HTTP_GET,
        .handler=wlan_general_get_handler, .user_ctx=ctx_info };
    httpd_uri_t wlan_general_post = { .uri="/wlan_general", .method=HTTP_POST,
        .handler=wlan_general_post_handler, .user_ctx=ctx_info };

    /* WiFi 高级设置 API */
    httpd_uri_t wlan_advance_get = { .uri="/wlan_advance", .method=HTTP_GET,
        .handler=wlan_advance_get_handler, .user_ctx=ctx_info };
    httpd_uri_t wlan_advance_post = { .uri="/wlan_advance", .method=HTTP_POST,
        .handler=wlan_advance_post_handler, .user_ctx=ctx_info };

    /* STA 设备管理 API */
    httpd_uri_t station_get = { .uri="/system/station_state", .method=HTTP_GET,
        .handler=system_station_get_handler, .user_ctx=ctx_info };
    httpd_uri_t station_delete = { .uri="/system/station_state/delete_device", .method=HTTP_POST,
        .handler=system_station_delete_device_post_handler, .user_ctx=ctx_info };
    httpd_uri_t station_rename = { .uri="/system/station_state/change_name", .method=HTTP_POST,
        .handler=system_station_change_name_post_handler, .user_ctx=ctx_info };

    /* 登录接口 */
    httpd_uri_t login_get_uri = { .uri="/login", .method=HTTP_GET,
        .handler=login_get_handler, .user_ctx=ctx_info };
    httpd_uri_t login_post_uri = { .uri="/login", .method=HTTP_POST,
        .handler=login_post_handler, .user_ctx=ctx_info };

    /* 静态文件服务（通配符匹配） */
    httpd_uri_t common_get = { .uri="/*", .method=HTTP_GET,
        .handler=rest_common_get_handler, .user_ctx=ctx_info };

    /* 注册所有处理器 */
    httpd_register_uri_handler(server, &wlan_general_get);
    httpd_register_uri_handler(server, &wlan_general_post);
    httpd_register_uri_handler(server, &wlan_advance_get);
    httpd_register_uri_handler(server, &wlan_advance_post);
    httpd_register_uri_handler(server, &station_get);
    httpd_register_uri_handler(server, &station_delete);
    httpd_register_uri_handler(server, &station_rename);
    httpd_register_uri_handler(server, &login_get_uri);
    httpd_register_uri_handler(server, &login_post_uri);
    httpd_register_uri_handler(server, &common_get);
}

/**
 * @brief 启动 Web 服务器
 */
static httpd_handle_t start_webserver(const char *base_path)
{
    /* 分配上下文 */
    ctx_info_t *ctx_info = calloc(1, sizeof(ctx_info_t));
    if (!ctx_info) {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }

    ctx_info->rest_context = calloc(1, sizeof(rest_server_context_t));
    if (!ctx_info->rest_context) {
        ESP_LOGE(TAG, "内存分配失败");
        free(ctx_info);
        return NULL;
    }
    strlcpy(ctx_info->rest_context->base_path, base_path,
            sizeof(ctx_info->rest_context->base_path));

    ctx_info->basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    ctx_info->basic_auth_info->username = CONFIG_EXAMPLE_WEB_USERNAME;
    ctx_info->basic_auth_info->password = CONFIG_EXAMPLE_WEB_PASSWORD;

    /* 配置并启动 HTTP 服务器 */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 15;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "启动 Web 服务器，端口: %d", config.server_port);

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "注册 URI 处理器");
        register_uri_handlers(server, ctx_info);
        return server;
    }

    ESP_LOGE(TAG, "Web 服务器启动失败");
    free(ctx_info->rest_context);
    free(ctx_info->basic_auth_info);
    free(ctx_info);
    return NULL;
}

/* ===== 公共 API ===== */

esp_err_t modem_http_deinit(httpd_handle_t server)
{
    if (s_initialized) {
        s_modem_wifi_config = NULL;
        httpd_stop(server);
        s_initialized = false;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t modem_http_init(modem_wifi_config_t *wifi_config)
{
    if (s_initialized) {
        ESP_LOGI(TAG, "Web 服务器已初始化");
        return ESP_FAIL;
    }

    s_modem_wifi_config = wifi_config;

    /* 初始化 STA 管理器 */
    ESP_ERROR_CHECK(sta_manager_init());

    /* 挂载 SPIFFS 文件系统 */
    ESP_ERROR_CHECK(init_spiffs());

    /* 注册 WiFi/IP 事件处理 */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED,
                                                &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifi_event_handler, NULL));

    /* 启动 Web 服务器 */
    s_server = start_webserver(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
    if (!s_server) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Web 服务器启动成功");
    s_initialized = true;
    return ESP_OK;
}

esp_err_t modem_http_get_nvs_wifi_config(modem_wifi_config_t *wifi_config)
{
    char str[64] = "";
    size_t str_size;

    /* 读取 SSID */
    str_size = sizeof(str);
    if (nvs_storage_get_str("ssid", str, &str_size) == ESP_OK) {
        strncpy(wifi_config->ssid, str, sizeof(wifi_config->ssid));
    }

    /* 读取密码 */
    str_size = sizeof(str);
    if (nvs_storage_get_str("password", str, &str_size) == ESP_OK) {
        strncpy(wifi_config->password, str, sizeof(wifi_config->password));
    }

    /* 读取认证模式 */
    str_size = sizeof(str);
    if (nvs_storage_get_str("auth_mode", str, &str_size) == ESP_OK) {
        wifi_config->authmode = str_to_authmode(str);
    }

    /* 读取信道 */
    str_size = sizeof(str);
    if (nvs_storage_get_str("channel", str, &str_size) == ESP_OK) {
        wifi_config->channel = atoi(str);
    }

    /* 读取隐藏 SSID 标志 */
    str_size = sizeof(str);
    if (nvs_storage_get_str("hide_ssid", str, &str_size) == ESP_OK) {
        wifi_config->ssid_hidden = (strcmp(str, "true") == 0) ? 1 : 0;
    }

    /* 读取带宽 */
    str_size = sizeof(str);
    if (nvs_storage_get_str("bandwidth", str, &str_size) == ESP_OK) {
        wifi_config->bandwidth = (strcmp(str, "40") == 0) ? WIFI_BW_HT40 : WIFI_BW_HT20;
    }

    /* 读取最大连接数 */
    str_size = sizeof(str);
    if (nvs_storage_get_str("max_connection", str, &str_size) == ESP_OK) {
        wifi_config->max_connection = atoi(str);
    }

    return ESP_OK;
}
