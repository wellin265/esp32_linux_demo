#include "ksdiy_lvgl_port.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"

static const char *TAG = "ksdiy_lvgl_port";

#define KSDIY_LCD_HOST SPI2_HOST
#define KSDIY_LCD_H_RES 280
#define KSDIY_LCD_V_RES 240
#define KSDIY_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
#define KSDIY_LCD_CMD_BITS 8
#define KSDIY_LCD_PARAM_BITS 8
#define KSDIY_LCD_PIN_SCLK 1
#define KSDIY_LCD_PIN_MOSI 0
#define KSDIY_LCD_PIN_MISO -1
#define KSDIY_LCD_PIN_DC 2
#define KSDIY_LCD_PIN_RST -1
#define KSDIY_LCD_PIN_CS 46
#define KSDIY_LCD_PIN_BK_LIGHT -1
#define KSDIY_TOUCH_I2C_PORT I2C_NUM_1
#define KSDIY_TOUCH_I2C_SCL 18
#define KSDIY_TOUCH_I2C_SDA 17
#define KSDIY_TOUCH_I2C_SPEED_HZ 400000
#define KSDIY_IO_EXPANDER_ADDR_TCA9554 ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000
#define KSDIY_IO_EXPANDER_ADDR_TCA9554A ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000
#define KSDIY_LCD_X_OFFSET 20

static bool s_initialized;
static lv_display_t *s_display;
static i2c_master_bus_handle_t s_i2c_bus;

static void ksdiy_i2c_init(void)
{
    if (s_i2c_bus != NULL) {
        return;
    }

    const i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = KSDIY_TOUCH_I2C_PORT,
        .sda_io_num = KSDIY_TOUCH_I2C_SDA,
        .scl_io_num = KSDIY_TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &s_i2c_bus));
}

static esp_io_expander_handle_t ksdiy_io_expander_init(void)
{
    esp_io_expander_handle_t io_expander = NULL;
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(s_i2c_bus, KSDIY_IO_EXPANDER_ADDR_TCA9554, &io_expander);
    if (ret != ESP_OK) {
        ret = esp_io_expander_new_i2c_tca9554(s_i2c_bus, KSDIY_IO_EXPANDER_ADDR_TCA9554A, &io_expander);
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1));
    return io_expander;
}

bool ksdiy_lvgl_lock(int timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
}

void ksdiy_lvgl_unlock(void)
{
    esp_lv_adapter_unlock();
}

lv_display_t *ksdiy_lvgl_get_display(void)
{
    return s_display;
}

void ksdiy_lvgl_port_init(void)
{
    if (s_initialized) {
        return;
    }

    ksdiy_i2c_init();
    (void) ksdiy_io_expander_init();

    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = KSDIY_LCD_PIN_SCLK,
        .mosi_io_num = KSDIY_LCD_PIN_MOSI,
        .miso_io_num = KSDIY_LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = KSDIY_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(KSDIY_LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = KSDIY_LCD_PIN_DC,
        .cs_gpio_num = KSDIY_LCD_PIN_CS,
        .pclk_hz = KSDIY_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = KSDIY_LCD_CMD_BITS,
        .lcd_param_bits = KSDIY_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)KSDIY_LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = KSDIY_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    const esp_lv_adapter_display_config_t disp_cfg = {
        .panel = panel_handle,
        .panel_io = io_handle,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER,
            .rotation = ESP_LV_ADAPTER_ROTATE_0,
            .hor_res = KSDIY_LCD_H_RES,
            .ver_res = KSDIY_LCD_V_RES,
            .buffer_height = KSDIY_LCD_H_RES * 40,
            .use_psram = false,
            .enable_ppa_accel = false,
            .require_double_buffer = true,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
    };
    s_display = esp_lv_adapter_register_display(&disp_cfg);
    if (s_display == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL display");
        abort();
    }
    lv_display_set_offset(s_display, KSDIY_LCD_X_OFFSET, 0);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_config.scl_speed_hz = KSDIY_TOUCH_I2C_SPEED_HZ;

#if ((ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR >= 4) || ESP_IDF_VERSION_MAJOR > 5)
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(s_i2c_bus, &tp_io_config, &tp_io_handle));
#else
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2((esp_lcd_i2c_bus_handle_t) s_i2c_bus, &tp_io_config, &tp_io_handle));
#endif

    esp_lcd_touch_handle_t touch_handle = NULL;
    const esp_lcd_touch_config_t touch_config = {
        .x_max = KSDIY_LCD_H_RES,
        .y_max = KSDIY_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &touch_config, &touch_handle));

    const esp_lv_adapter_touch_config_t touch_cfg = {
        .disp = s_display,
        .handle = touch_handle,
        .scale = {
            .x = 1.0f,
            .y = 1.0f,
        },
    };
    ESP_ERROR_CHECK(esp_lv_adapter_register_touch(&touch_cfg) == NULL ? ESP_FAIL : ESP_OK);

    esp_lv_adapter_start();
    s_initialized = true;
}
