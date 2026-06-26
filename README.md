# ESP32-S3 Linux 示例项目

基于 ESP-IDF 框架的 ESP32-S3 开发示例集合，涵盖从基础入门到高级应用的完整学习路径。

## 环境要求

- **芯片**: ESP32-S3
- **开发框架**: [ESP-IDF](https://github.com/espressif/esp-idf) (v5.0+)
- **操作系统**: Linux / WSL

## 项目结构

```
esp32s3/
├── demo/ks_demo/          # 综合演示项目（LVGL + 外设）
└── ks_demo/               # 分级示例项目集
    ├── 01.basic.*         # 基础外设示例
    ├── 02.beginner.*      # 入门通信示例
    ├── 03.development.*   # 进阶开发示例
    └── 04.advanced.*      # 高级应用示例
```

### 01. 基础外设示例 (Basic)

| 示例 | 说明 |
|------|------|
| `hello_world` | 经典入门示例 |
| `gpio` | GPIO 输入输出控制 |
| `adc_continuous` | ADC 连续采样 |
| `adc_oneshot` | ADC 单次采样 |
| `gptimer` | 通用定时器 |
| `ledc_pwm` | LEDC PWM 输出 |
| `i2c` | I2C 总线通信 |
| `spiffs` | SPIFFS 文件系统 |
| `fatfs` | FAT 文件系统 |
| `nvs` | 非易失性存储 |
| `freertos` | FreeRTOS 任务管理 |
| `uart_echo` | UART 串口回显 |
| `deep_sleep` | 深度睡眠低功耗 |
| `rmt_led_strip` | RMT 驱动 WS2812 |

### 02. 入门通信示例 (Beginner)

| 示例 | 说明 |
|------|------|
| `wifi_station` | WiFi Station 模式 |
| `wifi_softap` | WiFi SoftAP 模式 |
| `wifi_scan` | WiFi 扫描 |
| `http_request` | HTTP 客户端请求 |
| `http_server` | HTTP 服务器 |
| `mqtt_tcp` | MQTT 通信 |
| `ota` | 空中升级 |
| `ble_spp_server` | BLE SPP 服务 |
| `blufi` | BluFi 配网 |
| `i2s_codec` | I2S 音频编解码 |
| `sdmmc_sd` | SD 卡读写 |
| `spi_lcd_touch` | SPI LCD 触摸屏 |
| `usb_cdc` | USB CDC 串口 |
| `usb_hid_keyboard` | USB HID 键盘 |
| `usb_hid_mouse` | USB HID 鼠标 |
| `usb_msc_flash` | USB MSC Flash 磁盘 |
| `usb_msc_ram` | USB MSC RAM 磁盘 |

### 03. 进阶开发示例 (Development)

| 示例 | 说明 |
|------|------|
| `lvgl_v9` | LVGL v9 图形库 |
| `lvgl_esp_port` | LVGL ESP 移植 |
| `lvgl_esp_adapter` | LVGL ESP 适配层 |
| `lvgl_squareline_demo` | SquareLine Studio 演示 |
| `lvgl_freetype` | LVGL FreeType 字体 |
| `lvgl_png` | LVGL PNG 解码 |
| `lvgl_jpg_download` | LVGL JPG 网络下载 |
| `lvgl_sdcard_jpg` | LVGL SD 卡 JPG 显示 |
| `lvgl_spiffs_jpg` | LVGL SPIFFS JPG 显示 |
| `lvgl_qrcode_font` | LVGL 二维码字体 |
| `lvgl_rlottie` | LVGL Lottie 动画 |
| `lvgl_rlottie_mmap` | LVGL Lottie mmap 模式 |
| `lvgl_rlottie_spiffs` | LVGL Lottie SPIFFS |
| `lvgl_adc_button_component` | LVGL ADC 按键组件 |
| `lvgl_adc_button_custom` | LVGL ADC 按键自定义 |
| `lvgl_eez_demo` | EEZ Studio 演示 |
| `audio_fft` | 音频 FFT 频谱 |
| `audio_record_play` | 音频录制播放 |
| `audio_record_sdcard` | 音频录制到 SD 卡 |
| `mp3_player` | MP3 播放器 |
| `photo_album` | 电子相册 |
| `usb_4g_module` | USB 4G 模块 |
| `usb_audio_player` | USB 音频播放器 |
| `usb_cdc_msc` | USB CDC+MSC 复合设备 |
| `usb_rndis` | USB RNDIS 网络 |
| `usb_wireless_disk` | USB 无线磁盘 |
| `low_power_wakeup` | 低功耗唤醒 |

### 04. 高级应用示例 (Advanced)

| 示例 | 说明 |
|------|------|
| `camera_lcd_display` | 摄像头 LCD 显示 |
| `camera_lvgl_display` | 摄像头 LVGL 显示 |
| `camera_webserver` | 摄像头 Web 服务器 |
| `usb_camera_display` | USB 摄像头显示 |
| `usb_extended_display` | USB 扩展屏 |
| `avi_player` | AVI 视频播放器 |
| `avi_multi_player` | AVI 多视频播放 |
| `avi_recorder` | AVI 视频录制 |
| `face_recognition` | 人脸识别 |
| `color_tracking_lvgl` | 颜色追踪 LVGL |
| `qrcode_detection` | 二维码检测 |
| `spectrum_box_lite` | 频谱盒子 Lite |
| `korvo_board_test` | Korvo 板测试 |

## 快速开始

```bash
# 1. 激活 ESP-IDF 环境
. ~/esp/esp-idf/export.sh

# 2. 进入任意示例目录
cd ks_demo/01.basic.hello_world

# 3. 编译烧录
idf.py set-target esp32s3
idf.py build flash monitor
```

## 相关资源

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)
- [ESP32-S3 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_cn.pdf)
