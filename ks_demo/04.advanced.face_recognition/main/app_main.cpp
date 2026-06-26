#include "esp_camera.h"
#include "esp_check.h"
#include "esp_spiffs.h"

#include "app_ai_config.h"
#include "camera_lvgl_view.h"
#include "ksdiy_lvgl_port.h"
#include "who_camera.h"
#include "who_human_face_detection.hpp"
#include "who_human_face_recognition.hpp"
#include "who_cat_face_detection.hpp"
#include "who_color_detection.hpp"
#include "who_motion_detection.hpp"

namespace {

constexpr uint32_t kEnabledFeatures = APP_AI_ENABLED_FEATURES;
constexpr uint32_t kSingleFramePipelineMask =
    APP_AI_FEATURE_HUMAN_FACE_DETECT |
    APP_AI_FEATURE_HUMAN_FACE_RECOGNITION |
    APP_AI_FEATURE_CAT_FACE_DETECT |
    APP_AI_FEATURE_COLOR_DETECT;

QueueHandle_t create_frame_queue(size_t depth)
{
    return xQueueCreate(depth, sizeof(camera_fb_t *));
}

esp_err_t init_face_db_storage_if_needed()
{
#if APP_AI_ENABLE_HUMAN_FACE_RECOGNITION
    if ((kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_RECOGNITION) == 0) {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 4,
        .format_if_mount_failed = true,
    };
    return esp_vfs_spiffs_register(&conf);
#else
    return ESP_OK;
#endif
}

void lvgl_display_task(void *arg)
{
    auto frame_queue = static_cast<QueueHandle_t>(arg);
    while (true) {
        camera_fb_t *frame = nullptr;
        if (xQueueReceive(frame_queue, &frame, portMAX_DELAY) == pdTRUE && frame != nullptr) {
            camera_lvgl_view_update(frame->buf, frame->width, frame->height);
            esp_camera_fb_return(frame);
        }
    }
}

void register_selected_ai(QueueHandle_t ai_frame_queue, QueueHandle_t display_frame_queue)
{
    if (kEnabledFeatures == 0) {
        register_camera(PIXFORMAT_RGB565, FRAMESIZE_QVGA, 2, display_frame_queue);
        return;
    }

    if ((kEnabledFeatures & APP_AI_FEATURE_MOTION_DETECT) != 0) {
        ESP_ERROR_CHECK((kEnabledFeatures & ~APP_AI_FEATURE_MOTION_DETECT) == 0 ? ESP_OK : ESP_ERR_NOT_SUPPORTED);
#if APP_AI_ENABLE_MOTION_DETECT
        register_motion_detection(ai_frame_queue, NULL, NULL, display_frame_queue);
        register_camera(PIXFORMAT_RGB565, FRAMESIZE_QVGA, 2, ai_frame_queue);
        return;
#else
        ESP_ERROR_CHECK(ESP_ERR_NOT_SUPPORTED);
#endif
    }

    QueueHandle_t pipeline_input = ai_frame_queue;
    QueueHandle_t pipeline_output = display_frame_queue;

    auto stage_count = 0;
    if ((kEnabledFeatures & kSingleFramePipelineMask) != 0) {
        stage_count += ((kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_DETECT) != 0);
        stage_count += ((kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_RECOGNITION) != 0);
        stage_count += ((kEnabledFeatures & APP_AI_FEATURE_CAT_FACE_DETECT) != 0);
        stage_count += ((kEnabledFeatures & APP_AI_FEATURE_COLOR_DETECT) != 0);
    }

    ESP_ERROR_CHECK(stage_count > 0 ? ESP_OK : ESP_ERR_NOT_SUPPORTED);

    auto acquire_next_output_queue = [&](int stage_index) -> QueueHandle_t {
        return (stage_index == stage_count - 1) ? pipeline_output : create_frame_queue(2);
    };

    int stage_index = 0;

#if APP_AI_ENABLE_HUMAN_FACE_DETECT
    if ((kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_DETECT) != 0) {
        QueueHandle_t next_queue = acquire_next_output_queue(stage_index++);
        ESP_ERROR_CHECK(next_queue ? ESP_OK : ESP_ERR_NO_MEM);
        register_human_face_detection(pipeline_input, NULL, NULL, next_queue, false);
        pipeline_input = next_queue;
    }
#endif

#if APP_AI_ENABLE_HUMAN_FACE_RECOGNITION
    if ((kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_RECOGNITION) != 0) {
        QueueHandle_t next_queue = acquire_next_output_queue(stage_index++);
        ESP_ERROR_CHECK(next_queue ? ESP_OK : ESP_ERR_NO_MEM);
        register_human_face_recognition(pipeline_input, NULL, NULL, next_queue, false);
        pipeline_input = next_queue;
    }
#endif

#if APP_AI_ENABLE_CAT_FACE_DETECT
    if ((kEnabledFeatures & APP_AI_FEATURE_CAT_FACE_DETECT) != 0) {
        QueueHandle_t next_queue = acquire_next_output_queue(stage_index++);
        ESP_ERROR_CHECK(next_queue ? ESP_OK : ESP_ERR_NO_MEM);
        register_cat_face_detection(pipeline_input, NULL, NULL, next_queue, false);
        pipeline_input = next_queue;
    }
#endif

#if APP_AI_ENABLE_COLOR_DETECT
    if ((kEnabledFeatures & APP_AI_FEATURE_COLOR_DETECT) != 0) {
        QueueHandle_t next_queue = acquire_next_output_queue(stage_index++);
        ESP_ERROR_CHECK(next_queue ? ESP_OK : ESP_ERR_NO_MEM);
        register_color_detection(pipeline_input, NULL, NULL, next_queue, false);
        pipeline_input = next_queue;
    }
#endif

    register_camera(PIXFORMAT_RGB565, FRAMESIZE_QVGA, 2, ai_frame_queue);
}

} // namespace

