# 01.basic.gptimer

## 这个工程能学什么

这个例子演示 ESP-IDF 新版 GPTimer 驱动的使用方式。

它不是单纯起一个定时器，而是连续展示了几种 alarm 配置和回调处理方式。

## 运行后你会看到什么

- 串口输出定时器 alarm 事件
- 屏幕显示 alarm 触发次数和当前计数值
- 队列负责把中断事件送到任务侧处理

## 关键文件

- `main/gptimer_example_main.c`

## 代码做了什么

主流程是：

1. 初始化显示
2. 创建事件队列
3. 创建 GPTimer
4. 注册 alarm 回调
5. 设置 alarm 动作
6. 启动计时器
7. 从队列接收回调事件并显示结果

## 建议重点看哪几段

- `gptimer_new_timer()`
- `gptimer_register_event_callbacks()`
- `gptimer_set_alarm_action()`
- `example_timer_on_alarm_cb_*()`

## 你会学到的知识点

- 定时器回调是怎么工作的
- 中断回调如何把数据送回任务
- 单次 alarm 和重复 alarm 的区别

## 第一次建议你改什么

- 改 alarm 时间
- 只保留一种 alarm 模式先看懂
- 调整屏幕显示，让它更突出计数变化

## 学完后建议看

- `01.basic.ledc_pwm`
- `01.basic.freertos`
