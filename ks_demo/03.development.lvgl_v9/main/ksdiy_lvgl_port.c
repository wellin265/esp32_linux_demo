/**
 * @file ksdiy_lvgl_port.c
 * @brief KSDIY LVGL 移植层实现
 *
 * 本文件实现 LVGL v9 在 ESP32-S3 上的移植，包括：
 * - I2C 总线初始化和 CST816S 触摸控制器检测
 * - SPI 总线初始化和 ST7789 LCD 驱动
 * - LVGL 库和 esp_lvgl_port 组件初始化
 * - 显示设备和触摸设备注册
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
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_io_expander_tca9554.h"

/** @brief 日志标签 */
static const char *TAG = "ksdiy_lvgl_port";

/** @brief 使用的 SPI 主机号 */
#define LCD_HOST SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// 请根据您的 LCD 规格更新以下配置 /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** @brief LCD 像素时钟频率 (80MHz) */
#define KSDIY_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

/** @brief 背光控制电平 */
#define KSDIY_LCD_BK_LIGHT_ON_LEVEL 1
#define KSDIY_LCD_BK_LIGHT_OFF_LEVEL !KSDIY_LCD_BK_LIGHT_ON_LEVEL

/* ========== SPI 引脚定义 ========== */
#define KSDIY_PIN_NUM_SCLK 1       /**< SPI 时钟引脚 */
#define KSDIY_PIN_NUM_MOSI 0       /**< SPI MOSI 引脚 */
#define KSDIY_PIN_NUM_MISO -1      /**< SPI MISO 引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_DC 2     /**< LCD 数据/命令选择引脚 */
#define KSDIY_PIN_NUM_LCD_RST -1   /**< LCD 复位引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_CS 46    /**< LCD 片选引脚 */
#define KSDIY_PIN_NUM_BK_LIGHT -1  /**< 背光控制引脚（未使用） */

/* ========== I2C 触摸引脚定义 ========== */
#define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18  /**< I2C 时钟引脚 */
#define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17  /**< I2C 数据引脚 */

/* ========== LCD 分辨率配置 ========== */
#define KSDIY_LCD_H_RES 280  /**< LCD 水平分辨率 */
#define KSDIY_LCD_V_RES 240  /**< LCD 垂直分辨率 */

/* ========== 显示偏移配置（用于修正坐标偏差） ========== */
#define offset_x 20  /**< X 方向偏移量 */
#define offset_y 0   /**< Y 方向偏移量 */

/* ========== LCD 通信参数 ========== */
#define KSDIY_LCD_CMD_BITS 8    /**< 命令位数 */
#define KSDIY_LCD_PARAM_BITS 8  /**< 参数位数 */

/** @brief LVGL 端口句柄 */
static void *lvgl_port_handle = NULL;

/** @brief 是否启用触摸功能 */
#define CONFIG_KSDIY_LCD_TOUCH_ENABLED 1

/** @brief I2C 总线句柄 */
i2c_master_bus_handle_t touch_i2c_bus_;

#if CONFIG_KSDIY_LCD_TOUCH_ENABLED == 1
#include "esp_lcd_touch_cst816s.h"

/** @brief CST816S 是否找到标志 */
bool cat816s_found = false;

/** @brief 触摸控制器句柄 */
esp_lcd_touch_handle_t tp = NULL;

/** @brief LVGL 触摸输入设备指针 */
static lv_indev_t *lvgl_touch_indev = NULL;

/**
 * @brief 扫描 I2C 总线查找 CST816S 触摸控制器
 *
 * 遍历 I2C 地址范围 0x01-0x7E，查找地址为 0x15 的 CST816S 设备。
 *
 * @return ESP_OK 找到设备
 * @return ESP_FAIL 未找到设备
 */
esp_err_t find_cst816s(void)
{
    esp_err_t ret = ESP_OK;
    /* 扫描所有可能的 I2C 地址 */
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
 *
 * 配置 I2C 主机模式，并扫描查找 CST816S 触摸控制器。
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
            .enable_internal_pullup = 1,  /* 启用内部上拉电阻 */
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus_));
    find_cst816s();
}

