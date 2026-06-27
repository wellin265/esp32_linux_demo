/**
 * @file page_cam.h
 * @brief Camera Preview Page Header
 *
 * Camera preview page create/destroy interface.
 *
 * @copyright Copyright (c) 2024 KSDIY
 * @license MIT
 */


#ifndef _page_cam_
#define _page_cam_

#ifdef __cplusplus
extern "C" {
#endif
/*********************
* INCLUDES
*********************/

#include "lvgl.h"

extern uint8_t cam_en, color_en, face_en;
void Cam_Task(void *pvParameters);
void page_cam_load(void);
void page_cam_end(void);

#ifdef __cplusplus
} /* extern "C" */
#endif




#endif // _TEST_


