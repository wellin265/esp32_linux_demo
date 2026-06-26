/*
 * RMT LED 灯带示例 (RMT LED Strip Example)
 *
 * 本示例演示如何使用 RMT 外设驱动 WS2812/WS2812B 等 LED 灯带
 *
 * RMT 简介：
 * - RMT = Remote Control Transceiver，远程控制收发器
 * - 最初设计用于红外遥控
 * - 由于其精确的时序控制能力，也可用于驱动 LED 灯带
 *
 * LED 灯带（WS2812）特点：
 * - 单线数据传输
 * - 每个 LED 可独立控制 RGB 颜色
 * - 需要精确的时序控制（纳秒级）
 * - 数据格式：GRB（绿-红-蓝）
 *
 * WS2812 时序要求：
 * - 0 码：高电平 0.4μs，低电平 0.85μs
 * - 1 码：高电平 0.85μs，低电平 0.4μs
 * - 复位：低电平 > 50μs
 *
 * 本示例演示：
 * 1. 配置 RMT 发送通道
 * 2. 创建 LED 灯带编码器
 * 3. 实现 HSV 到 RGB 颜色转换
 * 4. 实现彩虹追逐效果
 *
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"      // FreeRTOS 内核
#include "freertos/task.h"          // FreeRTOS 任务
#include "esp_log.h"                // 日志系统
#include "driver/rmt_tx.h"          // RMT 发送驱动
#include "led_strip_encoder.h"      // LED 灯带编码器
#include "ksdiy_example_display.h"

/* ==================== 硬件配置 ==================== */

/*
 * RMT 分辨率
 * 10MHz = 每秒 10,000,000 个 tick
 * 1 tick = 0.1μs = 100ns
 *
 * 高分辨率对于精确控制 LED 灯带时序很重要
 */
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000  // 10MHz

/* LED 灯带连接的 GPIO */
#define RMT_LED_STRIP_GPIO_NUM      0

/* LED 数量和动画速度 */
#define EXAMPLE_LED_NUMBERS         24        // 灯带上的 LED 数量
#define EXAMPLE_CHASE_SPEED_MS      10        // 动画帧间隔（毫秒）

/* 日志标签 */
static const char *TAG = "example";

/* LED 像素数据缓冲区 */
/* 每个 LED 需要 3 字节（G, R, B） */
static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

/* ==================== HSV 转 RGB 函数 ==================== */

