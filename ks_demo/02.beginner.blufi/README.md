# 02.beginner.blufi

## 这个工程能学什么

这个例子演示 Espressif 的 BluFi 配网流程：通过蓝牙把 Wi-Fi 信息下发给设备。

## 运行效果

- 板子启动后进入 BluFi 配网模式
- 手机 App 可通过蓝牙下发 Wi-Fi 配置
- 成功后设备尝试连接路由器
- 屏幕和串口显示当前配网/联网状态

## 关键文件

- `main/app_main.c`
- `main/blufi_init.c`
- `main/blufi_security.c`

## 重点理解

- 蓝牙配网不是普通 BLE 串口，而是一个完整配置流程
- Wi-Fi 状态和蓝牙状态是两条并行状态机
- 安全协商和配置回调是这个工程的核心

## 建议先改什么

- 改设备显示名称
- 增加配网成功后的屏幕提示
- 理清配网完成后 Wi-Fi 是在哪里真正发起连接的

## 学完后建议看

- `02.beginner.wifi_station`
- `02.beginner.ble_spp_server`
