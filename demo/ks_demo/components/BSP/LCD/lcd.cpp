/**
 * ST7789 LCD 驱动 (面板原生 240×280 竖屏, 横屏模式 280×240)
 *
 * 颜色格式: 16bit RGB565, BGR 元素顺序
 * SPI:      MOSI=0, SCLK=1, DC=2, CS=46
 * 供电:     TCA9554 P1, 复位: TCA9554 P2 硬复位 + 软复位
 * 横屏:     swap_xy=true, mirror_x=true, mirror_y=false
 */

#include "lcd.hpp"
#include "lcdfont.hpp"
#include "cst816s.hpp"
#include "esp_log.h"

Lcd::Lcd(MySpi &spi, const LcdPanelConfig &dev_cfg)
    : m_spi(spi), m_dev_cfg(dev_cfg) {}

void Lcd::init(lcd_cfg_t lcd_config) {
    dev.pwidth = m_dev_cfg.panel_width;
    dev.pheight = m_dev_cfg.panel_height;

    /* 1. 创建 SPI 面板 IO */
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = m_dev_cfg.cs_io;
    io_config.dc_gpio_num = m_dev_cfg.dc_io;
    io_config.spi_mode = 0;
    io_config.pclk_hz = m_dev_cfg.pclk_hz;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = lcd_config.notify_flush_ready;
    io_config.user_ctx = lcd_config.user_ctx;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(m_spi.host(), &io_config, &ioHandle));

    /* 3. 创建 ST7789 面板 */
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 16;
    panel_config.reset_gpio_num = GPIO_NUM_NC;

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(ioHandle, &panel_config, &panelHandle));

    /* 4. 软件复位 + 初始化 */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle));

    /* 4.5 设置面板偏移: ST7789 控制器 240x320, 可视区 240x280 居中 → y_gap=20 */
    esp_lcd_panel_set_gap(panelHandle, 20, 0);

    /* 5. 默认方向 */
    setRotation(m_dev_cfg.default_rotation);

    /* 6. 颜色反转 (参考: ST7789 需要反转) */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panelHandle, true));

    /* 7. 开显示 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panelHandle, true));

    /* 7.5 注册刷新完成回调 */
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = onRefreshDone,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(ioHandle, &cbs, this));

    /* 8. 清屏 (供电 P1 已在 TCA9554 初始化时设为 HIGH) */
    clear(WHITE);

    lcd_dev = dev;
    panel_handle = panelHandle;

    ESP_LOGI(TAG, "ST7789 LCD initialized: %dx%d", dev.width, dev.height);
}

bool Lcd::onRefreshDone(esp_lcd_panel_io_handle_t panel_io,
                         esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    Lcd *self = static_cast<Lcd *>(user_ctx);
    self->refreshDone = true;
    return false;
}

void Lcd::clear(uint16_t color) {
    uint16_t y = 0;
    const uint32_t MAX_BUFFER_SIZE = 65536;
    uint16_t block_height = MAX_BUFFER_SIZE / (dev.width * sizeof(uint16_t));
    if (block_height == 0) block_height = 1;

    uint16_t *buffer = static_cast<uint16_t *>(
        heap_caps_malloc(dev.width * block_height * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (nullptr == buffer) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }
    for (uint32_t i = 0; i < dev.width * block_height; i++) buffer[i] = color;
    while (y < dev.height) {
        uint16_t current_height = (y + block_height > dev.height) ? (dev.height - y) : block_height;
        esp_lcd_panel_draw_bitmap(panelHandle, 0, y, dev.width, y + current_height, buffer);
        y += current_height;
    }
    heap_caps_free(buffer);
    vTaskDelay(1);
}

void Lcd::fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color) {
    if (sx >= dev.width || sy >= dev.height || ex >= dev.width || ey >= dev.height || sx > ex || sy > ey) {
        ESP_LOGE(TAG, "Invalid fill area: sx=%d, sy=%d, ex=%d, ey=%d", sx, sy, ex, ey);
        return;
    }
    uint16_t width = ex - sx + 1;
    uint16_t height = ey - sy + 1;
    uint16_t *buffer = static_cast<uint16_t *>(heap_caps_malloc(width * sizeof(uint16_t), MALLOC_CAP_INTERNAL));
    if (nullptr == buffer) { ESP_LOGE(TAG, "Memory for bitmap is not enough"); return; }
    for (uint16_t i = 0; i < width; i++) buffer[i] = color;
    for (uint16_t y = 0; y < height; y++)
        esp_lcd_panel_draw_bitmap(panelHandle, sx, sy + y, ex + 1, sy + y + 1, buffer);
    drawPoint(ex, ey, color);
    heap_caps_free(buffer);
}

