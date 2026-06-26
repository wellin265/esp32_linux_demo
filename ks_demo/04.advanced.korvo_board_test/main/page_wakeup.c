/**
 * @file page_wakeup.c
 * @brief Speech wakeup LVGL page helpers
 */

#include "page_wakeup.h"
#include "ksdiy_lvgl_port.h"

#include <esp_system.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

LV_FONT_DECLARE(myFont);

static lv_obj_t *label_speech = NULL;

void wakeup_init(void)
{
    if (!ksdiy_lvgl_is_ready())
    {
        return;
    }

    if (label_speech == NULL)
    {
        label_speech = lv_label_create(lv_scr_act());
        lv_obj_align(label_speech, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(label_speech, &myFont, 0);
        lv_obj_set_size(label_speech, 240, 50);
        lv_label_set_recolor(label_speech, true);
        lv_label_set_text(label_speech, "#0000ff 你好呀#\n我是酷世DIY ESP32S3");
    }
}

void wakeup_set_text(char *text)
{
    if (!ksdiy_lvgl_is_ready())
    {
        return;
    }

    if (label_speech)
    {
        lv_label_set_text(label_speech, text);
    }
}

void page_wakeup_load()
{
    wakeup_init();
}

void page_wakeup_end()
{
    if (!ksdiy_lvgl_is_ready())
    {
        return;
    }

    if (label_speech)
    {
        lv_obj_del(label_speech);
        label_speech = NULL;
    }
}

void page_wakeup_start()
{
    if (!ksdiy_lvgl_is_ready())
    {
        return;
    }

    printf("     esp_get_free_heap_size : %d  \n", esp_get_free_heap_size());
    printf("     esp_get_minimum_free_heap_size : %d  \n", esp_get_minimum_free_heap_size());
    page_wakeup_load();
}
