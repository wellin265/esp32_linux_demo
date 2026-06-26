#include "who_human_face_detection.hpp"

#include <vector>

#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "dl_image_draw.hpp"
#include "esp_camera.h"
#include "esp_log.h"
#include "human_face_detect.hpp"

static const char *TAG = "human_face_detection";

static QueueHandle_t xQueueFrameI = NULL;
static QueueHandle_t xQueueEvent = NULL;
static QueueHandle_t xQueueFrameO = NULL;
static QueueHandle_t xQueueResult = NULL;

static bool gEvent = true;
static bool gReturnFB = true;

namespace {

constexpr uint8_t kLineWidth = 2;
constexpr uint8_t kPointRadius = 3;

const std::vector<uint8_t> kBoxColor565 = {0xE0, 0x07};
const std::vector<uint8_t> kLeftMarks565 = {0x00, 0xF8};
const std::vector<uint8_t> kNoseMark565 = {0xE0, 0x07};
const std::vector<uint8_t> kRightMarks565 = {0x1F, 0x00};

dl::image::img_t make_rgb565_image(camera_fb_t *frame)
{
    return {
        .data = frame->buf,
        .width = static_cast<uint16_t>(frame->width),
        .height = static_cast<uint16_t>(frame->height),
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE,
    };
}

void draw_detection_result(const dl::image::img_t &img, std::list<dl::detect::result_t> &results)
{
    for (auto &prediction : results) {
        prediction.limit_box(img.width, img.height);
        dl::image::draw_hollow_rectangle(img,
                                         prediction.box[0],
                                         prediction.box[1],
                                         prediction.box[2],
                                         prediction.box[3],
                                         kBoxColor565,
                                         kLineWidth);

        if (prediction.keypoint.size() == 10) {
            prediction.limit_keypoint(img.width, img.height);
            dl::image::draw_point(img, prediction.keypoint[0], prediction.keypoint[1], kLeftMarks565, kPointRadius);
            dl::image::draw_point(img, prediction.keypoint[2], prediction.keypoint[3], kLeftMarks565, kPointRadius);
            dl::image::draw_point(img, prediction.keypoint[4], prediction.keypoint[5], kNoseMark565, kPointRadius);
            dl::image::draw_point(img, prediction.keypoint[6], prediction.keypoint[7], kRightMarks565, kPointRadius);
            dl::image::draw_point(img, prediction.keypoint[8], prediction.keypoint[9], kRightMarks565, kPointRadius);
        }
    }
}

void print_detection_result(std::list<dl::detect::result_t> &results)
{
    int index = 0;
    for (auto &prediction : results) {
        ESP_LOGI(TAG,
                 "[%d] score=%.3f box=(%d,%d,%d,%d)",
                 index++,
                 prediction.score,
                 prediction.box[0],
                 prediction.box[1],
                 prediction.box[2],
                 prediction.box[3]);
    }
}

} // namespace

static void task_process_handler(void *arg)
{
    camera_fb_t *frame = NULL;
    HumanFaceDetect detector(HumanFaceDetect::MSRMNP_S8_V1, false);
    ESP_LOGI(TAG, "Human face detection task started");

    while (true) {
        if (!gEvent) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        bool is_detected = false;
        if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY)) {
            dl::image::img_t img = make_rgb565_image(frame);
            std::list<dl::detect::result_t> &detect_results = detector.run(img);
            if (!detect_results.empty()) {
                draw_detection_result(img, detect_results);
                print_detection_result(detect_results);
                is_detected = true;
            }

            if (xQueueFrameO) {
                xQueueSend(xQueueFrameO, &frame, portMAX_DELAY);
            } else if (gReturnFB) {
                esp_camera_fb_return(frame);
            } else {
                free(frame);
            }

            if (xQueueResult) {
                xQueueSend(xQueueResult, &is_detected, portMAX_DELAY);
            }
        }
    }
}

static void task_event_handler(void *arg)
{
    while (true) {
        xQueueReceive(xQueueEvent, &gEvent, portMAX_DELAY);
    }
}

void register_human_face_detection(QueueHandle_t frame_i,
                                   QueueHandle_t event,
                                   QueueHandle_t result,
                                   QueueHandle_t frame_o,
                                   bool camera_fb_return)
{
    xQueueFrameI = frame_i;
    xQueueFrameO = frame_o;
    xQueueEvent = event;
    xQueueResult = result;
    gReturnFB = camera_fb_return;

    ESP_LOGI(TAG, "Register human face detection");

    xTaskCreatePinnedToCore(task_process_handler, TAG, 6 * 1024, NULL, 5, NULL, 0);
    if (xQueueEvent) {
        xTaskCreatePinnedToCore(task_event_handler, TAG, 4 * 1024, NULL, 5, NULL, 1);
    }
}
