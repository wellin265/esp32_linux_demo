# 01.basic.freertos

## 这个工程能学什么

这是一个 FreeRTOS 基础综合例子，不是只讲一个 API，而是把任务、队列、锁、任务通知组合成串口控制台命令来演示。

适合已经会跑基础例子、准备真正理解多任务模型的人。

## 运行后你会看到什么

- 屏幕显示 `UART console ready`
- 串口控制台可输入命令
- 可通过 `help`、`create_task`、`queue`、`lock`、`notify` 等命令触发不同示例

## 关键文件

- `main/basic_freertos_smp_usage.c`
- `main/create_task_example.c`
- `main/queue_example.c`
- `main/lock_example.c`
- `main/task_notify_example.c`
- `main/batch_processing_example.c`

## 代码做了什么

这个工程不是单一循环，而是：

1. 初始化串口控制台
2. 注册一组 FreeRTOS 示例命令
3. 在你输入命令后运行对应 demo

## 建议重点看哪几段

- `app_main()`
- `register_queue()`
- `register_lock()`
- `comp_queue_entry_func()`
- `comp_lock_entry_func()`
- `ulTaskNotifyTake()` 相关示例

## 你会学到的知识点

- 任务创建
- 队列通信
- 互斥锁和自旋锁
- 任务通知
- 多核环境下的基本并发问题

## 第一次建议你怎么用

启动后先在串口里试：

- `help`
- `create_task`
- `queue`
- `lock`
- `notify`

## 学完后建议看

- `01.basic.gpio`
- `01.basic.adc_continuous`
- `02.beginner.http_server`
