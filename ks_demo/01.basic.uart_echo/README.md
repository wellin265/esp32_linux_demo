# 01.basic.uart_echo

## 这个工程能学什么

这个例子演示 UART 收发和回显，是最实用的基础例子之一。

几乎所有嵌入式项目都离不开串口调试，所以这个工程非常值得早一点学。

## 运行后你会看到什么

- 串口输入什么，程序就原样回显什么
- 屏幕显示 UART 端口号、TX/RX 引脚、波特率和累计字节数
- 屏幕第三行实时显示最近一次收到的文本

## 关键文件

- `main/uart_echo_example_main.c`

## 代码做了什么

主流程是：

1. 初始化显示
2. 配置 UART 参数
3. 安装 UART driver
4. 创建 `uart_echo_task`
5. 阻塞读取串口数据
6. 原样写回
7. 更新屏幕上的最新消息

## 建议重点看哪几段

- `uart_driver_install()`
- `uart_param_config()`
- `uart_set_pin()`
- `uart_read_bytes()`
- `uart_write_bytes()`
- `sanitize_uart_text()`

## 你会学到的知识点

- UART 的基本配置项
- 阻塞读取串口数据的常见写法
- 为什么显示层通常要先过滤不可打印字符

## 第一次建议你改什么

- 改波特率
- 改成收到数据后加前缀再回显
- 统计并显示总接收字节数

## 学完后建议看

- `01.basic.hello_world`
- `01.basic.freertos`
- `02.beginner.ble_spp_server`