extern "C" void app_main()
{
    static_assert(
        (kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_DETECT) == 0 || APP_AI_ENABLE_HUMAN_FACE_DETECT,
        "Enable human face detection or remove it from APP_AI_ENABLED_FEATURES.");
    static_assert(
        (kEnabledFeatures & APP_AI_FEATURE_HUMAN_FACE_RECOGNITION) == 0 || APP_AI_ENABLE_HUMAN_FACE_RECOGNITION,
        "Enable human face recognition or remove it from APP_AI_ENABLED_FEATURES.");
    static_assert(
        (kEnabledFeatures & APP_AI_FEATURE_CAT_FACE_DETECT) == 0 || APP_AI_ENABLE_CAT_FACE_DETECT,
        "Enable cat face detection or remove it from APP_AI_ENABLED_FEATURES.");
    static_assert(
        (kEnabledFeatures & APP_AI_FEATURE_COLOR_DETECT) == 0 || APP_AI_ENABLE_COLOR_DETECT,
        "Enable color detection or remove it from APP_AI_ENABLED_FEATURES.");
    static_assert(
        (kEnabledFeatures & APP_AI_FEATURE_MOTION_DETECT) == 0 || APP_AI_ENABLE_MOTION_DETECT,
        "Enable motion detection or remove it from APP_AI_ENABLED_FEATURES.");
    static_assert(
        (kEnabledFeatures & APP_AI_FEATURE_MOTION_DETECT) == 0 ||
            (kEnabledFeatures & ~APP_AI_FEATURE_MOTION_DETECT) == 0,
        "APP_AI_FEATURE_MOTION_DETECT currently supports standalone mode only.");

    ESP_ERROR_CHECK(init_face_db_storage_if_needed());

    ksdiy_lvgl_port_init();
    ESP_ERROR_CHECK(camera_lvgl_view_init(320, 240));

    QueueHandle_t ai_frame_queue = create_frame_queue(2);
    QueueHandle_t display_frame_queue = create_frame_queue(2);
    ESP_ERROR_CHECK(ai_frame_queue ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(display_frame_queue ? ESP_OK : ESP_ERR_NO_MEM);

    register_selected_ai(ai_frame_queue, display_frame_queue);

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != nullptr) {
        sensor->set_vflip(sensor, 1);
        sensor->set_hmirror(sensor, 0);
    }

    xTaskCreatePinnedToCore(lvgl_display_task, "lvgl_display", 4 * 1024, display_frame_queue, 5, NULL, 1);
}
