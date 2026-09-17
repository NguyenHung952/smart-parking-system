/*
 * led.h
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#include "main.h"

#ifndef INC_LED_H_
#define INC_LED_H_

void led_on(void);
void led_off(void);
void led_init(void);
void led_set(uint8_t led_index, uint8_t on);

#endif /* INC_LED_H_ */

