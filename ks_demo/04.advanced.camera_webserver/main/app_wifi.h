/**
 * @file app_wifi.h
 * @brief WiFi Station 模式初始化接口头文件
 *
 * 提供 WiFi Station 模式的初始化接口声明，
 * 供摄像头 Web 服务器等应用模块调用。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 * @license MIT
 */

/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _APP_WIFI_H_
#define _APP_WIFI_H_

void wifi_init_sta(void);


#endif