void Lcd::colorFill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color) {
    if (sx >= dev.width || sy >= dev.height || ex >= dev.width || ey >= dev.height || sx > ex || sy > ey) {
        ESP_LOGE(TAG, "Invalid fill area: sx=%d, sy=%d, ex=%d, ey=%d", sx, sy, ex, ey);
        return;
    }
    uint16_t width = ex - sx + 1;
    uint16_t height = ey - sy + 1;
    uint32_t total_pixels = width * height;
    uint32_t buf_index = 0;
    uint16_t *buffer = static_cast<uint16_t *>(heap_caps_malloc(width * sizeof(uint16_t), MALLOC_CAP_INTERNAL));
    if (nullptr == buffer) { ESP_LOGE(TAG, "Memory for bitmap is not enough"); return; }
    for (uint16_t y_index = 0; y_index < height; y_index++) {
        for (uint16_t x_index = 0; x_index < width; x_index++) {
            if (buf_index < total_pixels) { buffer[x_index] = color[buf_index]; buf_index++; }
        }
        esp_lcd_panel_draw_bitmap(panelHandle, sx, sy + y_index, ex + 1, sy + y_index + 1, buffer);
    }
    drawPoint(ex, ey, color[total_pixels - 1]);
    heap_caps_free(buffer);
}

void Lcd::showMonoIcos(uint16_t x, uint16_t y, uint8_t width, uint8_t height,
                      uint8_t *icosbase, uint16_t color, uint16_t bkcolor) {
    uint8_t temp;
    uint16_t x0 = x;
    for (uint16_t i = 0; i < height; i++) {
        for (uint8_t n = 0; n < width / 8; n++) {
            temp = icosbase[i * (width / 8) + n];
            for (uint8_t t = 0; t < 8; t++) {
                if (temp & 0x80) drawPoint(x, y, color);
                else drawPoint(x, y, bkcolor);
                temp <<= 1; x++;
            }
        }
        x = x0; y++;
    }
}

void Lcd::drawPoint(uint16_t x, uint16_t y, uint16_t color) {
    esp_lcd_panel_draw_bitmap(panelHandle, x, y, x + 1, y + 1, &color);
}

void Lcd::displayDir(uint8_t dir) {
    setRotation(dir ? LCD_ROTATION_PORTRAIT : LCD_ROTATION_LANDSCAPE);
}

void Lcd::setRotation(uint8_t rot) {
    dev.dir = rot;
    switch (rot) {
    case LCD_ROTATION_PORTRAIT:          /* 竖屏 240x280 */
        dev.width = dev.pwidth; dev.height = dev.pheight;
        esp_lcd_panel_swap_xy(panelHandle, false);
        esp_lcd_panel_mirror(panelHandle, false, false);
        break;
    case LCD_ROTATION_LANDSCAPE:         /* 横屏 280x240 */
        dev.width = dev.pheight; dev.height = dev.pwidth;
        esp_lcd_panel_swap_xy(panelHandle, true);
        esp_lcd_panel_mirror(panelHandle, false, true);
        break;
    case LCD_ROTATION_PORTRAIT_INV:      /* 竖屏倒置 240x280 */
        dev.width = dev.pwidth; dev.height = dev.pheight;
        esp_lcd_panel_swap_xy(panelHandle, false);
        esp_lcd_panel_mirror(panelHandle, true, true);
        break;
    case LCD_ROTATION_LANDSCAPE_INV:     /* 横屏倒置 280x240 */
        dev.width = dev.pheight; dev.height = dev.pwidth;
        esp_lcd_panel_swap_xy(panelHandle, true);
        esp_lcd_panel_mirror(panelHandle, true, false);
        break;
    }
    lcd_dev = dev;

    /* 旋转后清屏 */
    clear(WHITE);

    /* 同步触摸坐标系 */
    cst816s_set_rotation(rot);

    ESP_LOGI(TAG, "Rotation set to %d: %dx%d", rot, dev.width, dev.height);
}

