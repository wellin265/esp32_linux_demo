/**
 * @file ksdiy_lvgl_port.c
 * @brief KSDIY LVGL v8.3 移植层实现
 *
 * 本文件实现 LVGL v8.3 在 ESP32-S3 上的移植，包括：
 * - I2C 总线初始化和 CST816S 触摸控制器检测
 * - TCA9554 IO 扩展芯片初始化
 * - SPI 总线初始化和 ST7789 LCD 驱动
 * - LVGL 库初始化和任务创建
 *
 * @copyright SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 * @license SPDX-License-Identifier: CC0-1.0
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

/** @brief 日志标签 */
static const char *TAG = "ksdiy_lvgl_port";

/** @brief 使用的 SPI 主机号 */
#define LCD_HOST SPI2_HOST

/** @brief TCA9554 IO 扩展器 I2C 地址 */
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// 请根据您的 LCD 规格更新以下配置 /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** @brief LCD 像素时钟频率 (80MHz) */
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

/** @brief 背光控制电平 */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

/* ========== SPI 引脚定义 ========== */
#define KSDIY_PIN_NUM_SCLK 1       /**< SPI 时钟引脚 */
#define KSDIY_PIN_NUM_MOSI 0       /**< SPI MOSI 引脚 */
#define KSDIY_PIN_NUM_MISO -1      /**< SPI MISO 引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_DC 2     /**< LCD 数据/命令选择引脚 */
#define KSDIY_PIN_NUM_LCD_RST -1   /**< LCD 复位引脚（使用 IO 扩展器） */
#define KSDIY_PIN_NUM_LCD_CS 46    /**< LCD 片选引脚 */
#define KSDIY_PIN_NUM_BK_LIGHT -1  /**< 背光控制引脚（使用 IO 扩展器） */

/* ========== I2C 触摸引脚定义 ========== */
#define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18  /**< I2C 时钟引脚 */
#define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17  /**< I2C 数据引脚 */

/* ========== LCD 分辨率配置 ========== */
#define EXAMPLE_LCD_H_RES 280  /**< LCD 水平分辨率 */
#define EXAMPLE_LCD_V_RES 240  /**< LCD 垂直分辨率 */

/* ========== LCD 通信参数 ========== */
#define EXAMPLE_LCD_CMD_BITS 8    /**< 命令位数 */
#define EXAMPLE_LCD_PARAM_BITS 8  /**< 参数位数 */

/* ========== LVGL 任务配置 ========== */
#define EXAMPLE_LVGL_TICK_PERIOD_MS 10       /**< LVGL 时钟周期（毫秒） */
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500   /**< LVGL 任务最大延时 */
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1     /**< LVGL 任务最小延时 */
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)  /**< LVGL 任务栈大小 */
#define EXAMPLE_LVGL_TASK_PRIORITY 10        /**< LVGL 任务优先级 */

/** @brief 是否启用触摸功能 */
#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 1

/** @brief LVGL 互斥锁 */
static SemaphoreHandle_t lvgl_mux = NULL;

/** @brief I2C 总线句柄 */
i2c_master_bus_handle_t touch_i2c_bus_;

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
#include "esp_lcd_touch_cst816s.h"

/** @brief CST816S 是否找到标志 */
bool cat816s_found = false;

/** @brief 触摸控制器句柄 */
esp_lcd_touch_handle_t tp = NULL;

/**
 * @brief 扫描 I2C 总线查找 CST816S 触摸控制器
 *
 * @return ESP_OK 找到设备
 * @return ESP_FAIL 未找到设备
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
            if (i == 0X15)  /* CST816S 的 I2C 地址 */
            {
                ESP_LOGI("I2C", "找到设备，地址: 0x%02X,返回 OK", i);
                cat816s_found = true;
                return ESP_OK;
            }
        }
    }

    ESP_LOGE("I2C", "未找到地址 0x15,返回失败");
    cat816s_found = false;
    return ESP_FAIL;
}

/**
 * @brief 初始化 I2C 总线
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

/* 外部函数声明 */
extern void ksdiy_lvgl_demo_ui(lv_disp_t *disp);

/**
 * @brief LCD 传输完成回调函数
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
 * 注意：添加了 20 像素的 X 偏移量修正坐标偏差。
 */
static void ksdiy_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1 + 20;  /* X 坐标偏移修正 */
    int offsetx2 = area->x2 + 20;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/**
 * @brief 显示旋转更新回调函数
 */
static void ksdiy_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_90:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_180:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_270:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    }
}

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
/**
 * @brief LVGL 触摸输入回调函数
 */
static void ksdiy_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_read_data(drv->user_data);
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        ESP_LOGI(TAG, "TP X :%d Y :%d ", touchpad_x[0] + 20, touchpad_y[0]);
        data->point.x = touchpad_x[0] + 20;  /* X 坐标偏移修正 */
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
 * @brief LVGL 时钟增加回调函数
 */
static void ksdiy_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

/**
 * @brief 获取 LVGL 互斥锁
 */
bool ksdiy_lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

/**
 * @brief 释放 LVGL 互斥锁
 */
void ksdiy_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

/**
 * @brief LVGL 主任务
 */
static void ksdiy_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        if (ksdiy_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            ksdiy_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

/**
 * @brief 初始化 LVGL 端口
 *
 * 完整的初始化流程：
 * 1. 初始化 I2C 总线和 IO 扩展器
 * 2. 初始化 SPI 总线和 ST7789 LCD 驱动
 * 3. 初始化 CST816S 触摸控制器
 * 4. 初始化 LVGL 库和创建任务
 */
void ksdiy_lvgl_port_init(void)
{
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;

    /* ===== 初始化 I2C 总线 ===== */
    i2c_init();

    /* ===== 初始化 IO 扩展芯片 TCA9554 ===== */
    esp_io_expander_handle_t io_expander = NULL;
    if ((esp_io_expander_new_i2c_tca9554(touch_i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554, &io_expander) != ESP_OK))
    {
        if (esp_io_expander_new_i2c_tca9554(touch_i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A, &io_expander) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize IO expander");
        }
        else
        {
            /* 设置 IO 扩展器引脚为输出模式并打开背光和复位 */
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

    /* ===== 配置背光 GPIO ===== */
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn off LCD backlight");
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << KSDIY_PIN_NUM_BK_LIGHT
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    }

    /* ===== 初始化 SPI 总线 ===== */
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

    /* ===== 安装 LCD Panel IO ===== */
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
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    /* ===== 安装 ST7789 LCD 驱动 ===== */
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
    /* ===== 初始化触摸控制器 ===== */
    if (cat816s_found == true)
    {
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

    /* ===== 打开背光 ===== */
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(KSDIY_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
    }

    /* ===== 初始化 LVGL 库 ===== */
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    /* 分配 LVGL 绘制缓冲区 */
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 20);

    /* 注册显示驱动 */
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = ksdiy_lvgl_flush_cb;
    disp_drv.drv_update_cb = ksdiy_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* ===== 安装 LVGL 时钟定时器 ===== */
    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &ksdiy_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* 注册触摸输入设备 */
    if (cat816s_found == true)
    {
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.disp = disp;
        indev_drv.read_cb = ksdiy_lvgl_touch_cb;
        indev_drv.user_data = tp;
        lv_indev_drv_register(&indev_drv);
        ESP_LOGI(TAG, "Add tp task");
    }
#endif

    /* 创建互斥锁和 LVGL 任务 */
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(ksdiy_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);
}
