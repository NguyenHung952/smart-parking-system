#include "parking_guidance.h"

#include "parking_slot.h"

uint8_t parking_guidance_find_available(void)
{
    uint8_t slot_index;

    for(slot_index = 0U; slot_index < PARKING_SLOT_COUNT; slot_index++)
    {
        if(parking_slot_get_state(slot_index) == PARKING_SLOT_EMPTY)
        {
            return slot_index;
        }
    }

    return PARKING_GUIDANCE_NO_SLOT;
}
