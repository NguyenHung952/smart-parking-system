/*
 * button_event.h
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#ifndef BUTTON_EVENT_H_
#define BUTTON_EVENT_H_

#include <stdint.h>

typedef enum
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SET,
    BUTTON_EVENT_NEXT,
    BUTTON_EVENT_UP,
    BUTTON_EVENT_DOWN
} ButtonEvent_t;

/*==============================*/
/* Public API                   */
/*==============================*/

void button_event_init(void);

void button_event_update(void);

ButtonEvent_t button_event_get(void);

#endif /* BUTTON_EVENT_H_ */
