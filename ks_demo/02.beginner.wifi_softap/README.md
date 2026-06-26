# 02.beginner.wifi_softap

## 这个工程能学什么

这个例子演示把设备变成一个 Wi-Fi 热点。

## 运行效果

- 板子启动 SoftAP
- 手机或电脑可以搜索并连接到它
- 串口输出热点启动和连接事件

## 关键文件

- `main/softap_example_main.c`

## 重点理解

- SoftAP 和 Station 是两种完全不同的角色
- 做配网、离线本地控制时 SoftAP 非常常见
- 热点参数都可以通过配置项调整

## 建议先改什么

- 改 SSID 和密码
- 打印已连接设备数量
- 为后续 HTTP 本地服务打基础

## 学完后建议看

- `02.beginner.wifi_station`
- `02.beginner.http_server`
