/*
 * led.c
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#include "main.h"
#include "led.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} LedHardware_t;

static const LedHardware_t led_hardware[8] =
{
    {LED1_GPIO_Port, LED1_Pin},
    {LED2_GPIO_Port, LED2_Pin},
    {LED3_GPIO_Port, LED3_Pin},
    {LED4_GPIO_Port, LED4_Pin},
    {LED5_GPIO_Port, LED5_Pin},
    {LED6_GPIO_Port, LED6_Pin},
    {LED7_GPIO_Port, LED7_Pin},
    {LED8_GPIO_Port, LED8_Pin}
};

void led_init(void)
{
    led_off();
}

void led_set(uint8_t led_index, uint8_t on)
{
    if(led_index >= 8U)
    {
        return;
    }

    HAL_GPIO_WritePin(led_hardware[led_index].port,
                      led_hardware[led_index].pin,
                      (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void led_off(void){
	  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, 1);
	  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
	  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, 1);
	  HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, 1);
	  HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, 1);
	  HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, 1);
	  HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, 1);
	  HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, 1);
}

void led_on(void){
	  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, 0);
	  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
	  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, 0);
	  HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, 0);
	  HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, 0);
	  HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, 0);
	  HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, 0);
	  HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, 0);
}

