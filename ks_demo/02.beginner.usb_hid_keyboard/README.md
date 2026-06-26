# 02.beginner.usb_hid_keyboard

## 这个工程能学什么

这个例子演示把板子做成 USB HID 键盘。

## 运行效果

- 电脑识别设备为 HID 键盘
- 示例会发送键盘报告
- 串口可看到 USB 初始化日志

## 关键文件

- `main/app_main.c`
- `main/usbx_demo.c`

## 重点理解

- HID 报告描述符是键盘功能核心
- 键盘和 CDC/MSC 是不同设备类别
- HID 更像“输入事件”而不是字节流通信

## 建议先改什么

- 改发送的按键内容
- 增加按键发送间隔
- 对比和鼠标 HID 的结构差异

## 学完后建议看

- `02.beginner.usb_hid_mouse`
- `04.advanced.usb_extended_display`
