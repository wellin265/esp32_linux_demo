# 01.basic.adc_oneshot

## 这个工程能学什么

这个例子演示 ADC 单次采样。

它最适合第一次接触模数转换的人，因为流程很直接：初始化 ADC，读 GPIO5 的电压对应原始值，然后显示和打印。

## 运行后你会看到什么

- 串口周期性输出 GPIO5 的原始 ADC 值
- 屏幕显示 `GPIO5 raw xxx`
- 屏幕第三行会标注 `oneshot mode`

## 关键文件

- `main/oneshot_read_main.c`

## 代码做了什么

主流程非常简单：

1. 初始化显示界面
2. 创建 ADC oneshot 单元
3. 配置 GPIO5 对应的 ADC 通道
4. 循环读取 ADC 值
5. 把原始值和估算电压显示到屏幕

## 建议重点看哪几段

- `adc_oneshot_new_unit()`
- `adc_oneshot_config_channel()`
- `adc_oneshot_read()`
- `update_display()`

## 你会学到的知识点

- ADC 是怎么从模拟电压变成数字值的
- 为什么同一个输入会有轻微波动
- ADC 单次采样最适合哪类低速读取场景

## 第一次建议你改什么

- 调整采样周期
- 把 GPIO5 接不同电压观察变化
- 修改屏幕显示格式，让它更容易读

## 学完后建议看

- `01.basic.adc_continuous`
- `01.basic.gpio`
