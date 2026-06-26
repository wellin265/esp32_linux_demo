/**
 * @file ksdiy_lcd_port.c
 * @brief SPI LCD 显示屏驱动移植层
 *
 * 本文件实现 ESP32-S3 通过 SPI 接口驱动 ST7789 LCD 控制器：
 * - SPI 总线初始化
 * - LCD 面板驱动安装
 * - 背光控制
 * - 显示方向和颜色配置
 *
 * 硬件配置：
 * - LCD 控制器: ST7789
 * - 分辨率: 280x240
 * - 接口: SPI（80MHz）
 * - 颜色深度: 16位（RGB565）
 *
 * @author Kevincoooool
 * @date 2021-05-25
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <driver/i2c_master.h>

#include "esp_err.h"
#include "esp_log.h"
#include "ksdiy_lcd_port.h"

/** @brief 日志标签 */
static const char *TAG = "ksdiy_lcd_port";

/** @brief 使用 SPI2 主机 */
#define LCD_HOST SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// 请根据您的 LCD 规格更新以下配置 //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** @brief LCD 像素时钟频率 (80MHz) */
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

/** @brief 背光开启电平 */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1

/** @brief 背光关闭电平 */
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

/* ===== SPI 引脚配置 ===== */
#define KSDIY_PIN_NUM_SCLK 1    /**< SPI 时钟引脚 */
#define KSDIY_PIN_NUM_MOSI 0    /**< SPI MOSI 引脚 */
#define KSDIY_PIN_NUM_MISO -1   /**< SPI MISO 引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_DC 2  /**< LCD 数据/命令选择引脚 */
#define KSDIY_PIN_NUM_LCD_RST -1/**< LCD 复位引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_CS 46 /**< LCD 片选引脚 */
#define KSDIY_PIN_NUM_BK_LIGHT -1/**< 背光控制引脚（未使用） */

/* ===== 触摸屏 I2C 引脚配置 ===== */
#define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18  /**< 触摸屏 I2C 时钟引脚 */
#define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17  /**< 触摸屏 I2C 数据引脚 */

/* ===== LCD 分辨率配置 ===== */
#define EXAMPLE_LCD_H_RES 280  /**< LCD 水平分辨率 */
#define EXAMPLE_LCD_V_RES 240  /**< LCD 垂直分辨率 */

/* ===== SPI 通信配置 ===== */
#define EXAMPLE_LCD_CMD_BITS 8    /**< 命令位数 */
#define EXAMPLE_LCD_PARAM_BITS 8  /**< 参数位数 */

/** @brief LCD 面板句柄（全局变量，供其他模块使用） */
esp_lcd_panel_handle_t panel_handle = NULL;

/**
 * @brief 初始化 SPI LCD 显示屏
 *
 * 初始化流程：
 * 1. 配置背光 GPIO（如果使用）
 * 2. 初始化 SPI 总线
 * 3. 安装 LCD Panel IO（SPI 模式）
 * 4. 安装 ST7789 面板驱动
 * 5. 配置显示方向和颜色
 * 6. 点亮屏幕
 * 7. 清屏（填充白色）
 */
void ksdiy_lvgl_lcd_init(void)
{
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
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),  /* 最大传输大小 */
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* ===== 安装 Panel IO ===== */
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = KSDIY_PIN_NUM_LCD_DC,
        .cs_gpio_num = KSDIY_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,  /* SPI 传输队列深度 */
    };

    /* 将 LCD 连接到 SPI 总线 */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    /* ===== 配置 LCD 面板 ===== */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = KSDIY_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  /* RGB 元素顺序 */
        .bits_per_pixel = 16,  /* 16 位色深 */
    };

    /* ===== 安装 ST7789 面板驱动 ===== */
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    /* ===== 初始化 LCD 面板 ===== */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));      /* 硬件复位 */
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));       /* 初始化 */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));  /* 交换 XY 坐标 */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));  /* 镜像设置 */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));   /* 颜色反转 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));    /* 开启显示 */

    /* ===== 点亮背光 ===== */
    if (KSDIY_PIN_NUM_BK_LIGHT != -1)
    {
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(KSDIY_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
    }

    /* ===== 清屏（填充白色） ===== */
    uint16_t buffer[240] = {0xFFFF};  /* 白色缓冲区 */
    for (int y = 0; y < 280; y++)
    {
        /* 逐行填充白色 */
        esp_lcd_panel_draw_bitmap(panel_handle, 0 + 20, y, 240 + 20, y + 1, buffer);
    }
}
