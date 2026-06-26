# 02.beginner.i2s_codec

## 这个工程能学什么

这个例子演示 I2S 和板载音频 Codec 的基础初始化与收发。

## 运行效果

- 初始化 I2C 控制总线和 I2S 音频通道
- 配置输入/输出 Codec
- 串口输出音频设备初始化状态
- 屏幕显示当前音频测试阶段

## 关键文件

- `main/i2s_std_example_main.c`

## 重点理解

- I2C 控制 Codec，I2S 传输音频数据
- 输入设备和输出设备的初始化是分开的
- 音频工程通常比 GPIO/I2C 工程多一层“数据通道 + 控制通道”结构

## 建议先改什么

- 改采样率
- 看清各个 GPIO 分别承担什么角色
- 只保留播放或录音一条链先读懂

## 学完后建议看

- `03.development.audio_record_play`
- `03.development.audio_fft`
