# 01.basic.adc_continuous

## 这个工程能学什么

这个例子演示 ESP32-S3 的 ADC 连续采样模式。

和 `01.basic.adc_oneshot` 不同，这里不是“读一次值”，而是让 ADC 以固定频率持续采样，再把一批结果交给任务处理，适合波形采集、流式传感器数据和音频前端一类场景。

## 运行后你会看到什么

- 串口持续输出 GPIO5 的原始 ADC 数值
- 屏幕实时显示当前采样值
- 任务通过通知机制等待 ADC 转换完成事件

屏幕标题会显示：

- `01.basic.adc_continuous`
- `GPIO5 live adc`

## 关键文件

- `main/continuous_read_main.c`

## 代码做了什么

这个工程的主流程是：

1. 初始化共享显示界面
2. 创建 ADC continuous handle
3. 配置采样频率和采样通道
4. 注册转换完成回调
5. 启动连续采样
6. 主任务阻塞等待通知
7. 读取一批原始数据并解析
8. 把结果打印到串口并更新到屏幕

## 建议重点看哪几段

- `continuous_adc_init()`
  负责 ADC continuous 的核心配置

- `s_conv_done_cb()`
  中断回调里只负责通知任务，不做重活

- `adc_continuous_read()`
  从驱动缓冲区拿回一批数据

- `adc_continuous_parse_data()`
  把原始结果块解析成可读的采样数据

## 你会学到的知识点

- ADC 连续采样和单次采样的区别
- 回调通知任务的常见写法
- 流式数据为什么要“成批读取”
- DMA/缓冲区思路在外设驱动里的基本形态

## 第一次建议你改什么

- 把采样频率改低一点，方便观察
- 只保留一个通道，先把流程看顺
- 减少串口打印频率，避免日志刷太快

## 学完后建议看

- `01.basic.adc_oneshot`
- `01.basic.freertos`
- `03.development.audio_fft`
