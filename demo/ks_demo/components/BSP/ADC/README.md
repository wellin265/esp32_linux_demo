# ADC

ESP32-P4 ADC 驱动，支持单次采样（OneShot）和连续采样（Continuous）两种模式，含自动校准。

## 使用

```cpp
// 单次采样
driver::AdcOneShot adc(ADC_UNIT_2, ADC_CHANNEL_1, ADC_ATTEN_DB_12);
adc.init();
int raw = adc.read_raw();
float v = adc.read_voltage();

// 连续采样（DMA 模式）
driver::AdcContinuous adc(ADC_UNIT_2, ADC_CHANNEL_1, ADC_ATTEN_DB_12);
adc.init();
adc.start();
adc.fetch();
int raw = adc.read_raw();
uint32_t avg = adc.get_avg_raw();
```

## 配置

`AdcContinuous` 构造函数额外参数：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `sample_freq_hz` | `uint32_t` | `20000` | 采样频率 (Hz) |
| `conv_frame_size` | `uint32_t` | `256` | 单帧转换次数 |
| `max_store_buf_size` | `uint32_t` | `1024` | 最大缓存大小 |

## 方法

### AdcBase（公共接口）

| 方法 | 返回 | 说明 |
|------|------|------|
| `init()` | `esp_err_t` | 初始化 ADC |
| `read_raw()` | `int` | 读取原始值 |
| `read_voltage()` | `float` | 读取电压 (V) |

### AdcContinuous

| 方法 | 返回 | 说明 |
|------|------|------|
| `start()` | `esp_err_t` | 启动连续采样 |
| `stop()` | `esp_err_t` | 停止连续采样 |
| `fetch(timeout_ms)` | `esp_err_t` | 获取采样数据 |
| `get_avg_raw()` | `uint32_t` | 平均原始值 |
| `get_min_raw()` | `uint32_t` | 最小原始值 |
| `get_max_raw()` | `uint32_t` | 最大原始值 |
| `get_sample_count()` | `uint32_t` | 采样计数 |
