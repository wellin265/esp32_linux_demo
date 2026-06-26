# CST816S — 电容触摸驱动

## 硬件参数

| 参数 | 值 |
|------|-----|
| 芯片 | CST816S |
| 接口 | I2C |
| 地址 | 0x15 |
| 触摸点数 | 1 |

## I2C 配置

| 参数 | 值 | 说明 |
|------|-----|------|
| 时钟 | 100kHz | |
| 控制阶段 | 1 字节 + 禁用 | CST816S I2C 协议特性 |

## 坐标系匹配

触摸 `flags` 必须与 LCD 方向一致，否则坐标 X/Y 错乱：

| LCD 方向 | swap_xy | mirror_x | mirror_y |
|----------|---------|----------|----------|
| 横屏 (280×240) | 1 | 0 | 1 |
| 竖屏 (240×280) | 0 | 0 | 0 |

## 数据读取流程

1. `readData()` — 通过 I2C 从 0x02 寄存器读取触摸数据
2. `getPoint(&x, &y, &num)` — 提取坐标，**注意手动从 `esp_lcd_touch_point_data_t` 拷贝 x/y**

```cpp
// ⚠️ esp_lcd_touch_get_data() 只填充 data 结构体,
//    必须手动提取 data.x 和 data.y 到输出参数
esp_err_t Cst816s::getPoint(uint16_t *x, uint16_t *y, uint8_t *num) {
    esp_lcd_touch_point_data_t data;
    esp_err_t ret = esp_lcd_touch_get_data(tp_handle, &data, num, 1);
    if (ret == ESP_OK && *num > 0) {
        *x = data.x;
        *y = data.y;
    }
    return ret;
}
```

## 轮询任务

`touchTask` (优先级 3): 每 50ms 调用 `readData()` → `getPoint()`, 将坐标写入全局变量供 `lcdDemoTask` 绘制。
