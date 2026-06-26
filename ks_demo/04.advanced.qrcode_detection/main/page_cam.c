/**
 * @file page_cam.c
 * @brief Camera Preview LVGL Page
 *
 * Camera preview LVGL page with QR result display.
 *
 * @copyright Copyright (c) 2024 KSDIY
 * @license MIT
 */

#include "page_cam.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "app_camera.h"
#include "ksdiy_lvgl_port.h"

#include <esp_system.h>
#include "esp_log.h"
#include "esp_code_scanner.h"
#include "esp_heap_caps.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_timer.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "PAGE_CAM"
 camera_fb_t *fb;
lv_obj_t *img_cam; //要显示图像
lv_obj_t *label_detect;
static uint8_t *s_cam_frame_buffer;
lv_img_dsc_t img_dsc = {
	.header.w = 240,
	.header.h = 240,
	.data_size = 240 * 240 * 2,
	.header.cf = LV_COLOR_FORMAT_RGB565,
	.data = NULL,
};
void Cam_Task(void *pvParameters)
{


	int64_t time1, time2;
	// esp_image_scanner_t *esp_scn = esp_code_scanner_create();
	// esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, 320, 240};
	// esp_code_scanner_set_config(esp_scn, config);
	while (1)
	{

		static int64_t last_frame = 0;
		if (!last_frame)
		{
			last_frame = esp_timer_get_time();
		}
		fb = esp_camera_fb_get();//获取图像
		time1 = esp_timer_get_time();
		if (fb == NULL)
		{
			vTaskDelay(100);
			ESP_LOGE(TAG, "Get image failed!");
		}
		else
		{
			esp_image_scanner_t *esp_scn = esp_code_scanner_create();
			esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, fb->width, fb->height};
			esp_code_scanner_set_config(esp_scn, config);
			int decoded_num = esp_code_scanner_scan_image(esp_scn, fb->buf);

			if (ksdiy_lvgl_lock(20)) {
				if (decoded_num) {
					esp_code_scanner_symbol_t result = esp_code_scanner_result(esp_scn);
					time2 = esp_timer_get_time();
					ESP_LOGI(TAG, "Decode time in %lld ms.", (time2 - time1) / 1000);
					ESP_LOGI(TAG, "Decoded %s symbol \"%s\"\n", result.type_name, result.data);
					lv_label_set_text(label_detect, (char *)result.data);
				} else {
					lv_label_set_text(label_detect, "Nothing...");
				}
				ksdiy_lvgl_unlock();
			}
			esp_code_scanner_destroy(esp_scn);

			if (s_cam_frame_buffer == NULL) {
				ESP_LOGE(TAG, "Camera frame buffer is not allocated");
				esp_camera_fb_return(fb);
				vTaskDelay(pdMS_TO_TICKS(30));
				continue;
			}

			size_t frame_size = fb->width * fb->height * 2;
			for (size_t i = 0; i < frame_size; i += 2) {
				s_cam_frame_buffer[i] = fb->buf[i + 1];
				s_cam_frame_buffer[i + 1] = fb->buf[i];
			}

			img_dsc.header.w = fb->width;
			img_dsc.header.h = fb->height;
			img_dsc.data_size = frame_size;
			img_dsc.data = s_cam_frame_buffer;
			if (ksdiy_lvgl_lock(20)) {
				lv_img_set_src(img_cam, &img_dsc);
				ksdiy_lvgl_unlock();
			}

			esp_camera_fb_return(fb);

			int64_t fr_end = esp_timer_get_time();
			int64_t frame_time = fr_end - last_frame;
			last_frame = fr_end;
			frame_time /= 1000;
			ESP_LOGI("esp", "MJPG:  %ums (%.1ffps)", (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
		}

		vTaskDelay(pdMS_TO_TICKS(5));
	}

	// never reach
	while (1)
	{
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

void imgcam_init(void)
{
	if (s_cam_frame_buffer == NULL) {
		s_cam_frame_buffer = heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (s_cam_frame_buffer == NULL) {
			s_cam_frame_buffer = heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_8BIT);
		}
	}

	img_cam = lv_img_create(lv_scr_act());

	lv_obj_set_pos(img_cam, 0, 0);
	lv_obj_set_size(img_cam, 240, 240);

	label_detect = lv_label_create(lv_scr_act());
	/*Modify the Label's text*/

	lv_label_set_long_mode(label_detect, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(label_detect, 0, LV_VER_RES - 40);
	lv_obj_set_size(label_detect, LV_HOR_RES, 50);
	lv_label_set_recolor(label_detect, true);
	lv_obj_set_style_bg_color(label_detect, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(label_detect, LV_OPA_COVER, 0);
	lv_obj_set_style_text_color(label_detect, lv_color_black(), 0);
	lv_obj_set_style_pad_left(label_detect, 6, 0);
	lv_obj_set_style_pad_right(label_detect, 6, 0);
	lv_obj_set_style_pad_top(label_detect, 4, 0);
	lv_obj_set_style_pad_bottom(label_detect, 4, 0);
	lv_label_set_text(label_detect, "Hello!");
}
void page_cam_load()
{
	app_camera_init();//初始化摄像头

	imgcam_init();//初始化一个imgcam用于显示

	xTaskCreatePinnedToCore(&Cam_Task, "Cam_Task", 1024 * 5, NULL, 14, NULL, 0);
}
