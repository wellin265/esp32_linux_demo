#include "who_motion_detection.hpp"

#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "motion_detection";

static QueueHandle_t s_frame_in = NULL;
static QueueHandle_t s_frame_out = NULL;
static QueueHandle_t s_event_queue = NULL;
static QueueHandle_t s_result_queue = NULL;
static bool s_enabled = true;

static inline uint8_t rgb565_r(uint16_t p) { return ((p >> 11) & 0x1F) << 3; }
static inline uint8_t rgb565_g(uint16_t p) { return ((p >> 5) & 0x3F) << 2; }
static inline uint8_t rgb565_b(uint16_t p) { return (p & 0x1F) << 3; }

static uint32_t get_motion_score(const uint16_t *frame1, const uint16_t *frame2, int pixel_count)
{
    uint32_t moving_points = 0;
    for (int i = 0; i < pixel_count; ++i) {
        int dr = abs((int)rgb565_r(frame1[i]) - (int)rgb565_r(frame2[i]));
        int dg = abs((int)rgb565_g(frame1[i]) - (int)rgb565_g(frame2[i]));
        int db = abs((int)rgb565_b(frame1[i]) - (int)rgb565_b(frame2[i]));
        if ((dr + dg + db) > 80) {
            ++moving_points;
        }
    }
    return moving_points;
}

static void draw_motion_mark(camera_fb_t *frame)
{
    uint16_t *pixels = reinterpret_cast<uint16_t *>(frame->buf);
    const int box_w = 20;
    const int box_h = 20;
    for (int y = 0; y < box_h; ++y) {
        for (int x = 0; x < box_w; ++x) {
            pixels[y * frame->width + x] = 0xF800;
        }
    }
}

static void process_task(void *arg)
{
    camera_fb_t *frame1 = NULL;
    camera_fb_t *frame2 = NULL;

    while (true) {
        if (!s_enabled) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        bool moved = false;
        if (xQueueReceive(s_frame_in, &frame1, portMAX_DELAY) == pdTRUE &&
            xQueueReceive(s_frame_in, &frame2, portMAX_DELAY) == pdTRUE) {
            uint32_t moving_points = get_motion_score(reinterpret_cast<uint16_t *>(frame1->buf),
                                                      reinterpret_cast<uint16_t *>(frame2->buf),
                                                      frame1->width * frame1->height);

            if (moving_points > 2000) {
                ESP_LOGI(TAG, "Motion detected: %lu", static_cast<unsigned long>(moving_points));
                draw_motion_mark(frame2);
                moved = true;
            }

            esp_camera_fb_return(frame1);
            if (s_frame_out) {
                xQueueSend(s_frame_out, &frame2, portMAX_DELAY);
            } else {
                esp_camera_fb_return(frame2);
            }

            if (s_result_queue) {
                xQueueSend(s_result_queue, &moved, portMAX_DELAY);
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

void register_motion_detection(QueueHandle_t frame_i, QueueHandle_t event, QueueHandle_t result, QueueHandle_t frame_o)
{
    s_frame_in = frame_i;
    s_frame_out = frame_o;
    s_event_queue = event;
    s_result_queue = result;

    xTaskCreatePinnedToCore(process_task, TAG, 4 * 1024, NULL, 5, NULL, 1);
    if (s_event_queue) {
        xTaskCreatePinnedToCore(event_task, "motion_event", 3 * 1024, NULL, 5, NULL, 1);
    }
}
