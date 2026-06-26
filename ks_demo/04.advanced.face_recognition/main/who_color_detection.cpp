#include "who_color_detection.hpp"

#include <algorithm>
#include <array>

#include "esp_camera.h"
#include "esp_log.h"
#include "fb_gfx.h"

static const char *TAG = "color_detection";

static QueueHandle_t s_frame_in = NULL;
static QueueHandle_t s_frame_out = NULL;
static QueueHandle_t s_event_queue = NULL;
static QueueHandle_t s_result_queue = NULL;
static bool s_enabled = true;
static bool s_return_fb = true;

typedef struct {
    const char *name;
    uint16_t draw_color;
    bool (*match)(uint8_t r, uint8_t g, uint8_t b);
} color_profile_t;

static inline uint16_t rgb565_swap_bytes(uint16_t p) { return (uint16_t)((p << 8) | (p >> 8)); }
static inline uint8_t rgb565_r(uint16_t p) { return ((p >> 11) & 0x1F) << 3; }
static inline uint8_t rgb565_g(uint16_t p) { return ((p >> 5) & 0x3F) << 2; }
static inline uint8_t rgb565_b(uint16_t p) { return (p & 0x1F) << 3; }

static bool is_red(uint8_t r, uint8_t g, uint8_t b) { return r > 150 && r > g + 40 && r > b + 40; }
static bool is_green(uint8_t r, uint8_t g, uint8_t b) { return g > 120 && g > r + 20 && g > b + 20; }
static bool is_blue(uint8_t r, uint8_t g, uint8_t b) { return b > 120 && b > r + 20 && b > g + 10; }
static bool is_yellow(uint8_t r, uint8_t g, uint8_t b) { return r > 140 && g > 110 && b < 100; }

static const std::array<color_profile_t, 4> kProfiles = {{
    {"red", 0xF800, is_red},
    {"green", 0x07E0, is_green},
    {"blue", 0x001F, is_blue},
    {"yellow", 0xFFE0, is_yellow},
}};

static void event_task(void *arg)
{
    while (true) {
        xQueueReceive(s_event_queue, &s_enabled, portMAX_DELAY);
    }
}

static void process_task(void *arg)
{
    while (true) {
        if (!s_enabled) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        camera_fb_t *frame = NULL;
        bool found_any = false;
        if (xQueueReceive(s_frame_in, &frame, portMAX_DELAY) == pdTRUE && frame != NULL) {
            uint16_t *pixels = reinterpret_cast<uint16_t *>(frame->buf);
            for (const auto &profile : kProfiles) {
                int min_x = frame->width;
                int min_y = frame->height;
                int max_x = -1;
                int max_y = -1;
                int count = 0;

                for (int y = 0; y < frame->height; ++y) {
                    for (int x = 0; x < frame->width; ++x) {
                        uint16_t pixel = rgb565_swap_bytes(pixels[y * frame->width + x]);
                        uint8_t r = rgb565_r(pixel);
                        uint8_t g = rgb565_g(pixel);
                        uint8_t b = rgb565_b(pixel);
                        if (profile.match(r, g, b)) {
                            min_x = std::min(min_x, x);
                            min_y = std::min(min_y, y);
                            max_x = std::max(max_x, x);
                            max_y = std::max(max_y, y);
                            ++count;
                        }
                    }
                }

                if (count > 300 && max_x > min_x && max_y > min_y) {
                    fb_gfx_drawFastHLine(frame, min_x, min_y, max_x - min_x, profile.draw_color);
                    fb_gfx_drawFastHLine(frame, min_x, max_y, max_x - min_x, profile.draw_color);
                    fb_gfx_drawFastVLine(frame, min_x, min_y, max_y - min_y, profile.draw_color);
                    fb_gfx_drawFastVLine(frame, max_x, min_y, max_y - min_y, profile.draw_color);
                    fb_gfx_print(frame, min_x, std::max(0, min_y - 16), profile.draw_color, profile.name);
                    ESP_LOGI(TAG, "Detected color %s, pixels=%d", profile.name, count);
                    found_any = true;
                }
            }

            if (s_frame_out) {
                xQueueSend(s_frame_out, &frame, portMAX_DELAY);
            } else if (s_return_fb) {
                esp_camera_fb_return(frame);
            }

            if (s_result_queue) {
                xQueueSend(s_result_queue, &found_any, portMAX_DELAY);
            }
        }
    }
}

void register_color_detection(QueueHandle_t frame_i,
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

    xTaskCreatePinnedToCore(process_task, TAG, 5 * 1024, NULL, 5, NULL, 1);
    if (s_event_queue) {
        xTaskCreatePinnedToCore(event_task, "color_event", 3 * 1024, NULL, 5, NULL, 1);
    }
}
