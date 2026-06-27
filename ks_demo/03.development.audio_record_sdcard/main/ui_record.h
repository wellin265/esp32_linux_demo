#ifndef UI_RECORD_H
#define UI_RECORD_H

#include "lvgl.h"
#include "esp_err.h"

typedef enum {
    idle,
    record,
    play
} record_mode_t;

extern record_mode_t mode;
extern uint32_t audio_index;
extern uint8_t *audio_buffer;
extern uint32_t audio_total;  // 添加音频总长度声明

#define BUF_SIZE (16000 * 20 * sizeof(uint8_t))

void ui_record(void);

#endif