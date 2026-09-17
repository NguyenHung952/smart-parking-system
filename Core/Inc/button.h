/*
 * button.h
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#ifndef BUTTON_H
#define BUTTON_H

#include "main.h"
typedef enum
{
    BUTTON_START = 0,
    BUTTON_PAUSE = 1,
    BUTTON_STOP = 2,
    BUTTON_RESUME =3
} ButtonId;
uint8_t Read74HC165(void);
uint8_t ButtonPressed(uint8_t data, uint8_t sw);

#endif
