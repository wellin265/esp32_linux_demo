#include "who_cat_face_detection.hpp"

#include <list>

#include "cat_detect.hpp"
#include "dl_image_define.hpp"
#include "dl_image_draw.hpp"
#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "cat_face_detection";

static QueueHandle_t s_frame_in = NULL;
static QueueHandle_t s_frame_out = NULL;
static QueueHandle_t s_event_queue = NULL;
static QueueHandle_t s_result_queue = NULL;
static bool s_enabled = true;
static bool s_return_fb = true;

static dl::image::img_t make_rgb565_image(camera_fb_t *frame)
{
    return {
        .data = frame->buf,
        .width = static_cast<uint16_t>(frame->width),
        .height = static_cast<uint16_t>(frame->height),
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE,
    };
}

static void draw_results(const dl::image::img_t &img, std::list<dl::detect::result_t> &results)
{
    const std::vector<uint8_t> box_color = {0x1F, 0x00};
    for (auto &prediction : results) {
        prediction.limit_box(img.width, img.height);
        dl::image::draw_hollow_rectangle(
            img, prediction.box[0], prediction.box[1], prediction.box[2], prediction.box[3], box_color, 2);
    }
}

static void process_task(void *arg)
{
    CatDetect detector(CatDetect::ESPDET_PICO_224_224_CAT, false);

    while (true) {
        if (!s_enabled) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        camera_fb_t *frame = NULL;
        bool detected = false;
        if (xQueueReceive(s_frame_in, &frame, portMAX_DELAY) == pdTRUE && frame != NULL) {
            dl::image::img_t img = make_rgb565_image(frame);
            std::list<dl::detect::result_t> &results = detector.run(img);
            if (!results.empty()) {
                draw_results(img, results);
                ESP_LOGI(TAG, "Detected %d cat face(s)", static_cast<int>(results.size()));
                detected = true;
            }

            if (s_frame_out) {
                xQueueSend(s_frame_out, &frame, portMAX_DELAY);
            } else if (s_return_fb) {
                esp_camera_fb_return(frame);
            }

            if (s_result_queue) {
                xQueueSend(s_result_queue, &detected, portMAX_DELAY);
            }
        }
    }
}

static void event_task(void *arg)
{
    while (true) {
        xQueueReceive(s_event_queue, &s_enabled, portMAX_DELAY);
    }
}

void register_cat_face_detection(QueueHandle_t frame_i,
                                 QueueHandle_t event,
                                 QueueHandle_t result,
                                 QueueHandle_t frame_o,
                                 bool camera_fb_return)
{
    s_frame_in = frame_i;
    s_frame_out = frame_o;
    s_event_queue = event;
    s_result_queue = result;
    s_return_fb = camera_fb_return;

    xTaskCreatePinnedToCore(process_task, TAG, 6 * 1024, NULL, 5, NULL, 0);
    if (s_event_queue) {
        xTaskCreatePinnedToCore(event_task, "cat_face_event", 3 * 1024, NULL, 5, NULL, 1);
    }
}
