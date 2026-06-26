# 02.beginner.wifi_station

## 这个工程能学什么

这是最标准的 Wi-Fi 入门例子：让设备作为客户端连接到路由器。

## 运行效果

- 设备连接指定路由器
- 串口打印连接、断开、重试和获取 IP 的过程
- 成功后可以作为其他联网工程的基础

## 关键文件

- `main/station_example_main.c`

## 重点理解

- Wi-Fi 初始化
- 事件循环和重连逻辑
- 什么时候算“真正联网成功”

## 建议先改什么

- 换成你自己的 SSID 和密码
- 打印 IP 地址
- 增加失败重试次数提示

## 学完后建议看

- `02.beginner.http_request`
- `02.beginner.mqtt_tcp`
