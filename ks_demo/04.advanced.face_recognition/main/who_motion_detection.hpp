#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

void register_motion_detection(QueueHandle_t frame_i, QueueHandle_t event, QueueHandle_t result, QueueHandle_t frame_o);

#ifdef __cplusplus
}
#endif
