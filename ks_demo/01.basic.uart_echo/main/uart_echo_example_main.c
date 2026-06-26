/*
 * Public Domain (or CC0 licensed, at your option.)
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"
#include "sdkconfig.h"

#define ECHO_TEST_TXD          CONFIG_EXAMPLE_UART_TXD
#define ECHO_TEST_RXD          CONFIG_EXAMPLE_UART_RXD
#define ECHO_TEST_RTS          UART_PIN_NO_CHANGE
#define ECHO_TEST_CTS          UART_PIN_NO_CHANGE
#define ECHO_UART_PORT_NUM     CONFIG_EXAMPLE_UART_PORT_NUM
#define ECHO_UART_BAUD_RATE    CONFIG_EXAMPLE_UART_BAUD_RATE
#define ECHO_TASK_STACK_SIZE   CONFIG_EXAMPLE_TASK_STACK_SIZE
#define BUF_SIZE               1024

static const char *TAG = "UART TEST";

static void sanitize_uart_text(const uint8_t *src, int len, char *dst, size_t dst_size)
{
    size_t pos = 0;

    if (dst_size == 0) {
        return;
    }

    for (int i = 0; i < len && pos + 1 < dst_size; i++) {
        char ch = (char) src[i];
        if (ch == '\r' || ch == '\n') {
            continue;
        }
        dst[pos++] = isprint((unsigned char) ch) ? ch : '.';
    }

    if (pos == 0) {
        snprintf(dst, dst_size, "<binary or empty>");
    } else {
        dst[pos] = '\0';
    }
}

static void update_uart_display(size_t total_bytes, const char *message)
{
    char line1[64];
    char line2[64];
    char line3[64];

    snprintf(line1, sizeof(line1), "UART%d TX=%d RX=%d",
             ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD);
    snprintf(line2, sizeof(line2), "baud=%d total=%u",
             ECHO_UART_BAUD_RATE, (unsigned) total_bytes);
    snprintf(line3, sizeof(line3), "last: %s", message);

    ksdiy_example_display_set_lines(line1, line2, line3);
}

static void echo_task(void *arg)
{
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(
        ECHO_UART_PORT_NUM,
        BUF_SIZE * 2,
        0,
        0,
        NULL,
        intr_alloc_flags
    ));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        ECHO_UART_PORT_NUM,
        ECHO_TEST_TXD,
        ECHO_TEST_RXD,
        ECHO_TEST_RTS,
        ECHO_TEST_CTS
    ));

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate UART buffer");
        update_uart_display(0, "malloc failed");
        vTaskDelete(NULL);
        return;
    }

    size_t total_bytes = 0;
    update_uart_display(total_bytes, "waiting for input");

    while (1) {
        int len = uart_read_bytes(
            ECHO_UART_PORT_NUM,
            data,
            BUF_SIZE - 1,
            pdMS_TO_TICKS(20)
        );

        if (len > 0) {
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data, len);
            total_bytes += (size_t) len;

            char message[48];
            sanitize_uart_text(data, len, message, sizeof(message));
            update_uart_display(total_bytes, message);
            ESP_LOGI(TAG, "Recv len=%d text=%s", len, message);
        }
    }
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.uart_echo", "UART live echo");
    update_uart_display(0, "starting...");

    xTaskCreate(
        echo_task,
        "uart_echo_task",
        ECHO_TASK_STACK_SIZE,
        NULL,
        10,
        NULL
    );
}
