/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ksdiy_example_display.h"

static const char *TAG = "i2c_cst816";

#define I2C_MASTER_SCL_IO      18
#define I2C_MASTER_SDA_IO      17
#define I2C_MASTER_NUM         I2C_NUM_0
#define I2C_MASTER_FREQ_HZ     400000
#define I2C_MASTER_TIMEOUT_MS  1000

#define CST816_DEVICE_ADDR     0x15
#define CST816_REG_GESTURE     0x01
#define CST816_REG_CHIP_ID     0xA7

static void update_display(const char *line2, const char *line3)
{
    char line1[64];
    snprintf(line1, sizeof(line1), "SDA=%d SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    ksdiy_example_display_set_lines(line1, line2, line3);
}

static void i2c_master_init(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
}

static uint8_t i2c_scan_devices(i2c_master_bus_handle_t bus_handle, char *summary, size_t summary_len)
{
    uint8_t found = 0;
    size_t used = 0;

    if (summary_len > 0) {
        summary[0] = '\0';
    }

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus_handle, addr, -1) == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
            found++;

            if (used < summary_len) {
                int written = snprintf(summary + used, summary_len - used,
                                       used == 0 ? "0x%02X" : " 0x%02X", addr);
                if (written > 0) {
                    used += (size_t) written;
                    if (used >= summary_len) {
                        used = summary_len - 1;
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "Scan complete, found %u device(s)", found);
    if (found == 0 && summary_len > 0) {
        snprintf(summary, summary_len, "none");
    }
    return found;
}

static esp_err_t cst816_register_read(i2c_master_dev_handle_t dev_handle,
                                      uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(
        dev_handle,
        &reg_addr, 1,
        data, len,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS
    );
}

static void cst816_read_loop(i2c_master_bus_handle_t bus_handle, uint8_t found_count, const char *scan_summary)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CST816_DEVICE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    uint8_t chip_id = 0;
    esp_err_t ret = cst816_register_read(dev_handle, CST816_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CST816 read failed: %s", esp_err_to_name(ret));
        update_display("touch init failed", esp_err_to_name(ret));
        i2c_master_bus_rm_device(dev_handle);
        return;
    }

    char line2[64];
    char line3[64];
    snprintf(line2, sizeof(line2), "found=%u %s", found_count, scan_summary);
    snprintf(line3, sizeof(line3), "CST816 id=0x%02X waiting", chip_id);
    update_display(line2, line3);

    ESP_LOGI(TAG, "CST816 chip id: 0x%02X", chip_id);

    while (1) {
        uint8_t buf[6];
        ret = cst816_register_read(dev_handle, CST816_REG_GESTURE, buf, sizeof(buf));
        if (ret == ESP_OK) {
            uint8_t gesture = buf[0];
            uint8_t finger = buf[1] & 0x0F;
            uint16_t x = ((buf[2] & 0x0F) << 8) | buf[3];
            uint16_t y = ((buf[4] & 0x0F) << 8) | buf[5];

            if (finger > 0) {
                ESP_LOGI(TAG, "touch: finger=%u gesture=%u x=%u y=%u", finger, gesture, x, y);
                snprintf(line3, sizeof(line3), "touch g=%u x=%u y=%u", gesture, x, y);
                update_display(line2, line3);
            }
        } else {
            ESP_LOGW(TAG, "CST816 read failed: %s", esp_err_to_name(ret));
            update_display(line2, "touch read failed");
        }

        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.i2c", "I2C scan and touch");
    update_display("initializing...", "");

    i2c_master_bus_handle_t bus_handle;
    i2c_master_init(&bus_handle);
    ESP_LOGI(TAG, "I2C initialized: SDA=%d SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    char scan_summary[64];
    uint8_t found = i2c_scan_devices(bus_handle, scan_summary, sizeof(scan_summary));
    cst816_read_loop(bus_handle, found, scan_summary);
}
