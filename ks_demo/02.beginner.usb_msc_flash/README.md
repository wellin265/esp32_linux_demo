# 02.beginner.usb_msc_flash

## 这个工程能学什么

这个例子演示把板子内部 Flash 暴露成一个 USB MSC 存储设备。

## 运行效果

- 电脑识别设备为 U 盘
- 存储介质来自板载 Flash
- 屏幕显示 USB 存储设备初始化状态

## 关键文件

- `main/app_main.c`
- `main/usbx_msc.c`

## 重点理解

- MSC 的重点在块设备读写
- Flash 版和 RAM 版 MSC 的存储后端不同
- 枚举成功不代表文件系统一定可用，还要看后端实现

## 建议先改什么

- 看清底层存储区是怎么映射的
- 给屏幕增加容量提示
- 对比 `usb_msc_ram` 的不同

## 学完后建议看

- `02.beginner.usb_msc_ram`
- `03.development.usb_wireless_disk`
