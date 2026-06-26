
#ifndef _APP_ADC_H_
#define _APP_ADC_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#ifdef __cplusplus
extern "C"
{
#endif
extern char button_str[128];
extern uint8_t Button_Value;

void ADC_BUTTON_Init(void);
#ifdef __cplusplus
}
#endif
#endif