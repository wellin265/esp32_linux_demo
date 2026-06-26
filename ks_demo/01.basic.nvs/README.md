# 01.basic.nvs

## 这个工程能学什么

这个例子演示 NVS 键值存储的基本用法。

它很适合拿来理解“设备掉电后如何保留参数”。

## 运行后你会看到什么

- 屏幕显示 NVS 初始化和读写状态
- 串口输出 `counter` 和 `message` 的写入、读取和删除过程

## 关键文件

- `main/nvs_value_example_main.c`

## 代码做了什么

主流程是：

1. 初始化显示
2. 初始化 NVS
3. 打开命名空间 `storage`
4. 写入整数 `counter`
5. 写入字符串 `message`
6. 再读回来验证
7. 删除一个 key 并提交

## 建议重点看哪几段

- `nvs_flash_init()`
- `nvs_open()`
- `nvs_set_i32()`
- `nvs_set_str()`
- `nvs_get_i32()`
- `nvs_commit()`

## 你会学到的知识点

- NVS 更适合存参数，不适合存大文件
- 写入以后为什么要 `commit`
- 整数和字符串读取方式有什么不同

## 第一次建议你改什么

- 把 `counter` 改成自己的参数名
- 多存一个布尔型或字符串配置
- 重启设备后观察数据是否仍存在

## 学完后建议看

- `01.basic.spiffs`
- `01.basic.deep_sleep`
