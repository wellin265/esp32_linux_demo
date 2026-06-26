#include "ksdiy_display.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

static const char *TAG = "ksdiy_display";

#define KSDIY_LCD_HOST SPI2_HOST
#define KSDIY_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
#define KSDIY_LCD_CMD_BITS 8
#define KSDIY_LCD_PARAM_BITS 8

#define KSDIY_LCD_PIN_SCLK 1
#define KSDIY_LCD_PIN_MOSI 0
#define KSDIY_LCD_PIN_MISO -1
#define KSDIY_LCD_PIN_DC 2
#define KSDIY_LCD_PIN_RST -1
#define KSDIY_LCD_PIN_CS 46
#define KSDIY_LCD_PIN_BACKLIGHT -1

#define KSDIY_TOUCH_I2C_PORT I2C_NUM_1
#define KSDIY_TOUCH_I2C_SCL 18
#define KSDIY_TOUCH_I2C_SDA 17
#define KSDIY_TOUCH_I2C_SPEED_HZ 400000
#define KSDIY_TOUCH_ADDR 0x15
#define KSDIY_TOUCH_DATA_START_REG 0x02
#define KSDIY_TOUCH_CHIP_ID_REG 0xA7

#define KSDIY_LCD_X_GAP 20
#define KSDIY_LCD_Y_GAP 0

#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)

static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_panel_io_handle_t s_panel_io_handle;
static i2c_master_bus_handle_t s_touch_i2c_bus;
static i2c_master_dev_handle_t s_touch_dev_handle;
static bool s_lcd_initialized;
static bool s_touch_initialized;
static bool s_spi_bus_initialized;

static esp_err_t ksdiy_i2c_init(void)
{
    if (s_touch_i2c_bus != NULL) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = KSDIY_TOUCH_I2C_PORT,
        .sda_io_num = KSDIY_TOUCH_I2C_SDA,
        .scl_io_num = KSDIY_TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };

    return i2c_new_master_bus(&i2c_bus_cfg, &s_touch_i2c_bus);
}

static esp_err_t ksdiy_touch_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(s_touch_dev_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "Touch device not initialized");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch data buffer is null");

    return i2c_master_transmit_receive(s_touch_dev_handle, &reg, 1, data, len, -1);
}

static void ksdiy_io_expander_init(void)
{
    esp_io_expander_handle_t io_expander = NULL;

    if (esp_io_expander_new_i2c_tca9554(
            s_touch_i2c_bus, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554, &io_expander) != ESP_OK &&
        esp_io_expander_new_i2c_tca9554(
            s_touch_i2c_bus, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A, &io_expander) != ESP_OK) {
        ESP_LOGW(TAG, "IO expander not found");
        return;
    }

    ESP_ERROR_CHECK(esp_io_expander_set_dir(
        io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(
        io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1));
}

esp_err_t ksdiy_display_lcd_init(void)
{
    if (s_lcd_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ksdiy_i2c_init(), TAG, "I2C init failed");
    ksdiy_io_expander_init();

    if (KSDIY_LCD_PIN_BACKLIGHT >= 0) {
        const gpio_config_t bk_gpio_config = {
#if KSDIY_LCD_PIN_BACKLIGHT >= 0
            .pin_bit_mask = 1ULL << (uint32_t)KSDIY_LCD_PIN_BACKLIGHT,
#else
            .pin_bit_mask = 0,
#endif
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_config), TAG, "Backlight gpio config failed");
    }

    if (!s_spi_bus_initialized) {
        const spi_bus_config_t buscfg = {
            .sclk_io_num = KSDIY_LCD_PIN_SCLK,
            .mosi_io_num = KSDIY_LCD_PIN_MOSI,
            .miso_io_num = KSDIY_LCD_PIN_MISO,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = KSDIY_LCD_H_RES * 80 * sizeof(uint16_t),
        };
        ESP_RETURN_ON_ERROR(
            spi_bus_initialize(KSDIY_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO),
            TAG,
            "SPI init failed");
        s_spi_bus_initialized = true;
    }

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = KSDIY_LCD_PIN_DC,
        .cs_gpio_num = KSDIY_LCD_PIN_CS,
        .pclk_hz = KSDIY_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = KSDIY_LCD_CMD_BITS,
        .lcd_param_bits = KSDIY_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)KSDIY_LCD_HOST, &io_config, &s_panel_io_handle),
        TAG,
        "New panel IO failed");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = KSDIY_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_st7789(s_panel_io_handle, &panel_config, &s_panel_handle),
        TAG,
        "New panel failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel_handle), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel_handle, true), TAG, "Panel swap_xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel_handle, false, true), TAG, "Panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel_handle, true), TAG, "Panel invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel_handle, KSDIY_LCD_X_GAP, KSDIY_LCD_Y_GAP), TAG, "Panel gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel_handle, true), TAG, "Panel display on failed");

    if (KSDIY_LCD_PIN_BACKLIGHT >= 0) {
        ESP_RETURN_ON_ERROR(gpio_set_level(KSDIY_LCD_PIN_BACKLIGHT, 1), TAG, "Backlight on failed");
    }

    s_lcd_initialized = true;
    ESP_LOGI(TAG, "LCD initialized");
    return ESP_OK;
}

