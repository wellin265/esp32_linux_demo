#include <string.h>
#include <unistd.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls_crypto.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "sdkconfig.h"
#include "sys/param.h"

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN 64

static const char *TAG = "http_server";

static httpd_handle_t s_server;
static char s_ip_addr[32] = "waiting";
static int s_server_port = 80;

static void update_http_display(const char *line2, const char *line3)
{
    char line1[64];
    snprintf(line1, sizeof(line1), "http://%s:%d", s_ip_addr, s_server_port);
    ksdiy_example_display_set_lines(line1, line2, line3);
}

static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;

    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);

            char param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN];
            char dec_param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
        }
        free(buf);
    }

    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");
    httpd_resp_send(req, "Hello World!", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret;
    int remaining = req->content_len;

    while (remaining > 0) {
        ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }

        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;
        ESP_LOGI(TAG, "Received %d bytes", ret);
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t any_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "Hello World!", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        return ESP_OK;
    }
    if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        return ESP_FAIL;
    }

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret = httpd_req_recv(req, &buf, 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
        update_http_display("ctrl: handlers off", "/ctrl PUT nonzero restore");
    } else {
        extern const httpd_uri_t hello;
        extern const httpd_uri_t echo;
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
        update_http_display("ctrl: handlers on", "/hello /echo /ctrl");
    }

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

const httpd_uri_t hello = {
    .uri = "/hello",
    .method = HTTP_GET,
    .handler = hello_get_handler,
    .user_ctx = NULL,
};

const httpd_uri_t echo = {
    .uri = "/echo",
    .method = HTTP_POST,
    .handler = echo_post_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t ctrl = {
    .uri = "/ctrl",
    .method = HTTP_PUT,
    .handler = ctrl_put_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t any = {
    .uri = "/any",
    .method = HTTP_ANY,
    .handler = any_handler,
    .user_ctx = NULL,
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    s_server_port = config.server_port;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &any);
        return server;
    }

    ESP_LOGE(TAG, "Error starting server");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    return httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server != NULL) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
            update_http_display("wifi disconnected", "server stopped");
        }
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
        if (*server != NULL) {
            update_http_display("/hello /echo /ctrl", "web server running");
        }
    }
}

static void refresh_ip_string(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&ip_info.ip));
    }
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("02.beginner.http_server", "wifi + uri demo");
    update_http_display("init nvs...", "");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    update_http_display("connecting wifi...", "waiting for IP");
    ESP_ERROR_CHECK(example_connect());
    refresh_ip_string();

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &s_server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &s_server));

    s_server = start_webserver();
    if (s_server != NULL) {
        update_http_display("/hello /echo /ctrl", "web server running");
    } else {
        update_http_display("server start failed", "check serial log");
    }

    while (s_server != NULL) {
        refresh_ip_string();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
