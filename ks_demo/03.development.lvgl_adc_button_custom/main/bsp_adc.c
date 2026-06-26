/**
 * @file bsp_adc.c
 * @brief ADC 按键驱动实现
 *
 * 本文件实现通过 ADC 读取多个按键的功能：
 * - 使用电阻分压网络区分不同按键
 * - 支持 6 个 ADC 按键
 * - 支持校准功能提高精度
 *
 * 按键原理：
 * 多个按键通过不同的电阻分压，按下时产生不同的电压值，
 * ADC 读取电压值后通过查表判断是哪个按键被按下。
 *
 * @author Kevincoooool
 * @date 2021-09-04
 */

#include "bsp_adc.h"

/** @brief 日志标签 */
#define TAG "BSP_ADC"

/** @brief ADC 默认参考电压 (mV) */
#define DEFAULT_VREF 3300

/** @brief 多次采样次数 */
#define NO_OF_SAMPLES 32

/* ===== ADC 配置 ===== */
static const adc_channel_t channel = ADC_CHANNEL_4;  /**< ADC 通道 4（GPIO1） */
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;  /**< 12 位分辨率 */
static const adc_atten_t atten = ADC_ATTEN_DB_11;  /**< 11dB 衰减（测量范围 0-3.3V） */

/** @brief ADC 校准特性结构体 */
static esp_adc_cal_characteristics_t adc_chars;

/** @brief ADC 衰减配置 */
#define ADC_EXAMPLE_ATTEN ADC_ATTEN_DB_11

/* ===== ADC 校准方案配置（根据芯片类型） ===== */
#if CONFIG_IDF_TARGET_ESP32
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif

/** @brief 校准使能标志 */
bool cali_enable = false;

/** @brief 电压值（mV） */
uint16_t voltage = 0;

/** @brief ADC 原始读数 */
uint16_t read_raw = 0;

/** @brief 滤波后的读数 */
float read_F = 0;

/**
 * @brief 初始化 ADC 校准
 *
 * 检查 eFuse 中的校准数据并初始化校准特性。
 *
 * @return true 校准成功，false 校准失败（仍可使用未校准值）
 */
static bool adc_calibration_init(void)
{
    esp_err_t ret;
    cali_enable = false;

    /* 检查 eFuse 中是否有校准数据 */
    ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
    if (ret == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    }
    else if (ret == ESP_ERR_INVALID_VERSION)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else if (ret == ESP_OK)
    {
        cali_enable = true;
        /* 初始化校准特性 */
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_EXAMPLE_ATTEN, ADC_WIDTH_BIT_12, 0, &adc_chars);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg");
    }

    return cali_enable;
}

/**
 * @brief 初始化 ADC
 *
 * 配置 ADC1 通道并启动转换。
 */
void adc_init()
{
    /* 初始化校准 */
    cali_enable = adc_calibration_init();

    esp_err_t r;

    /* 获取 ADC 通道对应的 GPIO 编号 */
    gpio_num_t adc_gpio_num;

    /* 配置 ADC 位宽 */
    adc1_config_width(width);

    r = adc1_pad_get_io_num(channel, &adc_gpio_num);
    assert(r == ESP_OK);

    printf("ADC1 channel %d @ GPIO %d\n", channel, adc_gpio_num);

    /* 配置通道衰减 */
    printf("adc1_init...\n");
    adc1_config_channel_atten(channel, atten);

    vTaskDelay(200 / portTICK_PERIOD_MS);

    printf("start conversion.\n");
}

/**
 * @brief 获取 ADC 原始值
 *
 * 读取 ADC 通道的原始值（12 位，0-4095）。
 *
 * @return ADC 原始值
 */
uint16_t get_adc(void)
{
    /* 读取 ADC 原始值 */
    read_raw = adc1_get_raw(channel);
    return read_raw;
}

/**
 * @brief 获取 ADC 电压值（校准后）
 *
 * 读取 ADC 值并转换为电压（mV）。
 *
 * @return ADC 原始值（同时打印校准后的电压）
 */
uint16_t get_adc_vol(void)
{
    /* 读取 ADC 原始值 */
    read_raw = adc1_get_raw(channel);

    /* 如果校准可用，转换为电压值 */
    if (cali_enable)
    {
        voltage = esp_adc_cal_raw_to_voltage(read_raw, &adc_chars);
        ESP_LOGI(TAG, "cali data: %d mV", voltage);
    }

    return read_raw;
}

/**
 * @brief ADC 按键电压范围表
 *
 * 每行定义一个按键的 ADC 值范围：
 * - [0]: 按键索引
 * - [1]: 最小 ADC 值
 * - [2]: 最大 ADC 值
 *
 * 电压计算：ADC值 = (电压 / 3.3V) * 4095
 *
 * @note 需要根据实际硬件调整阈值
 */
uint16_t touch_chart[][3] =
{
    /* 按键索引, 最小值, 最大值  对应电压 */
    {0, 280, 600},    /* 按键1: 约 0.23-0.48V */
    {1, 780, 1000},   /* 按键2: 约 0.63-0.81V */
    {2, 1100, 1400},  /* 按键3: 约 0.89-1.13V */
    {3, 1500, 2100},  /* 按键4: 约 1.21-1.69V */
    {4, 2200, 2600},  /* 按键5: 约 1.77-2.10V */
    {5, 2800, 3200},  /* 按键6: 约 2.26-2.58V */
};

/**
 * @brief 获取当前按下的 ADC 按键
 *
 * 读取 ADC 值并查表判断是哪个按键被按下。
 *
 * @return 按键编号（1-6），0 表示无按键按下
 */
uint8_t Get_Adc_Button()
{
    /* 遍历按键表，检查当前 ADC 值是否在某个按键的范围内 */
    for (uint8_t i = 0; i < 6; i++)
    {
        if (adc1_get_raw(channel) >= touch_chart[i][1] && adc1_get_raw(channel) <= touch_chart[i][2])
        {
            return i + 1;  /* 返回按键编号（1-6） */
        }
    }
    return 0;  /* 无按键按下 */
}
