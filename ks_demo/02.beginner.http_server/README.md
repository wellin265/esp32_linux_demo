# 02.beginner.http_server

## 这个工程能学什么

这个例子演示在板子上启动 HTTP 服务器，处理 GET 和 POST 请求。

## 运行效果

- 设备联网后在本机 IP 上启动 Web 服务
- 浏览器访问后可看到响应内容
- 串口输出请求头、查询参数和处理日志
- 屏幕显示访问地址和当前服务状态

## 关键文件

- `main/main.c`

## 重点理解

- URI handler 注册方式
- GET/POST 两种请求如何处理
- 请求头、Query、Body 怎么取

## 建议先改什么

- 新增一个自定义 URL
- 修改返回文本
- 把本机 IP 和端口显示得更明显

## 学完后建议看

- `02.beginner.http_request`
- `02.beginner.ota`
