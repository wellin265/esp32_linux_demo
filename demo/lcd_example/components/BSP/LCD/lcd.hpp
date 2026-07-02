#ifndef __LCD_H
#define __LCD_H

#include "spi.hpp"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_ili9341.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <math.h>
#include <stdint.h>

/* ========== 颜色常量 ========== */

#define WHITE           0xFFFF
#define BLACK           0x0000
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define CYAN            0x07FF
#define BROWN           0xBC40
#define BRRED           0xFC07
#define GRAY            0x8430
#define DARKBLUE        0x01CF
#define LIGHTBLUE       0x7D7C
#define GRAYBLUE        0x5458
#define LIGHTGREEN      0x841F
#define LGRAY           0xC618
#define LGRAYBLUE       0xA651
#define LBBLUE          0x2B12

#define LCD_320X240     1
#define LCD_240X240     0

/* ========== 配置结构体 ========== */

struct LcdConfig {
    gpio_num_t pin_wr   = GPIO_NUM_15;
    gpio_num_t pin_cs   = GPIO_NUM_17;
    gpio_num_t pin_pwr  = GPIO_NUM_16;
    gpio_num_t pin_rst  = GPIO_NUM_7;
    uint16_t   width    = 320;
    uint16_t   height   = 240;
    uint8_t    dir      = 1;                      /* 0=竖屏, 1=横屏 */
    uint32_t   pclk_hz  = 60 * 1000 * 1000;
};

/* ========== 类声明 ========== */

class Lcd {
public:
    Lcd(Spi &bus, const LcdConfig &cfg = {});

    esp_err_t init();

    /* 绘图方法 */
    void clear(uint16_t color);
    void fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
    void block(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color_buf);
    void drawPoint(uint16_t x, uint16_t y, uint16_t color);
    void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void drawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
    void drawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    void drawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
    void showChar(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint8_t mode, uint16_t color);
    void showNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
    void showXnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
    void showString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);
    void setDirection(uint8_t dir);
    void setBgColor(uint16_t color);   /* 设置字符/字符串背景色 */

    /* DMA 同步标志 — on_color_trans_done 回调设置 */
    volatile bool refreshDone = false;

    /* 获取内部句柄 */
    esp_lcd_panel_handle_t handle() const { return m_panel_handle; }

private:
    uint32_t pow(uint8_t m, uint8_t n);

    Spi                     &m_bus;
    LcdConfig                m_cfg;
    uint16_t                 m_bgColor      = BLACK;   /* 文本背景色 */
    esp_lcd_panel_io_handle_t m_io_handle   = nullptr;
    esp_lcd_panel_handle_t    m_panel_handle = nullptr;
    bool                      m_initialized  = false;
};

/* ========== C 兼容层声明 ========== */

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t lcd_init(void);
void lcd_clear(uint16_t color);
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void lcd_block(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color_buf);
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void lcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void lcd_show_char(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif
