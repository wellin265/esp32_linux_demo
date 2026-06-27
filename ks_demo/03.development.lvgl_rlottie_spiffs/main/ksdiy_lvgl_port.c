/**
 * @file ksdiy_lvgl_port.c
 * @brief 酷世DIY Korvo 开发板 LVGL v8.3 移植层
 *
 * 本文件实现 ESP32-S3 平台上 LVGL v8.3 图形库的硬件移植，包括：
 * - ST7789 LCD 显示驱动（SPI 接口，280x240 分辨率）
 * - CST816S 电容触摸屏驱动（I2C 接口）
 * - TCA9554 IO 扩展芯片驱动（用于背光控制）
 * - LVGL 互斥锁和任务管理
 *
 * 硬件连接：
 * - LCD SPI: SCLK=GPIO1, MOSI=GPIO0, DC=GPIO2, CS=GPIO46
 * - 触摸 I2C: SCL=GPIO18, SDA=GPIO17
 * - 背光控制: 通过 TCA9554 IO 扩展芯片
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @note 基于 ESP-IDF v5.x 开发
 */

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

static const char *TAG = "ksdiy_lvgl_port";

/*============================================================================*/
/*                              宏定义配置区域                                 */
/*============================================================================*/

/** @brief LCD 使用的 SPI 主机 */
/** @brief LCD 使用的 SPI 主机 */
#define LCD_HOST SPI2_HOST

/** @brief IO 扩展芯片 I2C 地址（TCA9554A） */
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)

/** @brief IO 扩展芯片 I2C 地址（TCA9554） */
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)

/*============================================================================*/
/*                          LCD 硬件参数配置                                   */
/*============================================================================*/

/** @brief LCD 像素时钟频率 (80MHz) */
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

/** @brief 背光开启电平 */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1

/** @brief 背光关闭电平 */
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

/*============================================================================*/
/*                          SPI 引脚定义（LCD）                                */
/*============================================================================*/

#define KSDIY_PIN_NUM_SCLK 1      /*!< SPI 时钟引脚 */
#define KSDIY_PIN_NUM_MOSI 0      /*!< SPI MOSI 引脚 */
#define KSDIY_PIN_NUM_MISO -1     /*!< SPI MISO 引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_DC 2    /*!< LCD 数据/命令选择引脚 */
#define KSDIY_PIN_NUM_LCD_RST -1  /*!< LCD 复位引脚（使用 IO 扩展芯片） */
#define KSDIY_PIN_NUM_LCD_CS 46   /*!< LCD 片选引脚 */
#define KSDIY_PIN_NUM_BK_LIGHT -1 /*!< 背光控制引脚（使用 IO 扩展芯片） */

/*============================================================================*/
/*                          I2C 引脚定义（触摸）                               */
/*============================================================================*/

#define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18  /*!< 触摸 I2C 时钟引脚 */
#define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17  /*!< 触摸 I2C 数据引脚 */

/*============================================================================*/
/*                          LCD 分辨率配置                                     */
/*============================================================================*/

#define EXAMPLE_LCD_H_RES 280  /*!< LCD 水平分辨率 */
#define EXAMPLE_LCD_V_RES 240  /*!< LCD 垂直分辨率 */

/*============================================================================*/
/*                          LCD 通信参数                                       */
/*============================================================================*/

#define EXAMPLE_LCD_CMD_BITS 8    /*!< LCD 命令位宽 */
#define EXAMPLE_LCD_PARAM_BITS 8  /*!< LCD 参数位宽 */

/*============================================================================*/
/*                          LVGL 任务配置                                      */
/*============================================================================*/

#define EXAMPLE_LVGL_TICK_PERIOD_MS 10       /*!< LVGL 时钟周期（毫秒） */
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500   /*!< LVGL 任务最大延迟 */
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1     /*!< LVGL 任务最小延迟 */
#define EXAMPLE_LVGL_TASK_STACK_SIZE (30 * 1024)  /*!< LVGL 任务栈大小 */
#define EXAMPLE_LVGL_TASK_PRIORITY 10        /*!< LVGL 任务优先级 */

/** @brief 是否启用触摸功能 */
#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 1

/*============================================================================*/
/*                          全局变量定义                                       */
/*============================================================================*/

/** @brief LVGL 互斥锁句柄 */
static SemaphoreHandle_t lvgl_mux = NULL;

/** @brief 触摸 I2C 总线句柄 */
i2c_master_bus_handle_t touch_i2c_bus_;

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
#include "esp_lcd_touch_cst816s.h"

/** @brief CST816S 触摸芯片是否找到 */
bool cat816s_found = false;

/** @brief 触摸屏句柄 */
esp_lcd_touch_handle_t tp = NULL;

/**
 * @brief 扫描 I2C 总线查找 CST816S 触摸芯片
 *
 * 遍历 I2C 地址范围 0x01-0x7E，查找地址为 0x15 的 CST816S 芯片。
 *
 * @return esp_err_t
 *     - ESP_OK: 找到 CST816S 芯片
 *     - ESP_FAIL: 未找到芯片
 */
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

