# 01.basic.blink

## 这个工程能学什么

这是最经典的第二个 ESP-IDF 工程。

它演示如何让一个 LED 按固定周期闪烁，同时把状态显示到屏幕上。这个例子既支持普通 GPIO LED，也兼容 addressable LED strip 配置。

## 运行后你会看到什么

- LED 按固定周期亮灭
- 串口输出运行日志
- 屏幕显示当前 LED 类型、亮灭状态和闪烁次数

## 关键文件

- `main/blink_example_main.c`

## 代码做了什么

主流程是：

1. 初始化显示界面
2. 根据配置选择普通 GPIO LED 或灯带 LED
3. 循环切换 LED 状态
4. 更新屏幕上的状态信息
5. 按配置周期延时

## 建议重点看哪几段

- `configure_led()`
- `blink_led()`
- `update_display()`
- `CONFIG_BLINK_PERIOD`

## 你会学到的知识点

- GPIO 输出的最基本用法
- `menuconfig` 配置如何影响代码行为
- 周期性任务最简单的写法

## 第一次建议你改什么

- 把闪烁周期改成 200ms 或 1000ms
- 修改亮灭时的屏幕显示文案
- 观察普通 GPIO LED 和 addressable LED 的差别

## 学完后建议看

- `01.basic.gpio`
- `01.basic.ledc_pwm`
