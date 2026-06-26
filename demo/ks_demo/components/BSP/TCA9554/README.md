# TCA9554 / TCA9554A — IO 扩展驱动

## 硬件连接

| I2C 地址 | 芯片 | 说明 |
|----------|------|------|
| 0x20 | TCA9554 | 优先尝试 |
| 0x38 | TCA9554A | 备选 (不同批次可能不同) |

## 引脚分配

| 引脚 | 功能 | 方向 | 说明 |
|------|------|------|------|
| P1 | **LCD 供电使能** | OUTPUT | ⚠️ 不是背光！拉 LOW = LCD 断电 |
| P2 | LCD+Touch 硬复位 | OUTPUT | 初始化时翻转 LOW→HIGH 复位外设 |
| P3-P7 | 未使用 | — | |

## 初始化流程

1. 尝试 `esp_io_expander_new_i2c_tca9554(0x20)`
2. 失败则尝试 `0x38`
3. 成功: P1+P2 → OUTPUT → HIGH (LCD 上电，不复位)
4. 两个地址都失败: 打印警告，P1/P2 高阻态 (靠外部上拉)

> **关键**: P1 必须在 LCD 初始化前设为 HIGH，否则 LCD 模组没电，SPI 配置无效、屏幕不亮。

## main.cpp 中的复位序列

```cpp
Tca9554::inst().init();                           // P1=HIGH, P2=HIGH
Tca9554::inst().pinSetLevel(TCA9554_PIN_EN, 0);   // P2=LOW   (复位)
vTaskDelay(200ms);
Tca9554::inst().pinSetLevel(TCA9554_PIN_EN, 1);   // P2=HIGH  (释放复位)
vTaskDelay(200ms);
Lcd::inst().init(lcd_config);                      // LCD 初始化
```
