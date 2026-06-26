# 02.beginner.sdmmc_sd

## 这个工程能学什么

这个例子演示通过 SDMMC 挂载 SD 卡并做文件读写。

## 运行效果

- 初始化 SDMMC 主机
- 挂载 SD 卡文件系统
- 创建、重命名并读取文件
- 屏幕显示挂载状态和卡片信息

## 关键文件

- `main/sd_mmc_example_main.c`

## 重点理解

- SD 卡和 SPI Flash 文件系统不同，它是外部存储
- SDMMC host、slot、mount_config 三个结构的角色
- 文件系统操作成功后最好同时打印卡片信息

## 建议先改什么

- 改挂载点
- 写入自己的测试内容
- 观察卡没插好和文件系统挂载失败时的表现

## 学完后建议看

- `01.basic.fatfs`
- `03.development.audio_record_sdcard`
