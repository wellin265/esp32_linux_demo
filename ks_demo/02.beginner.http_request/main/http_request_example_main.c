#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "sdkconfig.h"

#define WEB_SERVER "example.com"
#define WEB_PORT   "80"
#define WEB_PATH   "/"

static const char *TAG = "http_request";
static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
                             "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";

static void update_http_request_display(const char *line2, const char *line3)
{
    char line1[64];
    snprintf(line1, sizeof(line1), "%s%s", WEB_SERVER, WEB_PATH);
    ksdiy_example_display_set_lines(line1, line2, line3);
}

static void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    struct in_addr *addr;
    int s;
    int r;
    char recv_buf[64];

    while (1) {
        update_http_request_display("resolving dns...", WEB_SERVER);
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            update_http_request_display("dns failed", "retry in 1s");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if (s < 0) {
            ESP_LOGE(TAG, "Failed to allocate socket");
            freeaddrinfo(res);
            update_http_request_display("socket alloc fail", "retry in 1s");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            update_http_request_display("connect failed", "retry in 4s");
            vTaskDelay(pdMS_TO_TICKS(4000));
            continue;
        }

        freeaddrinfo(res);
        update_http_request_display("connected", "sending GET /");

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "socket send failed");
            close(s);
            update_http_request_display("send failed", "retry in 4s");
            vTaskDelay(pdMS_TO_TICKS(4000));
            continue;
        }

        struct timeval receiving_timeout = {
            .tv_sec = 5,
            .tv_usec = 0,
        };
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "failed to set socket receiving timeout");
            close(s);
            update_http_request_display("timeout setup fail", "retry in 4s");
            vTaskDelay(pdMS_TO_TICKS(4000));
            continue;
        }

        int total_read = 0;
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf) - 1);
            if (r > 0) {
                total_read += r;
                for (int i = 0; i < r; i++) {
                    putchar(recv_buf[i]);
                }
            }
        } while (r > 0);

        ESP_LOGI(TAG, "done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);

        char bytes_line[32];
        snprintf(bytes_line, sizeof(bytes_line), "recv=%d bytes", total_read);
        update_http_request_display("request complete", bytes_line);

        for (int countdown = 10; countdown >= 0; countdown--) {
            char wait_line[32];
            snprintf(wait_line, sizeof(wait_line), "restart in %ds", countdown);
            update_http_request_display("request complete", wait_line);
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("02.beginner.http_request", "socket GET demo");
    update_http_request_display("init wifi...", "");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    update_http_request_display("wifi connected", "starting task");
    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);
}
