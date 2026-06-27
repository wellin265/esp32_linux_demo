#ifndef __LCD_H__
#define __LCD_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "myspi.hpp"
#include "tca9554.hpp"

/* LCD DC 引脚 (直连 GPIO 2)
   CS 由 SPI 驱动自动控制 (GPIO 46)
   硬件复位由 esp_lcd_panel_reset() 软件复位完成 */

struct LcdPanelConfig {
    gpio_num_t dc_io = GPIO_NUM_2;
    gpio_num_t cs_io = GPIO_NUM_46;
    uint32_t pclk_hz = 80 * 1000 * 1000;
    uint16_t panel_width = 240;
    uint16_t panel_height = 280;
    uint8_t default_rotation = 1;  // landscape
};

/* LCD 旋转方向 (面板原生 240x280 竖屏) */
#define LCD_ROTATION_PORTRAIT          0   /* 竖屏 240x280 */
#define LCD_ROTATION_LANDSCAPE         1   /* 横屏 280x240 */
#define LCD_ROTATION_PORTRAIT_INV      2   /* 竖屏倒置 240x280 */
#define LCD_ROTATION_LANDSCAPE_INV     3   /* 横屏倒置 280x240 */

/* 常用颜色值 */
#define WHITE           0xFFFF
#define BLACK           0x0000
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define MAGENTA         0XF81F
#define YELLOW          0XFFE0
#define CYAN            0X07FF
#define BROWN           0XBC40
#define BRRED           0XFC07
#define GRAY            0X8430
#define DARKBLUE        0X01CF
#define LIGHTBLUE       0X7D7C
#define GRAYBLUE        0X5458
#define LIGHTGREEN      0X841F
#define LGRAY           0XC618
#define LGRAYBLUE       0XA651
#define LBBLUE          0X2B12

/* LCD信息结构体 */
typedef struct _lcd_obj_t {
    uint16_t        width;
    uint16_t        height;
    uint16_t        pwidth;
    uint16_t        pheight;
    uint8_t         dir;
} lcd_obj_t;

typedef struct _lcd_config_t {
    void *user_ctx;
    esp_lcd_panel_io_color_trans_done_cb_t notify_flush_ready;
} lcd_cfg_t;

class Lcd {
public:
    Lcd(MySpi &spi, const LcdPanelConfig &dev_cfg = {});
    void init(lcd_cfg_t lcd_config);
    void clear(uint16_t color);
    void fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
    void colorFill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
    void showMonoIcos(uint16_t x, uint16_t y, uint8_t width, uint8_t height,
                      uint8_t *icosbase, uint16_t color, uint16_t bkcolor);
    void drawPoint(uint16_t x, uint16_t y, uint16_t color);
    void displayDir(uint8_t dir);
    void setRotation(uint8_t rot);
    void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void drawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
    void drawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
    void showChar(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);
    void showString(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    uint8_t size, const char *p, uint16_t color);
    lcd_obj_t& getDev();
    esp_lcd_panel_handle_t getPanelHandle();

private:
    static constexpr const char *TAG = "LCD";
    MySpi &m_spi;
    LcdPanelConfig m_dev_cfg;
    lcd_obj_t dev = {};
    esp_lcd_panel_handle_t panelHandle = nullptr;
    esp_lcd_panel_io_handle_t ioHandle = nullptr;
    uint16_t backColor = 0xFFFF;
    volatile bool refreshDone = false;

    static bool onRefreshDone(esp_lcd_panel_io_handle_t panel_io,
                              esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
};

extern lcd_obj_t lcd_dev;
extern esp_lcd_panel_handle_t panel_handle;
extern uint32_t g_back_color;

#ifdef __cplusplus
extern "C" {
#endif

void lcd_init(lcd_cfg_t lcd_config);
void lcd_clear(uint16_t color);
void lcd_display_dir(uint8_t dir);
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, const char *p, uint16_t color);
void lcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void lcd_app_show_mono_icos(uint16_t x, uint16_t y, uint8_t width, uint8_t height, uint8_t *icosbase, uint16_t color, uint16_t bkcolor);

#ifdef __cplusplus
}
#endif

#endif
