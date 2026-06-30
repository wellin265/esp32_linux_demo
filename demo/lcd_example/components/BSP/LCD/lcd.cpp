/**
 * ILI9341 LCD 驱动
 *
 * 关键信息:
 *   - 总线: SPI (esp_lcd_panel_io_spi 内部管理 SPI 设备, CS=17)
 *   - 分辨率: 320×240, 16bit RGB565
 *   - 硬件连接: WR=15, CS=17, PWR=16, RST=7
 *   - DMA 异步传输: 所有绘图函数在释放缓冲区前等待 refreshDone
 */

#include "lcd.hpp"
#include "hal/color_types.h"
#include "lcdfont.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LCD";

/* SPI LCD 类型 (1=2.4寸, 0=1.3寸) */
#define SPI_LCD_TYPE    1

/* ========== DMA 回调 ========== */

static bool onRefreshDone(esp_lcd_panel_io_handle_t panel_io,
                          esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    Lcd *lcd = static_cast<Lcd *>(user_ctx);
    lcd->refreshDone = true;
    return false;
}

/* ========== 构造 ========== */

Lcd::Lcd(Spi &bus, const LcdConfig &cfg)
    : m_bus(bus), m_cfg(cfg) {}

/* ========== 硬件初始化 ========== */

esp_err_t Lcd::init() {
    if (m_initialized) return ESP_OK;

    /* 1. GPIO 配置 */
    gpio_config_t gpio_init_struct = {};

    gpio_init_struct.intr_type    = GPIO_INTR_DISABLE;
    gpio_init_struct.mode         = GPIO_MODE_INPUT_OUTPUT;
    gpio_init_struct.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;

    gpio_init_struct.pin_bit_mask = 1ull << m_cfg.pin_pwr;
    ESP_ERROR_CHECK(gpio_config(&gpio_init_struct));

    gpio_init_struct.pin_bit_mask = 1ull << m_cfg.pin_cs;
    ESP_ERROR_CHECK(gpio_config(&gpio_init_struct));

    gpio_init_struct.pin_bit_mask = 1ull << m_cfg.pin_wr;
    ESP_ERROR_CHECK(gpio_config(&gpio_init_struct));

    /* 2. 硬件复位 */
    gpio_set_level(m_cfg.pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(m_cfg.pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 3. 创建 SPI IO 句柄 (LCD 面板挂载至 SPI 总线) */
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num       = m_cfg.pin_wr;
    io_config.cs_gpio_num       = m_cfg.pin_cs;
    io_config.pclk_hz           = m_cfg.pclk_hz;
    io_config.lcd_cmd_bits      = 8;
    io_config.lcd_param_bits    = 8;
    io_config.spi_mode          = 0;
    io_config.trans_queue_depth = 7;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)m_bus.handle(), &io_config, &m_io_handle));

    /* 4. 创建 ILI9341 面板句柄 */
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = m_cfg.pin_rst;
    panel_config.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.data_endian    = LCD_RGB_DATA_ENDIAN_BIG;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(m_io_handle, &panel_config, &m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(m_panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_init(m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(m_panel_handle, true));

    /* 5. 注册 DMA 传输完成回调 */
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = onRefreshDone,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(m_io_handle, &cbs, this));

    /* 6. 横屏 + 清屏 */
    setDirection(m_cfg.dir);
    clear(WHITE);

    /* 7. 背光使能 */
    gpio_set_level(m_cfg.pin_pwr, 1);

    m_initialized = true;
    ESP_LOGI(TAG, "LCD initialized");
    return ESP_OK;
}

/* ========== 显示方向 ========== */

void Lcd::setDirection(uint8_t dir) {
    m_cfg.dir = dir;

    if (dir == 0) {          /* 竖屏 */
        m_cfg.width  = 240;
        m_cfg.height = 320;
        esp_lcd_panel_swap_xy(m_panel_handle, false);
        esp_lcd_panel_mirror(m_panel_handle, true, false);
    } else {                 /* 横屏 */
        m_cfg.width  = 320;
        m_cfg.height = 240;
        esp_lcd_panel_swap_xy(m_panel_handle, true);
        esp_lcd_panel_mirror(m_panel_handle, false, false);
    }
}

