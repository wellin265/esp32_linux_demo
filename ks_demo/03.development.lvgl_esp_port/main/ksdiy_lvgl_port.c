/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <driver/i2c_master.h>
#include "esp_io_expander_tca9554.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lvgl_port.h"
static const char *TAG = "ksdiy_lvgl_port";

// Using SPI2 in the example
#define LCD_HOST SPI2_HOST
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define KSDIY_PIN_NUM_SCLK 1
#define KSDIY_PIN_NUM_MOSI 0
#define KSDIY_PIN_NUM_MISO -1
#define KSDIY_PIN_NUM_LCD_DC 2
#define KSDIY_PIN_NUM_LCD_RST -1
#define KSDIY_PIN_NUM_LCD_CS 46
#define KSDIY_PIN_NUM_BK_LIGHT -1

#define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18
#define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17

// The pixel number in horizontal and vertical

#define EXAMPLE_LCD_H_RES 280
#define EXAMPLE_LCD_V_RES 240

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#define EXAMPLE_LVGL_TICK_PERIOD_MS 10
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY 10

#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 1
static SemaphoreHandle_t lvgl_mux = NULL;
i2c_master_bus_handle_t touch_i2c_bus_;

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
#include "esp_lcd_touch_cst816s.h"
bool cat816s_found = false;
esp_lcd_touch_handle_t tp = NULL;
esp_err_t find_cst816s(void)
{
    esp_err_t ret = ESP_OK;
    for (uint8_t i = 0x01; i < 0x7F; i++)
    {
        ret = i2c_master_probe(touch_i2c_bus_, i, -1);
        if (ret == ESP_OK)
        {
            printf("The slave has been found, the address is %x\n", i);
            if (i == 0X15)
            {
                ESP_LOGI("I2C", "找到设备，地址: 0x%02X,返回 OK", i);
                cat816s_found = true;
                return ESP_OK; // 找到 0x15 地址，返回 OK
            }
        }
    }

    ESP_LOGE("I2C", "未找到地址 0x15,返回失败");
    cat816s_found = false;
    return ESP_FAIL; // 没有找到设备
}
void i2c_init(void)
{
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = KSDIY_PIN_NUM_TOUCH_IIC_SDA,
        .scl_io_num = KSDIY_PIN_NUM_TOUCH_IIC_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus_));
    find_cst816s();
}
#endif

bool ksdiy_lvgl_lock(int timeout_ms)
{
    // 如果 lvgl_port_lock 函数不可用，可以使用自定义的互斥锁
    // 这里假设 esp_lvgl_port 组件提供了这个函数
    return lvgl_port_lock(timeout_ms);
}

void ksdiy_lvgl_unlock(void)
{
    // 如果 lvgl_port_unlock 函数不可用，可以使用自定义的互斥锁
    lvgl_port_unlock();
}

void ksdiy_lvgl_port_init(void)
{
    i2c_init();
    esp_io_expander_handle_t io_expander = NULL;
    // 初始化扩展IO芯片 先找TCA9554 如果找不到就找TCA9554A 都找不到就不初始化扩展IO芯片
    if ((esp_io_expander_new_i2c_tca9554(touch_i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554, &io_expander) != ESP_OK))
    {
        if (esp_io_expander_new_i2c_tca9554(touch_i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A, &io_expander) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize IO expander");
        }
        else
        {
            esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
            esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1);
            ESP_LOGI(TAG, "Initialize IO expander TCA9554A OK");
        }
    }
    else
    {
        esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
        esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1);
        ESP_LOGI(TAG, "Initialize IO expander TCA9554 OK");
    }
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn off LCD backlight");
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << KSDIY_PIN_NUM_BK_LIGHT};
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    }
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = KSDIY_PIN_NUM_SCLK,
        .mosi_io_num = KSDIY_PIN_NUM_MOSI,
        .miso_io_num = KSDIY_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = KSDIY_PIN_NUM_LCD_DC,
        .cs_gpio_num = KSDIY_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,

    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = KSDIY_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(KSDIY_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
    }
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // 初始化LVGL端口
    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    lvgl_port_init(&port_cfg);

    // 配置显示设备
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * 40, // 缓冲区大小
        .double_buffer = true,                 // 使用双缓冲
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false, // 是否为单色显示器
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,

        .flags = {
            // .sw_rotate = true,
            .buff_dma = true, // 使用DMA缓冲区
            // .buff_spiram = true, // 使用DMA缓冲区
            // .full_refresh = true, // 启用全屏幕刷新
            .swap_bytes = 1,
        }};
    // 添加显示设备
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    lv_display_set_offset(disp, 20, 0);

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED

    if (cat816s_found == true)
    {
        /************* 初始化触摸屏 **************/
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
        tp_io_config.scl_speed_hz = 400000;

        esp_lcd_touch_config_t tp_cfg = {
            .x_max = EXAMPLE_LCD_H_RES,
            .y_max = EXAMPLE_LCD_V_RES,
            .rst_gpio_num = -1,
            .int_gpio_num = -1,
            .flags = {
                .swap_xy = 1,
                .mirror_x = 0,
                .mirror_y = 1,
            },
        };

#if ((ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR >= 4) || ESP_IDF_VERSION_MAJOR > 5)
        esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_handle_t)touch_i2c_bus_, &tp_io_config, &tp_io_handle);
#else
        esp_lcd_new_panel_io_i2c_v2((esp_lcd_i2c_bus_handle_t)touch_i2c_bus_, &tp_io_config, &tp_io_handle);
#endif
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp));
        /* Add touch input (for selected screen) */
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = tp,
        };
        lv_indev_t *lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    }
#endif
}