/**
 * @brief HSV 颜色空间转 RGB 颜色空间
 *
 * HSV 颜色空间：
 * - H (Hue): 色调，0-360°
 * - S (Saturation): 饱和度，0-100%
 * - V (Value): 明度，0-100%
 *
 * HSV 的优点：
 * - 更直观的颜色表示
 * - 容易生成彩虹效果
 * - 方便调整亮度
 *
 * @param h 色调 (0-360)
 * @param s 饱和度 (0-100)
 * @param v 明度 (0-100)
 * @param r 输出红色值 (0-255)
 * @param g 输出绿色值 (0-255)
 * @param b 输出蓝色值 (0-255)
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360;  // 确保 h 在 0-360 范围内

    /* 计算最大和最小 RGB 值 */
    uint32_t rgb_max = v * 2.55f;                      // v% 转换为 0-255
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;   // 根据饱和度计算最小值

    uint32_t i = h / 60;      // 色调区间 (0-5)
    uint32_t diff = h % 60;   // 区间内的偏移

    /* 根据色调区间计算 RGB 调整量 */
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    /*
     * 根据 H 值确定 R、G、B
     *
     * 色调环：
     * 0°   - 红色
     * 60°  - 黄色
     * 120° - 绿色
     * 180° - 青色
     * 240° - 蓝色
     * 300° - 品红
     * 360° - 红色（回到起点）
     */
    switch (i) {
    case 0:   // 0° - 60°: 红色 -> 黄色
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:   // 60° - 120°: 黄色 -> 绿色
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:   // 120° - 180°: 绿色 -> 青色
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:   // 180° - 240°: 青色 -> 蓝色
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:   // 240° - 300°: 蓝色 -> 品红
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:  // 300° - 360°: 品红 -> 红色
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

/* ==================== 主函数 ==================== */

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.rmt_led_strip", "LVGL display ready");
    ksdiy_example_display_set_lines("Example started", "Watch serial for details", "Touch is enabled");
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;   // 彩虹起始色调

    /* ==================== 第一步：创建 RMT 发送通道 ==================== */

    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;

    /*
     * RMT 发送通道配置
     *
     * - clk_src: 时钟源（使用默认）
     * - gpio_num: 输出 GPIO
     * - mem_block_symbols: 内存块大小（符号数）
     *   增大可以减少闪烁
     * - resolution_hz: 分辨率
     *   10MHz = 1 tick = 0.1μs
     * - trans_queue_depth: 传输队列深度
     *   允许后台排队的传输数量
     */
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,         // 默认时钟源
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,     // GPIO 编号
        .mem_block_symbols = 64,                // 内存块大小
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,  // 分辨率
        .trans_queue_depth = 4,                 // 队列深度
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    /* ==================== 第二步：安装 LED 灯带编码器 ==================== */

    ESP_LOGI(TAG, "Install led strip encoder");
    rmt_encoder_handle_t led_encoder = NULL;

    /*
     * LED 灯带编码器
     *
     * 编码器的作用：将 LED 像素数据转换为 RMT 脉冲序列
     *
     * WS2812 编码规则：
     * - 0 码：T0H (0.4μs) + T0L (0.85μs)
     * - 1 码：T1H (0.85μs) + T1L (0.4μs)
     */
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    /* ==================== 第三步：使能 RMT 通道 ==================== */

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    /* ==================== 第四步：彩虹追逐动画 ==================== */

    ESP_LOGI(TAG, "Start LED rainbow chase");

    /*
     * 传输配置
     * - loop_count = 0: 不循环（手动控制循环）
     */
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,  // 不自动循环
    };

    while (1) {
        /*
         * 彩虹追逐效果
         *
         * 外层循环 i (0-2): 控制相位偏移
         * 内层循环 j: 遍历所有 LED
         *
         * 每次只点亮 1/3 的 LED，产生追逐效果
         */
        for (int i = 0; i < 3; i++) {
            for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3) {
                /* 计算当前 LED 的色调 */
                hue = j * 360 / EXAMPLE_LED_NUMBERS + start_rgb;

                /* HSV 转 RGB */
                led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);

                /*
                 * 填充像素数据
                 *
                 * WS2812 数据格式是 GRB（不是 RGB！）
                 * 所以顺序是：Green, Red, Blue
                 */
                led_strip_pixels[j * 3 + 0] = green;
                led_strip_pixels[j * 3 + 1] = blue;   // 注意：有些灯带是 Blue
                led_strip_pixels[j * 3 + 2] = red;
            }

            /*
             * 发送数据到 LED 灯带
             *
             * rmt_transmit: 发送数据
             * - led_chan: RMT 通道
             * - led_encoder: 编码器
             * - led_strip_pixels: 数据缓冲区
             * - sizeof(led_strip_pixels): 数据大小
             * - &tx_config: 传输配置
             */
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));

            /* 等待传输完成 */
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));

            /* 清除所有像素（关灯） */
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
        }

        /*
         * 改变彩虹起始色调
         * 每次增加 60°，产生颜色滚动效果
         */
        start_rgb += 60;
    }
}

/*
 * ===== 常见 LED 灯带类型 =====
 *
 * WS2812 / WS2812B:
 * - 最常见，性价比高
 * - 数据格式：GRB
 * - 速度：800kHz
 *
 * WS2813:
 * - 双信号线，更稳定
 * - 断点续传
 *
 * SK6812:
 * - 类似 WS2812
 * - 有 RGBW 版本（带白光）
 *
 * APA102:
 * - 使用 SPI 接口
 * - 有独立的时钟线
 * - 速度更快
 *
 * ===== RMT 优势 =====
 *
 * 1. 精确时序
 *    - 硬件控制，不受 CPU 影响
 *    - 纳秒级精度
 *
 * 2. DMA 支持
 *    - 传输时不占用 CPU
 *    - 可以传输大量数据
 *
 * 3. 灵活配置
 *    - 可用于各种协议
 *    - 红外遥控、LED 灯带等
 *
 * ===== 调试技巧 =====
 *
 * 1. 如果 LED 不亮：
 *    - 检查 GPIO 连接
 *    - 检查电源（5V）
 *    - 检查数据格式（GRB vs RGB）
 *
 * 2. 如果颜色不对：
 *    - 调整 R/G/B 顺序
 *    - 检查亮度值
 *
 * 3. 如果 LED 闪烁：
 *    - 增加 mem_block_symbols
 *    - 检查电源是否稳定
 */