/* ========== 清屏 ========== */

void Lcd::clear(uint16_t color) {
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(
        m_cfg.width * sizeof(uint16_t) * 40, MALLOC_CAP_DMA);
    uint16_t color_tmp = ~color;

    if (NULL == buffer) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }

    for (uint32_t i = 0; i < m_cfg.width * 40; i++) {
        buffer[i] = color_tmp;
    }

    for (uint16_t y = 0; y < m_cfg.height; y += 40) {
        esp_lcd_panel_draw_bitmap(m_panel_handle, 0, y,
                                  m_cfg.width, y + 40, buffer);
    }

    /* 等待 DMA 传输完成 */
    refreshDone = false;
    do {
        vTaskDelay(1);
    } while (!refreshDone);

    heap_caps_free(buffer);
}

/* ========== 区域填充 ========== */

void Lcd::fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color) {
    uint16_t width  = ex - sx;
    uint16_t height = ey - sy;

    uint16_t *buffer = (uint16_t *)heap_caps_malloc(
        width * sizeof(uint16_t), MALLOC_CAP_DMA);
    uint16_t color_tmp = ~color;

    if (NULL == buffer) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }

    for (uint16_t i = 0; i < width; i++) {
        buffer[i] = color_tmp;
    }

    for (uint16_t y = 0; y < height; y++) {
        esp_lcd_panel_draw_bitmap(m_panel_handle, sx, sy + y, ex, sy + y + 1, buffer);
    }

    /* 等待 DMA 传输完成再释放缓冲区 */
    refreshDone = false;
    do {
        vTaskDelay(1);
    } while (!refreshDone);

    heap_caps_free(buffer);
}

/* ========== 像素块绘制 ========== */

void Lcd::block(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color_buf) {
    uint16_t width  = ex - sx;
    uint16_t height = ey - sy;

    uint16_t *buffer = (uint16_t *)heap_caps_malloc(
        width * sizeof(uint16_t), MALLOC_CAP_DMA);

    if (NULL == buffer) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }

    for (uint16_t i = 0; i < width; i++) {
        buffer[i] = ~color_buf[i];
    }

    for (uint16_t y = 0; y < height; y++) {
        esp_lcd_panel_draw_bitmap(m_panel_handle, sx, sy + y, ex, sy + y + 1, buffer);
    }

    /* 等待 DMA 传输完成再释放缓冲区 */
    refreshDone = false;
    do {
        vTaskDelay(1);
    } while (!refreshDone);

    heap_caps_free(buffer);
}

/* ========== 画点 ========== */

void Lcd::drawPoint(uint16_t x, uint16_t y, uint16_t color) {
    uint16_t color_tmp = ~color;
    esp_lcd_panel_draw_bitmap(m_panel_handle, x, y, x + 1, y + 1, &color_tmp);
}

/* ========== 画线 (Bresenham) ========== */

void Lcd::drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0) {
        incx = 1;
    } else if (delta_x == 0) {
        incx = 0;
    } else {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0) {
        incy = 1;
    } else if (delta_y == 0) {
        incy = 0;
    } else {
        incy = -1;
        delta_y = -delta_y;
    }

    distance = (delta_x > delta_y) ? delta_x : delta_y;

    for (t = 0; t <= distance + 1; t++) {
        drawPoint(row, col, color);
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance) {
            xerr -= distance;
            row += incx;
        }

        if (yerr > distance) {
            yerr -= distance;
            col += incy;
        }
    }
}

/* ========== 画水平线 ========== */

