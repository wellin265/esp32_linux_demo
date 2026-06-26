# ESP32-S3 LCD + Touch 测试项目

## 硬件资源

| 外设 | 型号 | 接口 | 说明 |
|------|------|------|------|
| LCD | ST7789 | SPI (SPI2_HOST) | 280×240 像素，16bit RGB565 |
| 触摸 | CST816S | I2C (I2C_NUM_0, 0x15) | 电容触摸 |
| IO 扩展 | TCA9554 | I2C (I2C_NUM_0, 0x20) | 8-bit GPIO 扩展 |

## 引脚分配

### SPI (LCD)
| 引脚 | GPIO | 说明 |
|------|------|------|
| SCLK | 1 | SPI 时钟 |
| MOSI | 0 | SPI 数据 |
| MISO | NC | 未使用 |
| DC | 2 | 数据/命令选择 |
| CS | 46 | 片选 (直连 GPIO) |

### I2C (TCA9554 + CST816S 共用)
| 引脚 | GPIO | 说明 |
|------|------|------|
| SDA | 17 | I2C 数据 |
| SCL | 18 | I2C 时钟 |

### TCA9554 IO 扩展引脚
| 引脚 | 功能 | 说明 |
|------|------|------|
| P1 | **LCD 供电使能** | ⚠️ 不是背光！拉低会导致 LCD 断电黑屏 |
| P2 | LCD 复位/使能 | 保持 HIGH，不需要翻转 |

> **关键：** P1 是 LCD 模组供电控制，不是背光开关。初始化时必须设为 HIGH，否则 LCD 没电、SPI 通信看似成功但屏幕不亮。参考项目中 P1+P2 一起设为 HIGH，不区分配置。

## 目录结构

```
ks_demo/
├── main/
│   ├── main.cpp                    # 入口: 初始化外设 → 启动 FreeRTOS 任务
│   ├── CMakeLists.txt
│   └── APP/FREERTOS_DEMO/
│       ├── freertos_demo.hpp
│       └── freertos_demo.cpp       # 测试任务: 系统信息 + LCD 图形 + 触摸轮询
├── components/
│   ├── BSP/
│   │   ├── MYIIC/                  # I2C 总线驱动 (GPIO 17/18, 400kHz)
│   │   ├── MYSPI/                  # SPI 总线驱动 (GPIO 0/1, SPI2_HOST)
│   │   ├── LCD/                    # ST7789 LCD 驱动 (280×240, 16bit RGB565)
│   │   ├── CST816S/                # CST816S 触摸驱动 (I2C 0x15)
│   │   └── TCA9554/               # TCA9554 IO 扩展驱动 (I2C 0x20)
│   └── CMakeLists.txt
├── managed_components/             # ESP-IDF 托管组件
│   ├── espressif__esp_lcd_touch
│   ├── espressif__esp_lcd_touch_cst816s
│   ├── espressif__esp_io_expander
│   └── espressif__esp_io_expander_tca9554
├── CMakeLists.txt
├── sdkconfig
└── partitions-16MiB.csv
```

## 初始化流程

1. NVS 初始化
2. **TCA9554 初始化** → P1+P2 立即设为 HIGH（LCD 供电 + 使能）
3. **LCD 初始化** → SPI 总线 → ST7789 面板 → 软件复位 → 开显示 → 清屏
4. **CST816S 初始化** → I2C 触摸控制器
5. **启动 FreeRTOS 任务** → 系统信息 / LCD 演示 / 触摸轮询

## 关键注意事项

1. **TCA9554 P1 = LCD 供电，不是背光。** 初始化时必须 HIGH，否则屏幕不亮。
2. **IO Expander 地址是 0x20 (TCA9554)，不是 0x38 (TCA9554A)。** 0x20 有设备响应但不同批次可能不同，代码会先试 0x38 再试 0x20。
3. **不要翻转 P2 做硬件复位。** LCD 用 `esp_lcd_panel_reset()` 软件复位即可，翻转 P2 可能导致状态异常。
4. **触摸坐标系需与 LCD 方向匹配。** 横屏时 LCD 用 `swap_xy=true, mirror_y=true`，触摸也要设 `swap_xy=1, mirror_y=1`。
