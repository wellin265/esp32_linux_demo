/*
 * @Author: Kevincoooool
 * @Date: 2022-01-12 09:48:30
 * @Description: 
 * @version:  
 * @Filename: Do not Edit
 * @LastEditTime: 2024-12-25 13:32:14
 * @FilePath: \17.avi_recorder\main\avi_recorder.h
 */
#ifndef _AVI_PRECESS_H_
#define _AVI_PRECESS_H_

#include "esp_log.h"
#include "esp_camera.h"

#ifdef __cplusplus 
extern "C" {
#endif

void avi_recorder_start(const char *fname, framesize_t rec_size, uint32_t rec_time, bool block);
void avi_recorder_stop(void);


#ifdef __cplusplus 
}
#endif

#endif