void Lcd::drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    int xerr = 0, yerr = 0, delta_x = x2 - x1, delta_y = y2 - y1, incx, incy;
    uint16_t row = x1, col = y1;
    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }
    int distance = (delta_x > delta_y) ? delta_x : delta_y;
    for (uint16_t t = 0; t <= distance + 1; t++) {
        drawPoint(row, col, color);
        xerr += delta_x; yerr += delta_y;
        if (xerr > distance) { xerr -= distance; row += incx; }
        if (yerr > distance) { yerr -= distance; col += incy; }
    }
}

void Lcd::drawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color) {
    if (len == 0 || x >= dev.width || y >= dev.height) return;

    uint16_t ex = (x + len - 1 < dev.width) ? (x + len - 1) : (dev.width - 1);
    uint16_t width = ex - x + 1;

    uint16_t *buffer = static_cast<uint16_t *>(
        heap_caps_malloc(width * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (nullptr == buffer) {
        for (uint16_t t = 0; t < len; t++) drawPoint(x + t, y, color);
        return;
    }

    for (uint16_t i = 0; i < width; i++) buffer[i] = color;
    refreshDone = false;
    esp_lcd_panel_draw_bitmap(panelHandle, x, y, ex + 1, y + 1, buffer);
    while (!refreshDone) { vTaskDelay(1); }
    heap_caps_free(buffer);
}

void Lcd::drawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color) {
    int a = 0, b = r, di = 3 - (r << 1);
    while (a <= b) {
        drawPoint(x0 + a, y0 - b, color); drawPoint(x0 + b, y0 - a, color);
        drawPoint(x0 + b, y0 + a, color); drawPoint(x0 + a, y0 + b, color);
        drawPoint(x0 - a, y0 + b, color); drawPoint(x0 - b, y0 + a, color);
        drawPoint(x0 - b, y0 - a, color); drawPoint(x0 - a, y0 - b, color);
        a++;
        if (di < 0) di += 4 * a + 6;
        else { di += 10 + 4 * (a - b); b--; }
    }
}

void Lcd::showChar(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color) {
    const uint8_t *ch_code = nullptr;
    uint8_t ch_width, ch_height, ch_size;
    uint8_t ch_offset = static_cast<uint8_t>(chr) - ' ';
    uint16_t colortemp;

    switch (size) {
    case 12:
        ch_code = (const uint8_t *)asc2_1206[ch_offset];
        ch_width = 6; ch_height = 12;
        break;
    case 16:
        ch_code = (const uint8_t *)asc2_1608[ch_offset];
        ch_width = 8; ch_height = 16;
        break;
    case 24:
        ch_code = (const uint8_t *)asc2_2412[ch_offset];
        ch_width = 12; ch_height = 24;
        break;
    case 32:
        ch_code = (const uint8_t *)asc2_3216[ch_offset];
        ch_width = 16; ch_height = 32;
        break;
    default:
        return;
    }

    ch_size = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    uint16_t *pcolor = static_cast<uint16_t *>(
        heap_caps_malloc(ch_width * ch_height * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (nullptr == pcolor) {
        /* 内存不足时回退到逐像素绘制 */
        uint16_t y0 = y;
        for (uint8_t t = 0; t < ch_size; t++) {
            uint8_t temp = ch_code[t];
            for (uint8_t t1 = 0; t1 < 8; t1++) {
                if (temp & 0x80) drawPoint(x, y, color);
                else if (mode == 0) drawPoint(x, y, backColor);
                temp <<= 1; y++;
                if (y >= dev.height) return;
                if ((y - y0) == size) { y = y0; x++; if (x >= dev.width) return; break; }
            }
        }
        return;
    }

    /* 逐列式字库 → 行优先缓冲区 (逐像素填充) */
    uint8_t bytes_per_col = size / 8 + ((size % 8) ? 1 : 0);
    for (uint8_t byte_idx = 0; byte_idx < ch_size; byte_idx++) {
        uint8_t col = byte_idx / bytes_per_col;
        uint8_t row_base = (byte_idx % bytes_per_col) * 8;
        uint8_t byte_code = ch_code[byte_idx];

        for (uint8_t bit = 0; bit < 8; bit++) {
            uint8_t row = row_base + bit;
            if (row >= ch_height) break;

            if (byte_code & 0x80)
                colortemp = color;
            else if (mode == 0)
                colortemp = backColor;
            else
                colortemp = 0;  /* 叠加模式不绘制背景 */

            if (mode != 0 && !(byte_code & 0x80)) {
                byte_code <<= 1;
                continue;  /* 叠加模式跳过背景像素 */
            }

            pcolor[row * ch_width + col] = colortemp;
            byte_code <<= 1;
        }
    }

    refreshDone = false;
    esp_lcd_panel_draw_bitmap(panelHandle, x, y, x + ch_width, y + ch_height, pcolor);
    while (!refreshDone) { vTaskDelay(1); }
    heap_caps_free(pcolor);
}

void Lcd::showString(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     uint8_t size, const char *p, uint16_t color) {
    uint16_t x0 = x;
    width += x;
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

lcd_obj_t& Lcd::getDev() { return dev; }
esp_lcd_panel_handle_t Lcd::getPanelHandle() { return panelHandle; }

lcd_obj_t lcd_dev = {};
esp_lcd_panel_handle_t panel_handle = nullptr;
uint32_t g_back_color = 0xFFFF;

extern "C" {

static MySpi g_spi;
static Lcd g_lcd(g_spi);

void lcd_init(lcd_cfg_t lcd_config) {
    g_spi.init();
    g_lcd.init(lcd_config);
}
void lcd_clear(uint16_t color) { g_lcd.clear(color); }
void lcd_display_dir(uint8_t dir) { g_lcd.displayDir(dir); }
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color) { g_lcd.drawPoint(x, y, color); }
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color) { g_lcd.fill(sx, sy, ex, ey, color); }
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color) { g_lcd.colorFill(sx, sy, ex, ey, color); }
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) { g_lcd.drawLine(x1, y1, x2, y2, color); }
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color) { g_lcd.drawHLine(x, y, len, color); }
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color) { g_lcd.drawCircle(x0, y0, r, color); }
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color) { g_lcd.showChar(x, y, chr, size, mode, color); }

void lcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    g_lcd.drawHLine(x0, y0, x1 - x0 + 1, color);
    g_lcd.drawHLine(x0, y1, x1 - x0 + 1, color);
    g_lcd.drawLine(x0, y0, x0, y1, color);
    g_lcd.drawLine(x1, y0, x1, y1, color);
}

void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color) {
    uint8_t t, temp, enshow = 0;
    for (t = 0; t < len; t++) {
        temp = (num / static_cast<uint32_t>(pow(10, len - t - 1))) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                g_lcd.showChar(x + (size / 2) * t, y, ' ', size, 0, color);
                continue;
            } else enshow = 1;
        }
        g_lcd.showChar(x + (size / 2) * t, y, temp + '0', size, 0, color);
    }
}

void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color) {
    uint8_t t, temp;
    for (t = 0; t < len; t++) {
        temp = (num / static_cast<uint32_t>(pow(10, len - t - 1))) % 10;
        g_lcd.showChar(x + (size / 2) * t, y, temp + '0', size, mode, color);
    }
}

void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, const char *p, uint16_t color) {
    g_lcd.showString(x, y, width, height, size, p, color);
}

void lcd_app_show_mono_icos(uint16_t x, uint16_t y, uint8_t width, uint8_t height,
                            uint8_t *icosbase, uint16_t color, uint16_t bkcolor) {
    g_lcd.showMonoIcos(x, y, width, height, icosbase, color, bkcolor);
}

}