/**
 * @brief 初始化触摸控制器
 *
 * 如果检测到 CST816S 设备，则初始化触摸控制器驱动。
 *
 * @return ESP_OK 初始化成功
 * @return 其他 初始化失败
 */
static esp_err_t app_touch_init(void)
{
    if (cat816s_found == true)
    {
        /************* 初始化触摸屏 **************/
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
        tp_io_config.scl_speed_hz = 400000;  /* I2C 时钟 400kHz */

        esp_lcd_touch_config_t tp_cfg = {
            .x_max = KSDIY_LCD_H_RES,
            .y_max = KSDIY_LCD_V_RES,
            .rst_gpio_num = -1,
            .int_gpio_num = -1,
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };

        ESP_LOGI(TAG, "Initialize touch controller cst816s");
        esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_handle_t)touch_i2c_bus_, &tp_io_config, &tp_io_handle);

        return esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp);
    }
    return ESP_OK;
}

#endif /* CONFIG_KSDIY_LCD_TOUCH_ENABLED */

/**
 * @brief 获取 LVGL 互斥锁
 *
 * 封装 esp_lvgl_port 组件的锁函数。
 *
 * @param timeout_ms 超时时间（毫秒），-1 表示无限等待
 * @return true 成功获取锁
 * @return false 获取锁超时
 */
bool ksdiy_lvgl_lock(int timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

/**
 * @brief 释放 LVGL 互斥锁
 */
void ksdiy_lvgl_unlock(void)
{
    lvgl_port_unlock();
}

/**
 * @brief 获取 LVGL 显示设备指针
 *
 * @return lv_disp_t* 默认显示设备指针
 */
lv_disp_t *ksdiy_lvgl_get_disp(void)
{
    return lv_disp_get_default();
}

/**
 * @brief 初始化 LVGL 端口
 *
 * 完整的初始化流程：
 * 1. 初始化 I2C 总线并检测触摸控制器
 * 2. 初始化 SPI 总线
 * 3. 安装 ST7789 LCD 驱动
 * 4. 初始化触摸控制器
 * 5. 初始化 LVGL 库和 esp_lvgl_port
 * 6. 注册显示设备和触摸设备
 */
void ksdiy_lvgl_port_init(void)
{
    /* ===== 初始化 I2C 总线和检测触摸控制器 ===== */
    i2c_init();

    /* ===== 配置背光 GPIO ===== */
    ESP_LOGI(TAG, "Turn off LCD backlight");
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
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
        .max_transfer_sz = KSDIY_LCD_H_RES * 240 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* ===== 安装 LCD Panel IO ===== */
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = KSDIY_PIN_NUM_LCD_DC,
        .cs_gpio_num = KSDIY_PIN_NUM_LCD_CS,
        .pclk_hz = KSDIY_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = KSDIY_LCD_CMD_BITS,
        .lcd_param_bits = KSDIY_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
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

    /* ===== 初始化触摸控制器 ===== */
    app_touch_init();

    /* ===== 打开背光 ===== */
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(KSDIY_PIN_NUM_BK_LIGHT, KSDIY_LCD_BK_LIGHT_ON_LEVEL);
    }

    /* ===== 初始化 LVGL 库 ===== */
    lv_init();

    /* ===== 初始化 LVGL 端口 ===== */
    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 10 * 1024;  /* LVGL 任务栈 10KB */
    port_cfg.task_priority = 1;       /* 任务优先级 1 */
    lvgl_port_init(&port_cfg);

    /* ===== 配置显示设备 ===== */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = KSDIY_LCD_H_RES * 20,  /* 缓冲区大小：20 行 */
        .double_buffer = true,                 /* 使用双缓冲提高性能 */
        .hres = KSDIY_LCD_H_RES,
        .vres = KSDIY_LCD_V_RES,
        .monochrome = false,                   /* 彩色显示 */
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,    /* 使用 DMA 传输 */
            .swap_bytes = 1,     /* 交换字节序 */
        }
    };

    /* 添加显示设备 */
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL)
    {
        ESP_LOGE(TAG, "LVGL显示设备添加失败");
        return;
    }

    /* 设置显示偏移（修正坐标偏差） */
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(disp, offset_x, offset_y);
    }

    /* ===== 添加触摸输入设备 ===== */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "LVGL端口初始化完成");
}
