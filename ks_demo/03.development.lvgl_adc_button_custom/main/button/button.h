
#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "bsp_button.h"

enum
{
    BT_NONE,
    BT1_DOWN,
    BT1_DOUBLE,
    BT1_LONG,
    BT1_LONGFREE,

    BT2_DOWN,
    BT2_DOUBLE,
    BT2_LONG,
    BT2_LONGFREE,

    BT3_DOWN,
    BT3_DOUBLE,
    BT3_LONG,
    BT3_LONGFREE,

    BT4_DOWN,
    BT4_DOUBLE,
    BT4_LONG,
    BT4_LONGFREE,

    BT5_DOWN,
    BT5_DOUBLE,
    BT5_LONG,
    BT5_LONGFREE,

    BT6_DOWN,
    BT6_DOUBLE,
    BT6_LONG,
    BT6_LONGFREE,
};
extern uint8_t Button_Value;
extern char * Button_Tips[];
void Button_Init(void);

#endif
