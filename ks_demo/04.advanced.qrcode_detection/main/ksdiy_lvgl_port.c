/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file ksdiy_lvgl_port.c
 * @brief KSDIY LVGL 移植层实现
 *
 * 本文件实现了 ESP32 平台上 LVGL 图形库的移植，包括：
 * - SPI LCD 显示屏驱动初始化（ST7789）
 * - I2C 触摸屏驱动初始化（CST816S）
 * - IO 扩展芯片初始化（TCA9554/TCA9554A）
 * - LVGL 适配器配置与启动
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
 #include "esp_lv_adapter.h"
 
 /** 日志标签 */
 static const char *TAG = "ksdiy_lvgl_port";
 
 /*******************************************************************************
  * 硬件配置宏定义
  ******************************************************************************/
 
 /** SPI 主机选择，使用 SPI2 */
 #define LCD_HOST SPI2_HOST
 
 /** TCA9554A IO扩展芯片 I2C 地址（A0/A1/A2 全接地） */
 #define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)
 
 /** TCA9554 IO扩展芯片 I2C 地址（A0/A1/A2 全接地） */
 #define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)
 
 /*******************************************************************************
  * LCD 显示屏配置
  * 请根据实际使用的 LCD 规格修改以下配置
  ******************************************************************************/
 
 /** SPI 时钟频率：80MHz */
 #define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
 
 /** 背光开启电平：高电平有效 */
 #define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
 
 /** 背光关闭电平：与开启电平相反 */
 #define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
 
 /** SPI 时钟引脚 */
 #define KSDIY_PIN_NUM_SCLK 1
 
 /** SPI MOSI 数据引脚 */
 #define KSDIY_PIN_NUM_MOSI 0
 
 /** SPI MISO 数据引脚（未使用，设为 -1） */
 #define KSDIY_PIN_NUM_MISO -1
 
 /** LCD 数据/命令选择引脚 */
 #define KSDIY_PIN_NUM_LCD_DC 2
 
 /** LCD 复位引脚（未使用，设为 -1） */
 #define KSDIY_PIN_NUM_LCD_RST -1
 
 /** LCD 片选引脚 */
 #define KSDIY_PIN_NUM_LCD_CS 46
 
 /** LCD 背光控制引脚（未使用，设为 -1） */
 #define KSDIY_PIN_NUM_BK_LIGHT -1
 
 /** 触摸屏 I2C SCL 引脚 */
 #define KSDIY_PIN_NUM_TOUCH_IIC_SCL 18
 
 /** 触摸屏 I2C SDA 引脚 */
 #define KSDIY_PIN_NUM_TOUCH_IIC_SDA 17
 
 /** LCD 水平分辨率（像素） */
 #define EXAMPLE_LCD_H_RES 280
 
 /** LCD 垂直分辨率（像素） */
 #define EXAMPLE_LCD_V_RES 240
 
 /** LCD 命令位数 */
 #define EXAMPLE_LCD_CMD_BITS 8
 
 /** LCD 参数位数 */
 #define EXAMPLE_LCD_PARAM_BITS 8
 
 /*******************************************************************************
  * LVGL 任务配置
  ******************************************************************************/
 
 /** LVGL 时钟节拍周期（毫秒） */
 #define EXAMPLE_LVGL_TICK_PERIOD_MS 10
 
 /** LVGL 任务最大延迟时间（毫秒） */
 #define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
 
 /** LVGL 任务最小延迟时间（毫秒） */
 #define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
 
 /** LVGL 任务栈大小（字节） */
 #define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)
 
 /** LVGL 任务优先级 */
 #define EXAMPLE_LVGL_TASK_PRIORITY 10
 
 /** LVGL 显示设备句柄 */
 static lv_disp_t *lvgl_disp = NULL;
 
 /** 启用触摸屏功能 */
 #define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 1
 
 /** LVGL 互斥锁句柄（已弃用，改用 esp_lv_adapter 内部锁） */
 static SemaphoreHandle_t lvgl_mux = NULL;
 
 /** 触摸屏 I2C 总线句柄 */
 i2c_master_bus_handle_t touch_i2c_bus_;
 
 #if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
 #include "esp_lcd_touch_cst816s.h"
 
 /** CST816S 触摸屏是否找到标志 */
 bool cat816s_found = false;
 
 /** 触摸屏设备句柄 */
 esp_lcd_touch_handle_t tp = NULL;
 
 /**
  * @brief 在 I2C 总线上扫描并查找 CST816S 触摸屏
  *
  * 该函数会扫描 I2C 总线上所有可能的从机地址（0x01-0x7E），
  * 查找 CST816S 触摸屏（I2C 地址为 0x15）。
  *
  * @return
  *      - ESP_OK: 成功找到 CST816S 触摸屏
  *      - ESP_FAIL: 未找到 CST816S 触摸屏
  */
 esp_err_t find_cst816s(void)
 {
     esp_err_t ret = ESP_OK;
 
     // 遍历所有可能的 I2C 从机地址
     for (uint8_t i = 0x01; i < 0x7F; i++)
     {
         ret = i2c_master_probe(touch_i2c_bus_, i, -1);
         if (ret == ESP_OK)
         {
             printf("The slave has been found, the address is %x\n", i);
             // CST816S 的 I2C 地址为 0x15
             if (i == 0X15)
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
  * 配置并初始化用于触摸屏和 IO 扩展芯片的 I2C 总线。
  * 使用 I2C_NUM_1 端口，启用内部上拉电阻。
  * 初始化完成后会自动扫描 CST816S 触摸屏。
  */
 void i2c_init(void)
 {
     // I2C 总线配置
     i2c_master_bus_config_t i2c_bus_cfg = {
         .i2c_port = I2C_NUM_0,                    // 使用 I2C 端口 1
         .sda_io_num = KSDIY_PIN_NUM_TOUCH_IIC_SDA, // SDA 引脚
         .scl_io_num = KSDIY_PIN_NUM_TOUCH_IIC_SCL, // SCL 引脚
         .clk_source = I2C_CLK_SRC_DEFAULT,        // 使用默认时钟源
         .glitch_ignore_cnt = 7,                   // 毛刺过滤计数
         .intr_priority = 0,                       // 中断优先级（0 表示默认）
         .trans_queue_depth = 0,                   // 传输队列深度（0 表示同步模式）
         .flags = {
             .enable_internal_pullup = 1,          // 启用内部上拉电阻
         },
     };
 
     // 创建 I2C 主机总线
     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus_));
 
     // 扫描并查找 CST816S 触摸屏
     find_cst816s();
 }
 #endif
 
 /**
  * @brief 获取 LVGL 互斥锁
  *
  * LVGL 不是线程安全的，所有 LVGL API 调用必须在持有锁的情况下进行。
  * 该函数用于在多任务环境中保护 LVGL 操作。
  *
  * @param timeout_ms 等待超时时间（毫秒）
  * @return
  *      - true: 成功获取锁
  *      - false: 获取锁超时
  */
 bool ksdiy_lvgl_lock(int timeout_ms)
 {
     return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
 }
 
 /**
  * @brief 释放 LVGL 互斥锁
  *
  * 在完成 LVGL API 调用后，必须调用此函数释放锁，
  * 以允许其他任务访问 LVGL。
  */
 void ksdiy_lvgl_unlock(void)
 {
     esp_lv_adapter_unlock();
 }
 
 /**
  * @brief KSDIY LVGL 移植层初始化
  *
  * 该函数完成以下初始化工作：
  * 1. 初始化 I2C 总线（用于触摸屏和 IO 扩展芯片）
  * 2. 初始化 IO 扩展芯片（TCA9554/TCA9554A）
  * 3. 配置 LCD 背光（如果使用）
  * 4. 初始化 SPI 总线
  * 5. 安装 ST7789 LCD 驱动
  * 6. 初始化 LVGL 适配器
  * 7. 注册 LCD 显示设备到 LVGL
  * 8. 初始化触摸屏并注册到 LVGL（如果启用）
  * 9. 启动 LVGL 适配器任务
  */
 void ksdiy_lvgl_port_init(void)
 {
     // 步骤1：初始化 I2C 总线
     i2c_init();
 
     esp_io_expander_handle_t io_expander = NULL;
 
     // 步骤2：初始化 IO 扩展芯片
     // 先尝试 TCA9554，如果失败则尝试 TCA9554A
     if ((esp_io_expander_new_i2c_tca9554(touch_i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554, &io_expander) != ESP_OK))
     {
         // TCA9554 初始化失败，尝试 TCA9554A
         if (esp_io_expander_new_i2c_tca9554(touch_i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A, &io_expander) != ESP_OK)
         {
             ESP_LOGE(TAG, "Failed to initialize IO expander");
         }
         else
         {
             // TCA9554A 初始化成功，配置 PIN1 和 PIN2 为输出并置高
             // 这些引脚通常用于控制 LCD 复位和背光
             esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
             esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1);
             ESP_LOGI(TAG, "Initialize IO expander TCA9554A OK");
         }
     }
     else
     {
         // TCA9554 初始化成功，配置 PIN1 和 PIN2 为输出并置高
         esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
         esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1);
         ESP_LOGI(TAG, "Initialize IO expander TCA9554 OK");
     }
 
     // 步骤3：配置 LCD 背光引脚（如果定义了背光引脚）
     if (KSDIY_PIN_NUM_BK_LIGHT != -1)
     {
         ESP_LOGI(TAG, "Turn off LCD backlight");
         gpio_config_t bk_gpio_config = {
             .mode = GPIO_MODE_OUTPUT,
             .pin_bit_mask = 1ULL << KSDIY_PIN_NUM_BK_LIGHT};
         ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
     }
 
     // 步骤4：初始化 SPI 总线
     ESP_LOGI(TAG, "Initialize SPI bus");
     spi_bus_config_t buscfg = {
         .sclk_io_num = KSDIY_PIN_NUM_SCLK,        // SPI 时钟引脚
         .mosi_io_num = KSDIY_PIN_NUM_MOSI,        // SPI MOSI 引脚
         .miso_io_num = KSDIY_PIN_NUM_MISO,        // SPI MISO 引脚（未使用）
         .quadwp_io_num = -1,                      // 四线 SPI WP 引脚（未使用）
         .quadhd_io_num = -1,                      // 四线 SPI HD 引脚（未使用）
         .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t), // 最大传输大小
     };
     ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
 
     // 步骤5：安装 LCD 面板 IO
     ESP_LOGI(TAG, "Install panel IO");
     esp_lcd_panel_io_handle_t io_handle = NULL;
     esp_lcd_panel_io_spi_config_t io_config = {
         .dc_gpio_num = KSDIY_PIN_NUM_LCD_DC,      // 数据/命令选择引脚
         .cs_gpio_num = KSDIY_PIN_NUM_LCD_CS,      // 片选引脚
         .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,   // SPI 时钟频率
         .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,    // 命令位数
         .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS, // 参数位数
         .spi_mode = 0,                           // SPI 模式 0
         .trans_queue_depth = 10,                 // 传输队列深度
     };
 
     // 将 LCD 连接到 SPI 总线
     ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
 
     // 步骤6：安装 ST7789 LCD 驱动
     esp_lcd_panel_handle_t panel_handle = NULL;
     esp_lcd_panel_dev_config_t panel_config = {
         .reset_gpio_num = KSDIY_PIN_NUM_LCD_RST,  // 复位引脚
         .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // RGB 像素顺序
         .bits_per_pixel = 16,                    // 每像素位数（RGB565）
     };
 
     ESP_LOGI(TAG, "Install ST7789 panel driver");
     ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
 
     // 配置 LCD 面板参数
     ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));      // 复位面板
     ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));       // 初始化面板
     ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); // 交换 X/Y 轴（横屏显示）
     ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true)); // Y 轴镜像
     ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));  // 颜色反转
     ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));   // 开启显示
 
     // 开启背光（如果定义了背光引脚）
     if (KSDIY_PIN_NUM_BK_LIGHT != -1)
     {
         ESP_LOGI(TAG, "Turn on LCD backlight");
         gpio_set_level(KSDIY_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
     }
 
     // 步骤7：初始化 LVGL 适配器
     ESP_LOGI(TAG, "Initialize LVGL adapter");
     const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
     ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));
 
     // 步骤8：注册 LCD 显示设备到 LVGL
     ESP_LOGI(TAG, "Register LCD display to LVGL");
 
     esp_lv_adapter_display_config_t disp_cfg = {
         .panel = panel_handle,                   // LCD 面板句柄
         .panel_io = io_handle,                   // LCD IO 句柄
         .profile = {
             .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER, // 接口类型
             .rotation = ESP_LV_ADAPTER_ROTATE_0,        // 旋转角度
             .hor_res = EXAMPLE_LCD_H_RES,               // 水平分辨率
             .ver_res = EXAMPLE_LCD_V_RES,               // 垂直分辨率
             .buffer_height = EXAMPLE_LCD_H_RES * 20,    // 缓冲区高度（行数）
             .use_psram = false,                         // 不使用 PSRAM
             .enable_ppa_accel = false,                  // 不启用 PPA 加速
             .require_double_buffer = true,              // 使用双缓冲
         },
         .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE, // 撕裂避免模式
     };
 
     // 注册显示设备
     lvgl_disp = esp_lv_adapter_register_display(&disp_cfg);
     if (lvgl_disp == NULL)
     {
         ESP_LOGE(TAG, "Failed to register display");
         return;
     }
 
     // 设置显示偏移（某些 LCD 需要偏移以正确显示）
     // 这里 X 方向偏移 20 像素，用于适配 240x280 分辨率的 LCD
     lv_display_set_offset(lvgl_disp, 20, 0);
 
 #if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
     // 步骤9：初始化触摸屏（如果找到 CST816S）
     if (cat816s_found == true)
     {
         /************* 初始化触摸屏 **************/
         esp_lcd_panel_io_handle_t tp_io_handle = NULL;
 
         // 触摸屏 I2C IO 配置
         esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
         tp_io_config.scl_speed_hz = 400000; // I2C 时钟频率 400kHz
 
         // 触摸屏配置
         esp_lcd_touch_config_t tp_cfg = {
             .x_max = EXAMPLE_LCD_H_RES,  // X 轴最大值
             .y_max = EXAMPLE_LCD_V_RES,  // Y 轴最大值
             .rst_gpio_num = -1,          // 复位引脚（未使用）
             .int_gpio_num = -1,          // 中断引脚（未使用）
             .flags = {
                 .swap_xy = 1,            // 交换 X/Y 轴（与 LCD 方向匹配）
                 .mirror_x = 0,           // X 轴不镜像
                 .mirror_y = 1,           // Y 轴镜像（与 LCD 方向匹配）
             },
         };
 
         // 根据 ESP-IDF 版本选择不同的 I2C 初始化函数
 #if ((ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR >= 4) || ESP_IDF_VERSION_MAJOR > 5)
         esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_handle_t)touch_i2c_bus_, &tp_io_config, &tp_io_handle);
 #else
         esp_lcd_new_panel_io_i2c_v2((esp_lcd_i2c_bus_handle_t)touch_i2c_bus_, &tp_io_config, &tp_io_handle);
 #endif
 
         // 创建 CST816S 触摸屏驱动
         ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp));
 
         // 将触摸输入设备注册到 LVGL
         if (tp != NULL)
         {
             ESP_LOGI(TAG, "Register touch input to LVGL");
             const esp_lv_adapter_touch_config_t touch_cfg = {
                 .disp = lvgl_disp,        // 关联的显示设备
                 .handle = tp,             // 触摸屏句柄
                 .scale = {
                     .x = 1.0f,            // X 轴缩放比例
                     .y = 1.0f             // Y 轴缩放比例
                 }
             };
 
             // 注册触摸输入设备
             lv_indev_t *touch_indev = esp_lv_adapter_register_touch(&touch_cfg);
             if (touch_indev == NULL)
             {
                 ESP_LOGE(TAG, "Failed to register touch input");
             }
             ESP_LOGI(TAG, "LVGL端口初始化完成");
         }
     }
 #endif
 
     // 步骤10：启动 LVGL 适配器任务
     // 该任务会周期性调用 lv_timer_handler() 处理 LVGL 事件
     esp_lv_adapter_start();
 }
 