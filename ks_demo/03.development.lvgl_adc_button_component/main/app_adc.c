/**
 * @file app_adc.c
 * @brief ADC 按键驱动实现（使用 ESP button 组件）
 *
 * 本文件使用 ESP-IoT-Solution 的 button 组件实现 ADC 按键功能：
 * - 支持 6 个 ADC 按键
 * - 自动扫描和去抖动
 * - 支持多种按键事件回调
 *
 * 支持的按键事件：
 * - PRESS_DOWN: 按键按下
 * - PRESS_UP: 按键释放
 * - PRESS_REPEAT: 重复按下
 * - SINGLE_CLICK: 单击
 * - DOUBLE_CLICK: 双击
 * - LONG_PRESS_START: 长按开始
 * - LONG_PRESS_HOLD: 长按保持
 *
 * @author Kevincoooool
 * @date 2021-09-04
 */

#include "app_adc.h"
#include "iot_button.h"

/** @brief 日志标签 */
#define TAG "APP_ADC"

/** @brief 按键激活电平（未使用，ADC 按键不需要） */
#define BUTTON_ACTIVE_LEVEL 0

/** @brief 按键数量 */
#define BUTTON_NUM 6

/** @brief 按键句柄数组 */
static button_handle_t g_btns[BUTTON_NUM] = {0};

/** @brief 按键事件标志（用于通知主循环更新界面） */
uint8_t Button_Value = 0;

/** @brief 按键事件字符串 */
char button_str[128] = {0};

/**
 * @brief 获取按键索引
 *
 * 根据按键句柄查找在数组中的索引。
 *
 * @param btn 按键句柄
 * @return 按键索引（0-5），-1 表示未找到
 */
static int get_btn_index(button_handle_t btn)
{
    for (size_t i = 0; i < BUTTON_NUM; i++)
    {
        if (btn == g_btns[i])
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 按键按下回调函数
 *
 * @param arg 按键句柄
 */
static void button_press_down_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: PRESS_DOWN", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: PRESS_DOWN", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

/**
 * @brief 按键释放回调函数
 *
 * @param arg 按键句柄
 */
static void button_press_up_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: PRESS_UP", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: PRESS_UP", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

/**
 * @brief 按键重复按下回调函数
 *
 * @param arg 按键句柄
 */
static void button_press_repeat_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: PRESS_REPEAT[%d]", get_btn_index((button_handle_t)arg), iot_button_get_repeat((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: PRESS_REPEAT[%d]", get_btn_index((button_handle_t)arg) + 1, iot_button_get_repeat((button_handle_t)arg));
    Button_Value = 1;
}

/**
 * @brief 按键单击回调函数
 *
 * @param arg 按键句柄
 */
static void button_single_click_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: SINGLE_CLICK", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: SINGLE_CLICK", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

/**
 * @brief 按键双击回调函数
 *
 * @param arg 按键句柄
 */
static void button_double_click_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: DOUBLE_CLICK", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: DOUBLE_CLICK", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

/**
 * @brief 按键长按开始回调函数
 *
 * @param arg 按键句柄
 */
static void button_long_press_start_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: LONG_PRESS_START", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: LONG_PRESS_START", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

/**
 * @brief 按键长按保持回调函数
 *
 * @param arg 按键句柄
 */
static void button_long_press_hold_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: LONG_PRESS_HOLD", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: LONG_PRESS_HOLD", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

/**
 * @brief 打印按键事件类型
 *
 * @param btn 按键句柄
 */
static void print_button_event(button_handle_t btn)
{
    button_event_t evt = iot_button_get_event(btn);
    switch (evt)
    {
    case BUTTON_PRESS_DOWN:
        ESP_LOGI(TAG, "BUTTON_PRESS_DOWN");
        break;
    case BUTTON_PRESS_UP:
        ESP_LOGI(TAG, "BUTTON_PRESS_UP");
        break;
    case BUTTON_PRESS_REPEAT:
        ESP_LOGI(TAG, "BUTTON_PRESS_REPEAT");
        break;
    case BUTTON_SINGLE_CLICK:
        ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
        break;
    case BUTTON_DOUBLE_CLICK:
        ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK");
        break;
    case BUTTON_LONG_PRESS_START:
        ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START");
        break;
    case BUTTON_LONG_PRESS_HOLD:
        ESP_LOGI(TAG, "BUTTON_LONG_PRESS_HOLD");
        break;
    default:
        break;
    }
}

/**
 * @brief 初始化 ADC 按键
 *
 * 使用 ESP button 组件创建 6 个 ADC 按键：
 * - ADC 通道: ADC1_CHANNEL_4（GPIO1）
 * - 按键数量: 6
 * - 每个按键有不同的电压阈值范围
 *
 * 按键电压阈值表：
 * | 按键 | 中心电压值 | 范围 |
 * |------|-----------|------|
 * | BTN1 | 380       | 0-600 |
 * | BTN2 | 820       | 600-1000 |
 * | BTN3 | 1180      | 1000-1380 |
 * | BTN4 | 1570      | 1380-1775 |
 * | BTN5 | 1980      | 1775-2195 |
 * | BTN6 | 2410      | 2195-2705 |
 */
void ADC_BUTTON_Init(void)
{
    printf("Button Init!\n");

    /*
     * 按键电压范围配置
     * {0, 280, 600},    // 按键1
     * {1, 780, 1000},   // 按键2
     * {2, 1100, 1400},  // 按键3
     * {3, 1500, 2100},  // 按键4
     * {4, 2200, 2600},  // 按键5
     * {5, 2800, 3200},  // 按键6
     */

    /* 每个按键的中心电压值 */
    const uint16_t vol[BUTTON_NUM] = {380, 820, 1180, 1570, 1980, 2410};

    /* 配置按键 */
    button_config_t cfg = {0};
    cfg.type = BUTTON_TYPE_ADC;

    for (size_t i = 0; i < BUTTON_NUM; i++)
    {
        /* 配置 ADC 通道和按键索引 */
        cfg.adc_button_config.adc_channel = ADC_CHANNEL_4;
        cfg.adc_button_config.button_index = i;

        /* 计算电压阈值范围 */
        if (i == 0)
        {
            cfg.adc_button_config.min = (0 + vol[i]) / 2;
        }
        else
        {
            cfg.adc_button_config.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5)
        {
            cfg.adc_button_config.max = (vol[i] + 3000) / 2;
        }
        else
        {
            cfg.adc_button_config.max = (vol[i] + vol[i + 1]) / 2;
        }

        /* 创建按键 */
        g_btns[i] = iot_button_create(&cfg);

        /* 注册各种事件回调 */
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_DOWN, (void *)button_press_down_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_UP, (void *)button_press_up_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT, (void *)button_press_repeat_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_SINGLE_CLICK, (void *)button_single_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_DOUBLE_CLICK, (void *)button_double_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_START, (void *)button_long_press_start_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_HOLD, (void *)button_long_press_hold_cb, NULL);
    }
}
