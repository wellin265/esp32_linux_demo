#include "who_human_face_recognition.hpp"

#include <list>
#include <string>
#include <vector>

#include "app_ai_config.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "fb_gfx.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

static const char *TAG = "human_face_recog";

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

static void draw_face_box(camera_fb_t *frame, const dl::detect::result_t &face, uint32_t color)
{
    int x1 = face.box[0];
    int y1 = face.box[1];
    int x2 = face.box[2];
    int y2 = face.box[3];
    fb_gfx_drawFastHLine(frame, x1, y1, x2 - x1, color);
    fb_gfx_drawFastHLine(frame, x1, y2, x2 - x1, color);
    fb_gfx_drawFastVLine(frame, x1, y1, y2 - y1, color);
    fb_gfx_drawFastVLine(frame, x2, y1, y2 - y1, color);
}

static void process_task(void *arg)
{
    HumanFaceDetect detector(HumanFaceDetect::MSRMNP_S8_V1, false);
    HumanFaceRecognizer recognizer(APP_AI_RECOG_DB_PATH, HumanFaceFeat::MFN_S8_V1, false);

    while (true) {
        if (!s_enabled) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        camera_fb_t *frame = NULL;
        bool detected = false;
        if (xQueueReceive(s_frame_in, &frame, portMAX_DELAY) == pdTRUE && frame != NULL) {
            dl::image::img_t img = make_rgb565_image(frame);
            std::list<dl::detect::result_t> &faces = detector.run(img);

            if (!faces.empty()) {
                detected = true;
                if (recognizer.get_num_feats() == 0 && faces.size() == 1 && APP_AI_RECOG_AUTO_ENROLL_FIRST_FACE) {
                    if (recognizer.enroll(img, faces) == ESP_OK) {
                        ESP_LOGI(TAG, "Auto-enrolled first face as ID 1");
                        fb_gfx_print(frame, 8, 8, 0x07E0, "enrolled id=1");
                    }
                } else {
                    std::vector<dl::recognition::result_t> results = recognizer.recognize(img, faces);
                    size_t index = 0;
                    for (auto it = faces.begin(); it != faces.end(); ++it, ++index) {
                        draw_face_box(frame, *it, 0x07E0);
                        if (index < results.size()) {
                            char label[32];
                            snprintf(label, sizeof(label), "id=%u sim=%.2f", results[index].id, results[index].similarity);
                            fb_gfx_print(frame, it->box[0], it->box[1] > 16 ? it->box[1] - 16 : 0, 0x07E0, label);
                            ESP_LOGI(TAG,
                                     "Face recognized: id=%u similarity=%.3f",
                                     results[index].id,
                                     results[index].similarity);
                        }
                    }
                }
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

void register_human_face_recognition(QueueHandle_t frame_i,
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

    xTaskCreatePinnedToCore(process_task, TAG, 8 * 1024, NULL, 5, NULL, 0);
    if (s_event_queue) {
        xTaskCreatePinnedToCore(event_task, "human_face_recog_event", 3 * 1024, NULL, 5, NULL, 1);
    }
}
