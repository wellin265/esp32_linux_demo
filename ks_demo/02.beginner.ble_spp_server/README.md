# 02.beginner.ble_spp_server

## 这个工程能学什么

这个例子演示 BLE SPP 风格的数据通道，也就是通过 BLE GATT 做类似“蓝牙串口”的收发。

## 运行效果

- 板子会以 `ESP_SPP_SERVER` 名称广播
- 手机或上位机连接后可做数据收发
- 屏幕会显示蓝牙连接与数据状态

## 关键文件

- `main/app_main.c`
- `main/ksdiy_lvgl_port.c`

## 重点理解

- GATT 服务和特征值的基本组织方式
- BLE 连接、通知、写入事件处理
- 屏幕状态和 BLE 状态如何联动

## 建议先改什么

- 改广播名
- 改连接后的屏幕提示
- 在串口里增加收发日志

## 学完后建议看

- `02.beginner.blufi`
- `02.beginner.usb_cdc`
