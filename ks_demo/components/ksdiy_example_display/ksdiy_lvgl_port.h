#pragma once

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ksdiy_lvgl_port_init(void);
bool ksdiy_lvgl_lock(int timeout_ms);
void ksdiy_lvgl_unlock(void);
lv_display_t *ksdiy_lvgl_get_display(void);

#ifdef __cplusplus
}
#endif
