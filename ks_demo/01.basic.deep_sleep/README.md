# 01.basic.deep_sleep

## 这个工程能学什么

这个例子演示 ESP32-S3 的深度睡眠和多种唤醒方式。

它比一般基础例子复杂一些，因为同时包含 RTC timer 唤醒、GPIO 唤醒、EXT0/EXT1 唤醒和 NVS 存储睡眠时间，适合已经学过基础外设以后再看。

## 运行后你会看到什么

- 屏幕提示工程已经启动
- 串口输出当前唤醒原因
- 一段时间后设备主动进入 deep sleep
- 通过定时器或外部 GPIO 事件再次唤醒

## 关键文件

- `main/deep_sleep_example_main.c`
- `main/gpio_wakeup.c`
- `main/ext_wakeup.c`

## 代码做了什么

主流程是：

1. 初始化显示
2. 配置 RTC timer 唤醒
3. 根据配置注册 GPIO / EXT0 / EXT1 唤醒源
4. 创建 `deep_sleep_task`
5. 在任务里记录睡眠前时间
6. 打印唤醒原因
7. 进入深度睡眠

## 建议重点看哪几段

- `example_deep_sleep_register_rtc_timer_wakeup()`
- `example_deep_sleep_register_gpio_wakeup()`
- `example_deep_sleep_register_ext0_wakeup()`
- `example_deep_sleep_register_ext1_wakeup()`
- `deep_sleep_task()`

## 你会学到的知识点

- deep sleep 和普通延时完全不是一回事
- 唤醒源配置是低功耗开发的核心
- 为什么有些状态要放 NVS 或 RTC 内存

## 第一次建议你改什么

- 改短 RTC 唤醒时间
- 只保留一种唤醒方式先跑通
- 观察不同唤醒原因时串口输出的变化

## 学完后建议看

- `03.development.low_power_wakeup`
- `01.basic.nvs`
