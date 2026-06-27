/**
 * @file app_main.c
 * @brief SPI LCD 和触摸屏示例程序
 *
 * 本示例演示如何使用 ESP-IDF 的 esp_lcd 组件驱动 SPI 接口的 LCD 显示屏，
 * 并集成 LVGL 图形库和 CST816S 触摸控制器。
 *
 * 主要功能：
 * - 初始化 SPI 总线和 ST7789 LCD 驱动
 * - 初始化 I2C 触摸控制器 CST816S
 * - 移植 LVGL 图形库到 ESP32
 * - 运行 LVGL 音乐播放器演示
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
#include "driver/i2c.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"

/** @brief 日志标签 */
static const char *TAG = "example";

/** @brief 使用的 SPI 主机号 */
#define LCD_HOST SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// 请根据您的 LCD 规格更新以下配置 /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** @brief LCD 像素时钟频率 (80MHz) */
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

/** @brief 背光开启电平 */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

/* ========== SPI 引脚定义 ========== */
#define KSDIY_PIN_NUM_SCLK        1       /**< SPI 时钟引脚 */
#define KSDIY_PIN_NUM_MOSI        0       /**< SPI MOSI 引脚（主机输出从机输入） */
#define KSDIY_PIN_NUM_MISO        -1      /**< SPI MISO 引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_DC      2       /**< LCD 数据/命令选择引脚 */
#define KSDIY_PIN_NUM_LCD_RST     -1      /**< LCD 复位引脚（未使用） */
#define KSDIY_PIN_NUM_LCD_CS      46      /**< LCD 片选引脚 */
#define KSDIY_PIN_NUM_BK_LIGHT    -1      /**< 背光控制引脚（未使用） */

/* ========== I2C 触摸引脚定义 ========== */
#define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18    /**< I2C 时钟引脚 */
#define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17    /**< I2C 数据引脚 */

/* ========== LCD 分辨率配置 ========== */
#define EXAMPLE_LCD_H_RES 280             /**< LCD 水平分辨率 */
#define EXAMPLE_LCD_V_RES 240             /**< LCD 垂直分辨率 */

/* ========== LCD 通信参数 ========== */
#define EXAMPLE_LCD_CMD_BITS 8            /**< 命令位数 */
#define EXAMPLE_LCD_PARAM_BITS 8          /**< 参数位数 */

/* ========== LVGL 任务配置 ========== */
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2     /**< LVGL 时钟周期（毫秒） */
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500  /**< LVGL 任务最大延时 */
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1    /**< LVGL 任务最小延时 */
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)  /**< LVGL 任务栈大小 */
#define EXAMPLE_LVGL_TASK_PRIORITY 2      /**< LVGL 任务优先级 */

/** @brief 是否启用触摸功能 */
#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 1

/** @brief LVGL 互斥锁，保护 LVGL API 的线程安全 */
static SemaphoreHandle_t lvgl_mux = NULL;

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
#include "esp_lcd_touch_cst816s.h"

/** @brief 触摸句柄 */
esp_lcd_touch_handle_t tp = NULL;

/**
 * @brief 初始化 I2C 总线
 *
 * 配置 I2C 主机模式，用于与 CST816S 触摸控制器通信。
 */
void i2c_init(void)
{
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = KSDIY_PIN_NUM_TOUCH_IIC_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = KSDIY_PIN_NUM_TOUCH_IIC_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000  // 400kHz I2C 时钟
    };
    i2c_param_config(0, &i2c_conf);
    i2c_driver_install(0, i2c_conf.mode, 0, 0, 0);
}
#endif

/* 外部函数声明：LVGL 演示 UI */
extern void example_lvgl_demo_ui(lv_disp_t *disp);

/**
 * @brief LCD 传输完成回调函数
 *
 * 当 LCD 驱动完成一帧数据的传输后，通知 LVGL 刷新完成。
 *
 * @param panel_io 面板 IO 句柄
 * @param edata 事件数据
 * @param user_ctx 用户上下文（显示驱动指针）
 * @return false 表示不需要高优先级任务切换
 */
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

