# 02.beginner.http_request

## 这个工程能学什么

这个例子演示最基础的 HTTP GET 请求流程，适合第一次理解“板子主动访问网页服务器”。

## 运行效果

- 设备联网后解析 `example.com`
- 建立 socket 连接并发送 GET 请求
- 串口输出接收数据和请求结果
- 屏幕显示 DNS、连接、发送、接收等阶段

## 关键文件

- `main/http_request_example_main.c`

## 重点理解

- DNS 解析
- socket 连接
- HTTP 请求报文的最小构成
- 收到响应后如何统计数据量

## 建议先改什么

- 改访问域名或路径
- 把响应前几行打印出来
- 给失败场景补更清楚的提示

## 学完后建议看

- `02.beginner.http_server`
- `02.beginner.mqtt_tcp`
