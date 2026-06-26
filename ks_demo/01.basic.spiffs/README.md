# 01.basic.spiffs

## 这个工程能学什么

这个例子演示 SPIFFS 文件系统的挂载、信息查询、文件写入、重命名和读取。

如果你想在 ESP32 上放一些小资源文件，比如配置、图片、文本，SPIFFS 是非常常见的起点。

## 运行后你会看到什么

- 屏幕显示 SPIFFS 挂载与操作状态
- 串口打印分区大小、文件操作结果和错误信息
- 程序会创建、重命名并读取文件

## 关键文件

- `main/spiffs_example_main.c`

## 代码做了什么

主流程是：

1. 初始化显示
2. 注册并挂载 `/spiffs`
3. 查询分区总容量和已用容量
4. 创建 `hello.txt`
5. 重命名为 `foo.txt`
6. 重新读取内容验证
7. 结束时卸载 SPIFFS

## 建议重点看哪几段

- `esp_vfs_spiffs_register()`
- `esp_spiffs_info()`
- `fopen()/fprintf()/fread()`
- `rename()`
- `esp_vfs_spiffs_unregister()`

## 你会学到的知识点

- SPIFFS 在 ESP-IDF 里的最基本流程
- 文件系统和 NVS 的使用边界
- 文件类工程怎么做错误提示比较清晰

## 第一次建议你改什么

- 改写入文件的内容
- 新增第二个文件
- 在屏幕上加一行显示文件名

## 学完后建议看

- `01.basic.nvs`
- `01.basic.fatfs`
- `03.development.photo_album`
