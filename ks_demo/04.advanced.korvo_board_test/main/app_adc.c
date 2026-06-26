/**
 * @file app_adc.c
 * @brief ADC 按键驱动模块
 *
 * 本模块实现基于 ADC 的多路按键检测功能，用于 KSDIY Korvo 开发板。
 * 通过 iot_button 组件管理 6 个 ADC 按键，支持按下、释放、长按等事件回调。
 *
 * 硬件连接:
 *   - ADC 按键通过分压电阻网络连接到 ADC 输入通道
 *   - 6 个按键分别对应不同的 ADC 电压阈值
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

#include "app_adc.h"
#include "iot_button.h"

#define TAG "APP_ADC"

#define BUTTON_ACTIVE_LEVEL 0
#define BUTTON_NUM 6

static button_handle_t g_btns[BUTTON_NUM] = {0};
uint8_t Button_Value = 0;
char button_str[128] = {0};
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

static void button_press_down_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: PRESS_DOWN", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: PRESS_DOWN", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

static void button_press_up_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: PRESS_UP", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: PRESS_UP", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

static void button_press_repeat_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: PRESS_REPEAT[%d]", get_btn_index((button_handle_t)arg), iot_button_get_repeat((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: PRESS_REPEAT[%d]", get_btn_index((button_handle_t)arg) + 1, iot_button_get_repeat((button_handle_t)arg));
    Button_Value = 1;
}

static void button_single_click_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: SINGLE_CLICK", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: SINGLE_CLICK", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

static void button_double_click_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: DOUBLE_CLICK", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: DOUBLE_CLICK", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

static void button_long_press_start_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: LONG_PRESS_START", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: LONG_PRESS_START", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

static void button_long_press_hold_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: LONG_PRESS_HOLD", get_btn_index((button_handle_t)arg));
    memset(button_str, 0, sizeof(button_str));
    sprintf(button_str, "BTN%d: LONG_PRESS_HOLD", get_btn_index((button_handle_t)arg) + 1);
    Button_Value = 1;
}

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

void ADC_BUTTON_Init(void)
{
    printf("Button Init!\n");
    /*
            {0, 280, 600}, //1键  2.25v     315
            {1, 780, 1000}, //2键  1.12v     820
            {2, 1100, 1400}, //3键  1.689v  1290
            {3, 1500, 2100}, //4键  0.562v  1460
            {4, 2200, 2600}, //5键  2.81v   2480
            {5, 2800, 3200}, //5键  2.81v   2480
            */
    const uint16_t vol[BUTTON_NUM] = {380, 820, 1180, 1570, 1980, 2410};
    button_config_t cfg = {0};
    cfg.type = BUTTON_TYPE_ADC;
    for (size_t i = 0; i < BUTTON_NUM; i++)
    {
        cfg.adc_button_config.adc_channel = ADC_CHANNEL_4,
        cfg.adc_button_config.button_index = i;
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

        g_btns[i] = iot_button_create(&cfg);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_DOWN, (void *)button_press_down_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_UP, (void *)button_press_up_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT, (void *)(void *)button_press_repeat_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_SINGLE_CLICK, (void *)button_single_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_DOUBLE_CLICK, (void *)button_double_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_START, (void *)button_long_press_start_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_HOLD, (void *)button_long_press_hold_cb, NULL);
    }
}