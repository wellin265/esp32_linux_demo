#ifndef __LCD_H__
#define __LCD_H__

#include "driver/gpio.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_log.h"
#include "../../managed_components/espressif__esp_lcd_ili9341/include/esp_lcd_ili9341.h"
#include "math.h"


/* 引脚定义 */
#define LCD_NUM_WR       GPIO_NUM_15
#define LCD_NUM_CS       GPIO_NUM_17
#define LCD_NUM_PWR      GPIO_NUM_16
#define LCD_NUM_RST      GPIO_NUM_7

/* IO操作 */
#define LCD_WR(x)       do{ x ? \
                            (gpio_set_level(LCD_NUM_WR, 1)):    \
                            (gpio_set_level(LCD_NUM_WR, 0));    \
                        }while(0)

#define LCD_CS(x)       do{ x ? \
                            (gpio_set_level(LCD_NUM_CS, 1)):    \
                            (gpio_set_level(LCD_NUM_CS, 0));    \
                        }while(0)

#define LCD_PWR(x)       do{ x ? \
                            (gpio_set_level(LCD_NUM_PWR, 1)):    \
                            (gpio_set_level(LCD_NUM_PWR, 0));    \
                        }while(0)

#define LCD_RST(x)       do{ }while(0)

#define LCD_HOST         SPI2_HOST

/* 常用颜色值 */
#define WHITE           0xFFFF      /* 白色 */
#define BLACK           0x0000      /* 黑色 */
#define RED             0xF800      /* 红色 */
#define GREEN           0x07E0      /* 绿色 */
#define BLUE            0x001F      /* 蓝色 */ 
#define MAGENTA         0XF81F      /* 品红色/紫红色 = BLUE + RED */
#define YELLOW          0XFFE0      /* 黄色 = GREEN + RED */
#define CYAN            0X07FF      /* 青色 = GREEN + BLUE */  

/* 非常用颜色 */
#define BROWN           0XBC40      /* 棕色 */
#define BRRED           0XFC07      /* 棕红色 */
#define GRAY            0X8430      /* 灰色 */ 
#define DARKBLUE        0X01CF      /* 深蓝色 */
#define LIGHTBLUE       0X7D7C      /* 浅蓝色 */ 
#define GRAYBLUE        0X5458      /* 灰蓝色 */ 
#define LIGHTGREEN      0X841F      /* 浅绿色 */  
#define LGRAY           0XC618      /* 浅灰色(PANNEL),窗体背景色 */ 
#define LGRAYBLUE       0XA651      /* 浅灰蓝色(中间层颜色) */ 
#define LBBLUE          0X2B12      /* 浅棕蓝色(选择条目的反色) */ 

/* 屏幕选择 */
#define LCD_320X240     1
#define LCD_240X240     0

/* LCD信息结构体 */
typedef struct _lcd_obj_t
{
    uint16_t        width;          /* 宽度 */
    uint16_t        height;         /* 高度 */
    uint8_t         dir;            /* 横屏还是竖屏控制：0，竖屏；1，横屏。 */
    uint16_t        wramcmd;        /* 开始写gram指令 */
    uint16_t        setxcmd;        /* 设置x坐标指令 */
    uint16_t        setycmd;        /* 设置y坐标指令 */
    uint16_t        wr;             /* 命令/数据IO */
    uint16_t        cs;             /* 片选IO */
    uint16_t        pwr;
    uint16_t        rst;
} lcd_obj_t;

/* LCD缓存大小设置，修改此值时请注意！！！！修改这两个值时可能会影响以下函数 lcd_clear/lcd_fill/lcd_draw_line */
#define LCD_TOTAL_BUF_SIZE      (320 * 240 * 2)
#define LCD_BUF_SIZE            15360

/* 导出相关变量 */
extern lcd_obj_t lcddev;
extern uint8_t lcd_buf[LCD_TOTAL_BUF_SIZE];

/* 函数声明 */
esp_err_t lcd_init(void);                                                                                                    /* 初始化LCD */
void lcd_display_dir(uint8_t dir);
void lcd_clear(uint16_t color);
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void lcd_block(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color_buf);
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void lcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,uint16_t color);
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void lcd_show_char(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);

                                                                                       

#endif
