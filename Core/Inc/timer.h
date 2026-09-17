/*
 * timer.h
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#ifndef TIMER_H
#define TIMER_H

#include "main.h"
#include "stdbool.h"


void timer_init(void);
void timer_reset(uint32_t *tick);

bool timer_timeout(uint32_t *tick,
                   uint32_t timeout);
uint32_t timer_get_tick(void);

bool timer_expired(uint32_t *previous_tick,
                      uint32_t interval);

#endif
