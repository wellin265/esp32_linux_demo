# 02.beginner.spi_lcd_touch

## 这个工程能学什么

这个例子演示 SPI LCD、触摸芯片和 LVGL 的完整基础接法，是后面几乎所有图形界面工程的起点。

## 运行效果

- 初始化 ST7789 屏幕
- 初始化 CST816S 触摸
- 启动 LVGL
- 运行一个 LVGL 仪表盘/演示界面

## 关键文件

- `main/app_main.c`
- `main/lvgl_demo_ui.c`

## 重点理解

- 显示驱动、触摸驱动、LVGL 三层怎么串起来
- LVGL demo UI 和底层驱动是分离的
- 图形工程的“硬件初始化”和“界面逻辑”最好拆文件

## 建议先改什么

- 改界面上的文字
- 增加一个按钮或标签
- 触摸一下屏幕，观察坐标和交互效果

## 学完后建议看

- `03.development.lvgl_esp_port`
- `03.development.lvgl_v9`
