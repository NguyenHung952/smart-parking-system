/*
 * buzzer.c
 *
 *  Created on: Jul 11, 2026
 *      Author: nguye
 */
#include "main.h"
#include "buzzer.h"

void buzzer_init(void)
{
    buzzer_off();
}

void buzzer_on(void)
{
    HAL_GPIO_WritePin(COI_GPIO_Port,
                      COI_Pin,
                      GPIO_PIN_SET);
}

void buzzer_off(void)
{
    HAL_GPIO_WritePin(COI_GPIO_Port,
                      COI_Pin,
                      GPIO_PIN_RESET);
}
