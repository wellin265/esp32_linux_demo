# 04.advanced.usb_extended_display

## 这个工程能学什么

这是一个很有代表性的复合 USB 工程：主机通过 USB 给板子发送显示帧，板子负责显示，同时把触摸作为 HID 返回给主机。

## 运行效果

- 板子作为 USB 设备启动
- 接收主机发送的 JPG/帧数据
- 解码并显示到 LCD
- 触摸事件通过 HID 回传

## 关键文件

- `main/usb_extend_screen.c`
- `main/app_usb.c`
- `main/app_lcd_s3.c`
- `main/app_touch.c`
- `main/app_vendor.c`

## 学完后建议看

- `02.beginner.usb_hid_mouse`
- `04.advanced.usb_camera_display`
