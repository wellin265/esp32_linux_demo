# 01.basic.ledc_pwm

## 这个工程能学什么

这个例子演示用 LEDC 产生 PWM 输出，并做占空比扫动。

适合你在学完 GPIO 输出以后，进一步理解“不是只有开和关，还可以控制强度和比例”。

## 运行后你会看到什么

- GPIO19 输出 4kHz PWM
- 占空比在不同百分比之间变化
- 屏幕显示当前 PWM 频率、GPIO 和 duty 百分比

## 关键文件

- `main/ledc_basic_example_main.c`

## 代码做了什么

主流程是：

1. 初始化显示
2. 配置 LEDC timer
3. 配置 LEDC channel
4. 循环修改占空比
5. 更新屏幕显示当前 duty

## 建议重点看哪几段

- `example_ledc_init()`
- `ledc_timer_config_t`
- `ledc_channel_config_t`
- `ledc_set_duty()`
- `ledc_update_duty()`

## 你会学到的知识点

- PWM 的频率和占空比是什么
- LEDC timer 和 channel 的关系
- 为什么很多调光、调速都靠 PWM

## 第一次建议你改什么

- 改 PWM 频率
- 改输出 GPIO
- 把占空比扫动步进调小

## 学完后建议看

- `01.basic.blink`
- `01.basic.rmt_led_strip`
