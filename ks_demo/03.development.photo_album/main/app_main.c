/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "ksdiy_lvgl_port.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "lvgl.h"
#include "lv_demos.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_timer.h"
#endif
#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_jpeg_enc.h"

static const char *TAG = "main";

/************************* IMPORTANT2 ***************************
 * the actual image size must be smaller than JPG_IMAGE_MAX_SIZE */
#define JPG_IMAGE_MAX_SIZE (450 * 1024)

/*Initialize your Storage device and File system.*/
static esp_err_t spiffs_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS as demo assets storage.");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true // 挂载失败，可以先设置true格式化
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ret;
}

lv_obj_t *img_cam; // 要显示图像
lv_img_dsc_t img_dsc = {
    .header.always_zero = 0,
    .header.w = 800,
    .header.h = 480,
    .data_size = 800 * 480 * 2,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

extern const lv_img_dsc_t img_test;

static jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    int inbuf_consumed = 0;

    // Generate default configuration
    jpeg_dec_config_t config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB565_BE,
        .rotate = JPEG_ROTATE_0D,
    };

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK)
    {
        return ret;
    }

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL)
    {
        return JPEG_ERR_NO_MEM;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = (jpeg_dec_header_info_t *)calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL)
    {
        return JPEG_ERR_NO_MEM;
    }
    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0)
    {
        // Serial.println("JPEG decode parse failed");
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;
    inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + inbuf_consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0)
    {
        // Serial.println("JPEG decode process failed");
        goto _exit;
    }

_exit:
    // Decoder deinitialize
    if (jpeg_dec) {
        jpeg_dec_close(jpeg_dec);
    }
    free(out_info);
    free(jpeg_io);
    return ret;
}

void app_main(void)
{

    /* Initialize SPIFFS */
    spiffs_init();

    ksdiy_lvgl_port_init();

    if (ksdiy_lvgl_lock(0))
    {
        img_cam = lv_img_create(lv_scr_act());
        lv_obj_align(img_cam, LV_ALIGN_CENTER, 0, 0);
        ESP_LOGI(TAG, "init done");
        ksdiy_lvgl_unlock();
    }

    /* malloc a buffer for RGB565 data */
    uint8_t *lcd_buffer = (uint8_t *)heap_caps_malloc((480 * 800 * 2), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(lcd_buffer != NULL);

    uint8_t *jpeg_buf = (uint8_t *)heap_caps_malloc(100 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(jpeg_buf != NULL);

    size_t i = 1;
    char file_name[64] = {0};
    ESP_LOGI(TAG, "init done");

    while (1)
    {

        {
            if (i > 3)
                i = 1;
            sprintf(file_name, "/spiffs/r%03d.jpg", i);
        }

        FILE *fd = fopen(file_name, "r");

        int read_bytes = fread(jpeg_buf, 1, JPG_IMAGE_MAX_SIZE, fd);
        fclose(fd);
        esp_jpeg_decoder_one_image(jpeg_buf, read_bytes, lcd_buffer); // 使用乐鑫adf的jpg解码 速度快三倍
        img_dsc.data = (uint8_t *)lcd_buffer;
        lv_img_set_src(img_cam, &img_dsc);
        ESP_LOGD(TAG, "file_name: %s, fd: %p, read_bytes: %d, free_heap: %d", file_name, fd, read_bytes, esp_get_free_heap_size());

        vTaskDelay(2000 / portTICK_PERIOD_MS);

        i += 1;
    }

    free(jpeg_buf);
}