void Lcd::drawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color) {
    if (len == 0 || x >= m_cfg.width || y >= m_cfg.height) return;

    uint16_t ex = fmin(m_cfg.width - 1, x + len - 1);
    uint16_t ey = y;

    uint32_t w = ex - x + 1;
    uint32_t h = ey - y + 1;
    uint16_t *color_buffer = (uint16_t *)heap_caps_malloc(
        w * h * sizeof(uint16_t), MALLOC_CAP_DMA);

    if (color_buffer == NULL) return;

    for (uint32_t i = 0; i < w * h; i++) {
        color_buffer[i] = color;
    }

    esp_lcd_panel_draw_bitmap(m_panel_handle, x, y, ex + 1, ey + 1, color_buffer);

    refreshDone = false;
    do {
        vTaskDelay(1);
    } while (!refreshDone);

    heap_caps_free(color_buffer);
}

/* ========== 画矩形框 ========== */

void Lcd::drawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    drawLine(x0, y0, x1, y0, color);
    drawLine(x0, y0, x0, y1, color);
    drawLine(x0, y1, x1, y1, color);
    drawLine(x1, y0, x1, y1, color);
}

/* ========== 画圆 (Bresenham) ========== */

void Lcd::drawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int a, b;
    int di;
    a = 0;
    b = r;
    di = 3 - (r << 1);

    while (a <= b) {
        drawPoint(x0 - b, y0 - a, color);
        drawPoint(x0 + b, y0 - a, color);
        drawPoint(x0 - a, y0 + b, color);
        drawPoint(x0 - b, y0 - a, color);
        drawPoint(x0 - a, y0 - b, color);
        drawPoint(x0 + b, y0 + a, color);
        drawPoint(x0 + a, y0 - b, color);
        drawPoint(x0 + a, y0 + b, color);
        drawPoint(x0 - b, y0 + a, color);
        a++;

        if (di < 0) {
            di += 4 * a + 6;
        } else {
            di += 10 + 4 * (a - b);
            b--;
        }

        drawPoint(x0 + a, y0 + b, color);
    }
}

/* ========== 字符显示 ========== */

void Lcd::showChar(uint16_t x, uint16_t y, uint8_t chr, uint8_t size,
                   uint8_t mode, uint16_t color) {
    const uint8_t *ch_code;
    uint8_t ch_width;
    uint8_t ch_height;
    uint8_t ch_size;
    uint8_t ch_offset;
    uint8_t byte_index;
    uint8_t byte_code;
    uint8_t bit_index;
    uint16_t colortemp = 0;
    uint16_t pix_index = 0;

    /* 字体大小(字节) = 字体宽度占用字节 × 字体高度 */
    ch_size = ((size / 2) / 8 + (((size / 2) % 8) ? 1 : 0)) * size;

    uint16_t *pcolor = (uint16_t *)heap_caps_malloc(
        size * (size / 2) * 2, MALLOC_CAP_DMA);
    uint16_t color_tmp = ~color;

    if (NULL == pcolor) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }

    ch_offset = chr - ' ';

    switch (size) {
        case 12:
            ch_code   = (uint8_t *)asc2_1206[ch_offset];
            ch_width  = 6;
            ch_height = 12;
            break;
        case 16:
            ch_code   = (uint8_t *)asc2_1608[ch_offset];
            ch_width  = 8;
            ch_height = 16;
            break;
        case 24:
            ch_code   = (uint8_t *)asc2_2412[ch_offset];
            ch_width  = 12;
            ch_height = 24;
            break;
        case 32:
            ch_code   = (uint8_t *)asc2_3216[ch_offset];
            ch_width  = 16;
            ch_height = 32;
            break;
        default:
            heap_caps_free(pcolor);
            return;
    }

    for (byte_index = 0; byte_index < ch_size; byte_index++) {
        byte_code = ch_code[byte_index];

        for (bit_index = 0; bit_index < 8; bit_index++) {
            if ((byte_code & 0x80) != 0) {
                colortemp = color_tmp;
            } else if (mode == 0) {
                colortemp = 0xFFFF;
            }

            pcolor[pix_index] = colortemp;
            pix_index++;

            if ((size == 24) && (byte_index % 2)) {
                if (bit_index == 3) break;
            }

            byte_code <<= 1;
        }
    }

    esp_lcd_panel_draw_bitmap(m_panel_handle, x, y,
                              x + ch_width, y + ch_height, pcolor);

    refreshDone = false;
    do {
        vTaskDelay(1);
    } while (!refreshDone);

    heap_caps_free(pcolor);
}

