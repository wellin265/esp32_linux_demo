# 04.advanced.usb_camera_display

## 这个工程能学什么

演示 USB UVC 摄像头采集、MJPEG 解码和本地 LCD 实时显示。

## 运行效果

- 接入 USB 摄像头
- 获取 MJPEG 帧
- 解码成 RGB565
- 在 LCD/LVGL 画布上显示

## 关键文件

- `main/app_main.c`
- `main/app_jpeg_dec.c`
- `main/app_nvs_storage.c`

## 学完后建议看

- `04.advanced.camera_webserver`
- `04.advanced.usb_extended_display`
