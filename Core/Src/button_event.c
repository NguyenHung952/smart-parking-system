/*
 * button_event.c
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#include "button_event.h"
#include "button.h"

/*==============================*/
/* Private Variable             */
/*==============================*/

static ButtonEvent_t current_event;
static uint8_t raw_pressed_mask;
static uint8_t stable_pressed_mask;
static uint32_t raw_changed_tick;

/*==============================*/
/* Private Function             */
/*==============================*/

static uint8_t button_event_read_pressed_mask(uint8_t data);
static void button_event_decode_new_press(uint8_t new_press_mask);

/*==============================*/
/* Public Function              */
/*==============================*/

void button_event_init(void)
{
    uint8_t data = Read74HC165();

    current_event = BUTTON_EVENT_NONE;
    raw_pressed_mask = button_event_read_pressed_mask(data);
    stable_pressed_mask = raw_pressed_mask;
    raw_changed_tick = HAL_GetTick();
}

void button_event_update(void)
{
    uint8_t data = Read74HC165();
    uint8_t pressed_mask = button_event_read_pressed_mask(data);
    uint32_t now = HAL_GetTick();

    if(pressed_mask != raw_pressed_mask)
    {
        raw_pressed_mask = pressed_mask;
        raw_changed_tick = now;
    }
    else if((pressed_mask != stable_pressed_mask) &&
            ((uint32_t)(now - raw_changed_tick) >= 30U))
    {
        uint8_t new_press_mask =
            (uint8_t)(pressed_mask & (uint8_t)(~stable_pressed_mask));

        stable_pressed_mask = pressed_mask;
        button_event_decode_new_press(new_press_mask);
    }
}

ButtonEvent_t button_event_get(void)
{
    ButtonEvent_t event = current_event;

    current_event = BUTTON_EVENT_NONE;

    return event;
}

/*==============================*/
/* Private Function             */
/*==============================*/

static uint8_t button_event_read_pressed_mask(uint8_t data)
{
    uint8_t sw;
    uint8_t mask = 0U;

    for(sw = 0U; sw < 4U; sw++)
    {
        if(ButtonPressed(data, sw) != 0U)
        {
            mask |= (uint8_t)(1U << sw);
        }
    }

    return mask;
}

static void button_event_decode_new_press(uint8_t new_press_mask)
{
    current_event = BUTTON_EVENT_NONE;

    if((new_press_mask & (1U << 0)) != 0U)
    {
        current_event = BUTTON_EVENT_SET;
    }
    else if((new_press_mask & (1U << 1)) != 0U)
    {
        current_event = BUTTON_EVENT_NEXT;
    }
    else if((new_press_mask & (1U << 2)) != 0U)
    {
        current_event = BUTTON_EVENT_UP;
    }
    else if((new_press_mask & (1U << 3)) != 0U)
    {
        current_event = BUTTON_EVENT_DOWN;
    }
}
