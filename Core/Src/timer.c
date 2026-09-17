/*
 * timer.c
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#include "timer.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

void timer_init(void)
{
    HAL_TIM_Base_Start(&htim1);
}

uint32_t timer_get_tick(void)
{
    return HAL_GetTick();
}

bool timer_expired(uint32_t *last_tick,
                   uint32_t interval)
{
    uint32_t now = timer_get_tick();

    if((now - *last_tick) >= interval)
    {
        *last_tick = now;
        return true;
    }

    return false;
}

bool timer_timeout(uint32_t *tick,
                   uint32_t timeout)
{
    if(tick == 0)
    {
        return false;
    }

    return (uint32_t)(timer_get_tick() - *tick) >= timeout;
}

void timer_reset(uint32_t *last_tick)
{
    if(last_tick != 0)
    {
        *last_tick = timer_get_tick();
    }
}
