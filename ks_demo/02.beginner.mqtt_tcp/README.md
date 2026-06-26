# 02.beginner.mqtt_tcp

## 这个工程能学什么

这个例子演示 MQTT 客户端连接、发布、订阅和断线重连。

## 运行效果

- 设备连接 Broker
- 自动发布测试消息
- 订阅/取消订阅几个示例 Topic
- 屏幕显示 Broker 地址和当前 MQTT 状态

## 关键文件

- `main/app_main.c`

## 重点理解

- MQTT 事件回调模型
- publish 和 subscribe 的基本流程
- Wi-Fi 连接成功后为什么通常才启动 MQTT

## 建议先改什么

- 改 Broker URL
- 改 Topic 名称
- 在收到消息时把内容显示到屏幕上

## 学完后建议看

- `02.beginner.http_request`
- `02.beginner.ota`
