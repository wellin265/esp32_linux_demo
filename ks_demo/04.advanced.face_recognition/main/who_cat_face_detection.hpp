#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

void register_cat_face_detection(QueueHandle_t frame_i,
                                 QueueHandle_t event,
                                 QueueHandle_t result,
                                 QueueHandle_t frame_o,
                                 bool camera_fb_return);

#ifdef __cplusplus
}
#endif
