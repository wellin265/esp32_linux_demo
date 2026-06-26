# 02.beginner.usb_hid_mouse

## 这个工程能学什么

这个例子演示把板子做成 USB HID 鼠标。

## 运行效果

- 电脑识别设备为鼠标
- 示例会发送鼠标移动或按键报告
- 串口可看到设备启动日志

## 关键文件

- `main/app_main.c`
- `main/usbx_demo.c`

## 重点理解

- HID 鼠标报告结构
- 主机枚举成功后事件是怎样发送的
- 为什么 HID 项目通常要先理解报告描述符

## 建议先改什么

- 改鼠标移动步长
- 改按键行为
- 用抓包或调试工具观察 HID 报告

## 学完后建议看

- `02.beginner.usb_hid_keyboard`
- `04.advanced.usb_extended_display`
