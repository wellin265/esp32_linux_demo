# 02.beginner.wifi_scan

## 这个工程能学什么

这个例子演示扫描周围 Wi-Fi 热点。

## 运行效果

- 设备启动 Wi-Fi
- 扫描附近 AP
- 串口打印 SSID、RSSI、加密方式等信息

## 关键文件

- `main/scan.c`

## 重点理解

- Wi-Fi 扫描不等于连接
- 扫描结果是后续配网和选网的基础
- RSSI 和加密方式通常是最先要看的字段

## 建议先改什么

- 改扫描间隔
- 把最强的几个热点筛出来单独打印
- 给屏幕补一个简易扫描结果摘要

## 学完后建议看

- `02.beginner.wifi_station`
- `02.beginner.wifi_softap`
