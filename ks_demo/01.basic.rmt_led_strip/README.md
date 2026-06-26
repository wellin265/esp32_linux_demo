# 01.basic.rmt_led_strip

## 这个工程能学什么

这个例子演示如何通过 RMT 驱动 WS2812 一类地址灯带。

它比普通 `blink` 更进一步，因为灯带不是简单 GPIO 翻转，而是要按特定时序把颜色数据编码后发出去。

## 运行后你会看到什么

- 灯带颜色循环变化
- 串口输出初始化和传输相关日志
- 屏幕提示工程已经启动

## 关键文件

- `main/led_strip_example_main.c`
- `main/led_strip_encoder.c`
- `main/led_strip_encoder.h`

## 代码做了什么

主流程是：

1. 初始化显示
2. 创建 RMT TX channel
3. 创建灯带编码器
4. 计算每个像素的 RGB 数据
5. 调用 `rmt_transmit()` 发送灯带缓冲区
6. 循环更新颜色

## 建议重点看哪几段

- `rmt_new_tx_channel()`
- `rmt_new_led_strip_encoder()`
- `led_strip_hsv2rgb()`
- `rmt_transmit()`

## 你会学到的知识点

- 地址灯带为什么需要精确时序
- RMT 在“非传统串口协议”里的价值
- 颜色数据为什么经常先在内存里组织好再一起发送

## 第一次建议你改什么

- 改灯带数量
- 改颜色变化速度
- 只点亮第一个像素验证连线

## 学完后建议看

- `01.basic.blink`
- `01.basic.ledc_pwm`