/**
 * @brief 初始化触摸 I2C 总线
 *
 * 配置 I2C 主机总线并扫描查找 CST816S 触摸芯片。
 * I2C 参数：
 * - 端口: I2C_NUM_1
 * - SCL: GPIO18
 * - SDA: GPIO17
 * - 启用内部上拉电阻
 */
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

extern void ksdiy_lvgl_demo_ui(lv_disp_t *disp);

/**
 * @brief LCD 刷新完成回调函数
 *
 * 当 LCD DMA 传输完成时调用，通知 LVGL 刷新已完成。
 *
 * @param panel_io LCD 面板 IO 句柄
 * @param edata 事件数据
 * @param user_ctx 用户上下文（LVGL 显示驱动）
 * @return bool 是否需要高优先级任务切换
 */
static bool ksdiy_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

/**
 * @brief LVGL 显示刷新回调函数
 *
 * 将 LVGL 绘制缓冲区的内容传输到 LCD 显示。
 * 注意：由于屏幕偏移，x 坐标需要加上 20 像素的偏移量。
 *
 * @param drv LVGL 显示驱动
 * @param area 需要刷新的区域
 * @param color_map 颜色数据缓冲区
 */
static void ksdiy_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1 + 20;
    int offsetx2 = area->x2 + 20;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/**
 * @brief LVGL 显示旋转更新回调
 *
 * 当 LVGL 显示驱动参数更新时调用，用于处理屏幕旋转。
 * 根据 LVGL 的旋转设置，同步调整 LCD 和触摸屏的方向。
 *
 * @param drv LVGL 显示驱动
 */
static void ksdiy_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    }
}

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
/**
 * @brief LVGL 触摸输入回调函数
 *
 * 读取 CST816S 触摸屏数据并转换为 LVGL 输入事件。
 * 注意：由于屏幕偏移，x 坐标需要加上 20 像素的偏移量。
 *
 * @param drv LVGL 输入设备驱动
 * @param data 输入数据（输出参数）
 */
static void ksdiy_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read touch controller data */
    esp_lcd_touch_read_data(drv->user_data);

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0)//这个屏幕触摸有点偏移  得自己加上
    {
        ESP_LOGI(TAG, "TP X :%d Y :%d ", touchpad_x[0] + 20, touchpad_y[0]);

        data->point.x = touchpad_x[0] + 20;
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

/**
 * @brief LVGL 时钟增量回调函数
 *
 * 由 ESP 定时器周期性调用，通知 LVGL 已经过的时间。
 *
 * @param arg 未使用的参数
 */
static void ksdiy_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

/**
 * @brief 获取 LVGL 互斥锁
 *
 * 由于 LVGL API 不是线程安全的，在调用 LVGL API 之前必须获取此锁。
 *
 * @param timeout_ms 超时时间（毫秒），-1 表示永久等待
 * @return true 成功获取锁
 * @return false 获取锁超时
 */
bool ksdiy_lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

/**
 * @brief 释放 LVGL 互斥锁
 *
 * 在完成 LVGL API 调用后释放锁，允许其他任务访问 LVGL。
 */
void ksdiy_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

/**
 * @brief LVGL 主任务
 *
 * 周期性调用 LVGL 定时器处理器，处理 UI 更新、动画和输入事件。
 * 任务会根据 LVGL 返回的延迟时间动态调整下一次调用时间。
 *
 * @param arg 未使用的参数
 */
static void ksdiy_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (ksdiy_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            ksdiy_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

/**
 * @brief 初始化 LVGL 端口
 *
 * 完整的 LVGL 硬件移植初始化流程：
 * 1. 初始化 I2C 总线和 IO 扩展芯片（TCA9554/TCA9554A）
 * 2. 初始化 SPI 总线和 ST7789 LCD 驱动
 * 3. 初始化 CST816S 触摸屏（如果存在）
 * 4. 分配 LVGL 绘制缓冲区
 * 5. 注册 LVGL 显示和输入设备驱动
 * 6. 创建 LVGL 时钟定时器
 * 7. 创建 LVGL 主任务
 *
 * @note 此函数应在 app_main() 中调用
 */
void ksdiy_lvgl_port_init(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    i2c_init();
    esp_io_expander_handle_t io_expander = NULL;
    //初始化扩展IO芯片 先找TCA9554 如果找不到就找TCA9554A 都找不到就不初始化扩展IO芯片
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
        .on_color_trans_done = ksdiy_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
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
    }
#endif
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(KSDIY_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
    }
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 20);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = ksdiy_lvgl_flush_cb;
    disp_drv.drv_update_cb = ksdiy_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &ksdiy_increase_lvgl_tick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    if (cat816s_found == true)
    {
        static lv_indev_drv_t indev_drv; // Input device driver (Touch)
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.disp = disp;
        indev_drv.read_cb = ksdiy_lvgl_touch_cb;
        indev_drv.user_data = tp;

        lv_indev_drv_register(&indev_drv);
        ESP_LOGI(TAG, "Add tp task");
    }
#endif

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(ksdiy_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    // Lock the mutex due to the LVGL APIs are not thread-safe
    // if (ksdiy_lvgl_lock(-1))
    // {
    //     // ksdiy_lvgl_demo_ui(disp);
    //     // lv_demo_music();
    //     // Release the mutex
    //     ksdiy_lvgl_unlock();
    // }
}