/**
 * @brief LVGL 显示刷新回调函数
 *
 * 将 LVGL 的绘制缓冲区内容写入 LCD 显示区域。
 * 注意：这里添加了 20 像素的 X 偏移量，用于修正触摸和显示的坐标偏差。
 *
 * @param drv 显示驱动指针
 * @param area 待刷新区域
 * @param color_map 颜色数据缓冲区
 */
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1 + 20;  // X 坐标偏移修正
    int offsetx2 = area->x2 + 20;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // 将缓冲区内容写入 LCD 指定区域
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/**
 * @brief 显示旋转更新回调函数
 *
 * 当 LVGL 显示方向改变时，同步更新 LCD 和触摸屏的旋转方向。
 *
 * @param drv 显示驱动指针
 */
static void example_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:  // 0度
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_90:    // 90度
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_180:   // 180度
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_270:   // 270度
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
 *
 * 读取触摸控制器的数据并转换为 LVGL 输入事件。
 * 注意：X 坐标添加了 20 像素的偏移修正。
 *
 * @param drv 输入设备驱动指针
 * @param data 输入数据结构体
 */
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* 读取触摸控制器数据 */
    esp_lcd_touch_read_data(drv->user_data);

    /* 获取触摸坐标 */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        ESP_LOGI(TAG, "TP X :%d Y :%d ", touchpad_x[0] + 20, touchpad_y[0]);

        data->point.x = touchpad_x[0] + 20;  // X 坐标偏移修正
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
 *
 * 由 esp_timer 定期调用，通知 LVGL 经过的时间。
 *
 * @param arg 未使用
 */
static void example_increase_lvgl_tick(void *arg)
{
    /* 通知 LVGL 经过的毫秒数 */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

/**
 * @brief 获取 LVGL 互斥锁
 *
 * LVGL API 不是线程安全的，调用前需要获取锁。
 *
 * @param timeout_ms 超时时间（毫秒），-1 表示无限等待
 * @return true 成功获取锁，false 获取失败
 */
bool example_lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

/**
 * @brief 释放 LVGL 互斥锁
 */
void example_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

/**
 * @brief LVGL 主任务
 *
 * 在独立任务中运行 LVGL 定时器处理器，处理 UI 更新和动画。
 *
 * @param arg 未使用
 */
static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;

    while (1)
    {
        // 获取互斥锁（LVGL API 非线程安全）
        if (example_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            example_lvgl_unlock();
        }

        // 限制延时范围
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
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 SPI 总线
 * 2. 安装 ST7789 LCD 驱动
 * 3. 初始化 CST816S 触摸控制器
 * 4. 初始化 LVGL 库
 * 5. 创建 LVGL 任务
 * 6. 启动 LVGL 演示程序
 */
void app_main(void)
{
    static lv_disp_draw_buf_t disp_buf;  // LVGL 绘制缓冲区
    static lv_disp_drv_t disp_drv;       // LVGL 显示驱动

    /* ===== 初始化背光 GPIO ===== */
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
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),  // 最大传输大小
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
        .on_color_trans_done = example_notify_lvgl_flush_ready,
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

    /* 初始化 LCD 面板 */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* ===== 初始化触摸控制器 ===== */
    i2c_init();

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();

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

    ESP_LOGI(TAG, "Initialize touch controller cst816s");
    esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)0, &tp_io_config, &tp_io_handle);
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp));
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

    /* 分配 LVGL 绘制缓冲区（建议大小为屏幕的 1/10） */
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 40);

    /* 注册显示驱动到 LVGL */
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* ===== 安装 LVGL 时钟定时器 ===== */
    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* 注册触摸输入设备 */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;
    lv_indev_drv_register(&indev_drv);
#endif

    /* 创建 LVGL 互斥锁 */
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);

    /* 创建 LVGL 任务 */
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    /* ===== 启动 LVGL 演示 ===== */
    ESP_LOGI(TAG, "Display LVGL Meter Widget");
    if (example_lvgl_lock(-1))
    {
        lv_demo_music();  // 运行音乐播放器演示
        example_lvgl_unlock();
    }
}
