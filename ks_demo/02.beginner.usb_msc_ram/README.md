# 02.beginner.usb_msc_ram

## 这个工程能学什么

这个例子演示最简单的 USB MSC 设备：用 RAM 当作存储后端。

## 运行效果

- 电脑识别为 USB 存储设备
- 存储内容在 RAM 里，掉电不会保留
- 屏幕显示 USB 存储设备启动状态

## 关键文件

- `main/app_main.c`
- `main/usbx_msc.c`

## 重点理解

- RAM 后端适合快速验证枚举和读写流程
- 这类工程更关注协议正确性，不是持久化
- 和 Flash/SD 后端相比实现更简单

## 建议先改什么

- 改容量大小
- 增加主机连接后的状态显示
- 对比和 `usb_msc_flash` 的行为差异

## 学完后建议看

- `02.beginner.usb_msc_flash`
- `03.development.usb_cdc_msc`
