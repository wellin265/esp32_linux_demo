# LCD — ST7789 显示驱动

## 硬件参数

| 参数 | 值 |
|------|-----|
| 驱动芯片 | ST7789 |
| 分辨率 | 280×240 |
| 色深 | 16bit RGB565 |
| 接口 | SPI (4 线) |

## 引脚连接

| 信号 | GPIO / 控制源 | 说明 |
|------|--------------|------|
| SCLK | GPIO 1 | SPI 时钟 |
| MOSI | GPIO 0 | SPI 数据 |
| DC | GPIO 2 | 数据/命令选择 |
| CS | GPIO 46 | SPI 驱动自动控制 |
| 供电 | TCA9554 P1 | 模组供电使能 |
| 复位 | TCA9554 P2 | 硬复位 + 软复位配合 |

## 复位方式

1. **硬复位**: `main.cpp` 中翻转 TCA9554 P2 (LOW→200ms→HIGH)，将 LCD 控制器恢复到已知状态
2. **软复位**: `esp_lcd_panel_reset()` 发送软件复位命令 (因为 `reset_gpio_num = GPIO_NUM_NC`)

## 显示方向 (MADCTL)

| 方向 | width | height | swap_xy | mirror_x | mirror_y |
|------|-------|--------|---------|----------|----------|
| 横屏 (默认) | 280 | 240 | true | false | true |
| 竖屏 | 240 | 280 | false | true | false |

触摸坐标系需对应调整 (见 `CST816S/README.md`)。

## 关键 API

| 函数 | 说明 |
|------|------|
| `Lcd::init(cfg)` | SPI 总线 → ST7789 面板 → 复位 → 开显示 → 清屏 |
| `Lcd::clear(color)` | 填充全屏 |
| `Lcd::fill(sx,sy,ex,ey,color)` | 填充矩形区域 |
| `Lcd::drawPoint(x,y,color)` | 画点 |
| `Lcd::drawLine(x1,y1,x2,y2,color)` | 画线 (Bresenham) |
| `Lcd::showChar(x,y,chr,size,mode,color)` | 显示字符 (12/16/24/32 号) |
| `Lcd::showString(x,y,w,h,size,str,color)` | 显示字符串 |
| `Lcd::displayDir(dir)` | 切换横/竖屏 |