/* ========== 辅助: m^n ========== */

uint32_t Lcd::pow(uint8_t m, uint8_t n) {
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

/* ========== 显示数字 ========== */

void Lcd::showNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len,
                  uint8_t size, uint16_t color) {
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++) {
        temp = (num / pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                showChar(x + (size / 2) * t, y, ' ', size, 0, color);
                continue;
            } else {
                enshow = 1;
            }
        }

        showChar(x + (size / 2) * t, y, temp + '0', size, 0, color);
    }
}

/* ========== 扩展显示数字 (高位填0) ========== */

void Lcd::showXnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len,
                   uint8_t size, uint8_t mode, uint16_t color) {
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++) {
        temp = (num / pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                if (mode & 0x80) {
                    showChar(x + (size / 2) * t, y, '0', size, mode & 0x01, color);
                } else {
                    showChar(x + (size / 2) * t, y, ' ', size, mode & 0x01, color);
                }
                continue;
            } else {
                enshow = 1;
            }
        }

        showChar(x + (size / 2) * t, y, temp + '0', size, mode & 0x01, color);
    }
}

/* ========== 显示字符串 ========== */

void Lcd::showString(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     uint8_t size, char *p, uint16_t color) {
    uint8_t x0 = x;
    width  += x;
    height += y;

    while ((*p <= '~') && (*p >= ' ')) {
        if (x >= width) {
            x = x0;
            y += size;
        }

        if (y >= height) break;

        showChar(x, y, *p, size, 0, color);

        x += size / 2;
        p++;
    }
}

/* ================================================================
 * C 兼容层
 * ================================================================ */

extern "C" {

static SpiConfig s_spi_cfg;
static Spi      s_spi(s_spi_cfg);
static LcdConfig s_lcd_cfg;
static Lcd      s_lcd(s_spi, s_lcd_cfg);

esp_err_t lcd_init(void) {
    s_spi.init();
    return s_lcd.init();
}

void lcd_clear(uint16_t color)                      { s_lcd.clear(color); }
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex,
              uint16_t ey, uint16_t color)           { s_lcd.fill(sx, sy, ex, ey, color); }
void lcd_block(uint16_t sx, uint16_t sy, uint16_t ex,
               uint16_t ey, uint16_t *color_buf)     { s_lcd.block(sx, sy, ex, ey, color_buf); }
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color)        { s_lcd.drawPoint(x, y, color); }
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2,
                   uint16_t y2, uint16_t color)                     { s_lcd.drawLine(x1, y1, x2, y2, color); }
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len,
                    uint16_t color)                                  { s_lcd.drawHLine(x, y, len, color); }
void lcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1,
                        uint16_t y1, uint16_t color)                { s_lcd.drawRect(x0, y0, x1, y1, color); }
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r,
                     uint16_t color)                                 { s_lcd.drawCircle(x0, y0, r, color); }
void lcd_show_char(uint16_t x, uint16_t y, uint8_t chr, uint8_t size,
                   uint8_t mode, uint16_t color)                     { s_lcd.showChar(x, y, chr, size, mode, color); }
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len,
                  uint8_t size, uint16_t color)                      { s_lcd.showNum(x, y, num, len, size, color); }
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len,
                   uint8_t size, uint8_t mode, uint16_t color)       { s_lcd.showXnum(x, y, num, len, size, mode, color); }
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     uint8_t size, char *p, uint16_t color)          { s_lcd.showString(x, y, width, height, size, p, color); }

}