esp_err_t ksdiy_display_draw_bitmap(const void *data, uint16_t width, uint16_t height)
{
    ESP_RETURN_ON_FALSE(s_lcd_initialized, ESP_ERR_INVALID_STATE, TAG, "LCD not initialized");
    ESP_RETURN_ON_FALSE(width == KSDIY_LCD_H_RES && height == KSDIY_LCD_V_RES,
                        ESP_ERR_INVALID_SIZE, TAG, "Unexpected frame size %ux%u", width, height);

    return esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, width, height, data);
}

esp_err_t ksdiy_display_touch_init(void)
{
    if (s_touch_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ksdiy_i2c_init(), TAG, "I2C init failed");
    ksdiy_io_expander_init();

    if (s_touch_dev_handle == NULL) {
        const i2c_device_config_t touch_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = KSDIY_TOUCH_ADDR,
            .scl_speed_hz = KSDIY_TOUCH_I2C_SPEED_HZ,
        };
        ESP_RETURN_ON_ERROR(
            i2c_master_bus_add_device(s_touch_i2c_bus, &touch_dev_cfg, &s_touch_dev_handle),
            TAG,
            "Touch add device failed");
    }

    uint8_t chip_id = 0;
    ESP_RETURN_ON_ERROR(ksdiy_touch_read_reg(KSDIY_TOUCH_CHIP_ID_REG, &chip_id, 1), TAG, "Touch probe failed");
    ESP_LOGI(TAG, "Touch initialized, chip id: 0x%02X", chip_id);

    s_touch_initialized = true;
    return ESP_OK;
}

bool ksdiy_display_touch_ready(void)
{
    return s_touch_initialized && s_touch_dev_handle != NULL;
}

esp_err_t ksdiy_display_touch_read(
    uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_count, uint8_t max_points)
{
    typedef struct {
        uint8_t num;
        uint8_t x_h : 4;
        uint8_t reserved0 : 4;
        uint8_t x_l;
        uint8_t y_h : 4;
        uint8_t reserved1 : 4;
        uint8_t y_l;
    } touch_point_t;

    ESP_RETURN_ON_FALSE(s_touch_initialized, ESP_ERR_INVALID_STATE, TAG, "Touch not initialized");
    ESP_RETURN_ON_FALSE(x != NULL && y != NULL && point_count != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch buffers are null");
    ESP_RETURN_ON_FALSE(max_points > 0, ESP_ERR_INVALID_ARG, TAG, "max_points must be > 0");

    touch_point_t point = {0};
    ESP_RETURN_ON_ERROR(
        ksdiy_touch_read_reg(KSDIY_TOUCH_DATA_START_REG, (uint8_t *)&point, sizeof(point)),
        TAG,
        "Touch read failed");

    uint8_t points = point.num > 0 ? 1 : 0;
    if (points > max_points) {
        points = max_points;
    }
    *point_count = points;
    if (points == 0) {
        return ESP_OK;
    }

    uint16_t raw_x = (uint16_t)((point.x_h << 8) | point.x_l);
    uint16_t raw_y = (uint16_t)((point.y_h << 8) | point.y_l);

    /* Match the transform used in your LVGL port: mirror_y, then swap_xy. */
    x[0] = (uint16_t)(KSDIY_LCD_V_RES - raw_y);
    y[0] = raw_x;

    if (strength) {
        strength[0] = 1;
    }
    if (track_id) {
        track_id[0] = 0;
    }

    return ESP_OK;
}
