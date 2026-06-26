# 02.beginner.usb_cdc

## 这个工程能学什么

这个例子演示 CherryUSB CDC ACM 设备，也就是把板子模拟成一个 USB 虚拟串口。

## 运行效果

- 板子接到电脑后枚举成 CDC 设备
- 屏幕显示 USB 初始化和等待主机连接状态
- 主机连接后可通过虚拟串口通信

## 关键文件

- `main/app_main.c`
- `main/usbx_cdc.c`

## 重点理解

- USB 描述符决定主机看到什么设备
- CDC ACM 是最常见的 USB 入门设备类型
- `app_main` 通常只做启动，细节都在 USB 模块文件里

## 建议先改什么

- 改 VID/PID 或字符串描述符
- 增加串口收发提示
- 观察设备管理器中的枚举结果

## 学完后建议看

- `02.beginner.usb_msc_ram`
- `03.development.usb_cdc_msc`
