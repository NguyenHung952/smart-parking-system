#include "parking_gate.h"

#include "main.h"
#include "parking_slot.h"

#define PARKING_GATE_DEBOUNCE_MS        50U
#define PARKING_GATE_PRESSED_LEVEL      GPIO_PIN_RESET
#define PARKING_GATE_TIMEOUT_MS         5000U
#define PARKING_GATE_INVALID_SLOT       0xFFU

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t raw_pressed;
    uint8_t stable_pressed;
    uint32_t raw_changed_tick;
} ParkingGateButton_t;

static ParkingGateButton_t gate_in_button =
{
    GATE_IN_GPIO_Port, GATE_IN_Pin, 0U, 0U, 0U
};

static ParkingGateButton_t gate_out_button =
{
    GATE_OUT_GPIO_Port, GATE_OUT_Pin, 0U, 0U, 0U
};

static uint8_t gate_in_event_pending;
static uint8_t gate_out_event_pending;
static uint8_t gate_entry_open;
static uint8_t gate_exit_open;
static uint32_t gate_entry_deadline;
static uint32_t gate_exit_deadline;

static ParkingGateState_t gate_state;
static uint8_t gate_reference_mask;
static uint8_t gate_event_slot;
static uint8_t gate_completed_event;
static ParkingGateCompletedEvent_t gate_completed_type;

static uint8_t parking_gate_read_pressed(const ParkingGateButton_t *button);
static uint8_t parking_gate_update_button(ParkingGateButton_t *button);
static uint8_t parking_gate_find_changed_slot(uint8_t old_mask, uint8_t new_mask,
                                               uint8_t want_occupied);

void parking_gate_init(void)
{
    uint32_t now = HAL_GetTick();

    gate_in_button.raw_pressed = parking_gate_read_pressed(&gate_in_button);
    gate_in_button.stable_pressed = gate_in_button.raw_pressed;
    gate_in_button.raw_changed_tick = now;

    gate_out_button.raw_pressed = parking_gate_read_pressed(&gate_out_button);
    gate_out_button.stable_pressed = gate_out_button.raw_pressed;
    gate_out_button.raw_changed_tick = now;

    gate_in_event_pending = 0U;
    gate_out_event_pending = 0U;
    gate_entry_open = 0U;
    gate_exit_open = 0U;
    gate_entry_deadline = 0U;
    gate_exit_deadline = 0U;

    gate_state = PARKING_GATE_STATE_IDLE;
    gate_reference_mask = parking_slot_get_stable_mask();
    gate_event_slot = PARKING_GATE_INVALID_SLOT;
    gate_completed_event = 0U;
    gate_completed_type = PARKING_GATE_COMPLETED_NONE;
}

void parking_gate_update(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t current_mask = parking_slot_get_stable_mask();
    uint8_t slot;

    if(parking_gate_update_button(&gate_in_button) != 0U)
    {
        gate_in_event_pending = 1U;
    }

    if(parking_gate_update_button(&gate_out_button) != 0U)
    {
        gate_out_event_pending = 1U;
    }

    /* Khi cổng IN đang chờ xe đỗ, chỉ chấp nhận một bit E->F mới. */
    if(gate_state == PARKING_GATE_STATE_WAIT_PARK)
    {
        slot = parking_gate_find_changed_slot(gate_reference_mask,
                                               current_mask, 1U);
        if(slot != PARKING_GATE_INVALID_SLOT)
        {
            gate_event_slot = slot;
            gate_completed_event = 1U;
            gate_completed_type = PARKING_GATE_COMPLETED_ENTRY;
            gate_entry_open = 0U;
            gate_state = PARKING_GATE_STATE_IDLE;
        }
        else if((int32_t)(now - gate_entry_deadline) >= 0)
        {
            gate_entry_open = 0U;
            gate_state = PARKING_GATE_STATE_IDLE;
        }
    }
    else if(gate_state == PARKING_GATE_STATE_WAIT_EXIT)
    {
        slot = parking_gate_find_changed_slot(gate_reference_mask,
                                               current_mask, 0U);
        if(slot != PARKING_GATE_INVALID_SLOT)
        {
            gate_event_slot = slot;
            gate_completed_event = 1U;
            gate_completed_type = PARKING_GATE_COMPLETED_EXIT;
            gate_exit_open = 0U;
            gate_state = PARKING_GATE_STATE_IDLE;
        }
        else if((int32_t)(now - gate_exit_deadline) >= 0)
        {
            gate_exit_open = 0U;
            gate_state = PARKING_GATE_STATE_IDLE;
        }
    }

    /* Cập nhật trạng thái tham chiếu sau khi state machine đã xét thay đổi. */
    if(gate_state == PARKING_GATE_STATE_IDLE)
    {
        gate_reference_mask = current_mask;
    }

    /* Bãi đầy thì khóa cổng IN. */
    if(parking_slot_is_full() != 0U)
    {
        gate_entry_open = 0U;
        if(gate_state == PARKING_GATE_STATE_WAIT_PARK)
        {
            gate_state = PARKING_GATE_STATE_IDLE;
        }
    }
}

uint8_t parking_gate_in_event(void)
{
    return gate_in_event_pending;
}

uint8_t parking_gate_out_event(void)
{
    return gate_out_event_pending;
}

void parking_gate_clear_events(void)
{
    gate_in_event_pending = 0U;
    gate_out_event_pending = 0U;
}

uint8_t parking_gate_is_entry_allowed(void)
{
    if(gate_state != PARKING_GATE_STATE_IDLE)
    {
        return 0U;
    }

    return (parking_slot_is_full() == 0U) ? 1U : 0U;
}

uint8_t parking_gate_is_exit_allowed(void)
{
    if(gate_state != PARKING_GATE_STATE_IDLE)
    {
        return 0U;
    }

    return (parking_slot_get_occupied_count() > 0U) ? 1U : 0U;
}

void parking_gate_open_entry(void)
{
    if(parking_gate_is_entry_allowed() != 0U)
    {
        gate_entry_open = 1U;
        gate_exit_open = 0U;
        gate_reference_mask = parking_slot_get_stable_mask();
        gate_entry_deadline = HAL_GetTick() + PARKING_GATE_TIMEOUT_MS;
        gate_state = PARKING_GATE_STATE_WAIT_PARK;
        gate_event_slot = PARKING_GATE_INVALID_SLOT;
        gate_completed_event = 0U;
        gate_completed_type = PARKING_GATE_COMPLETED_NONE;
    }
}

void parking_gate_open_exit(void)
{
    if(parking_gate_is_exit_allowed() != 0U)
    {
        gate_exit_open = 1U;
        gate_entry_open = 0U;
        gate_reference_mask = parking_slot_get_stable_mask();
        gate_exit_deadline = HAL_GetTick() + PARKING_GATE_TIMEOUT_MS;
        gate_state = PARKING_GATE_STATE_WAIT_EXIT;
        gate_event_slot = PARKING_GATE_INVALID_SLOT;
        gate_completed_event = 0U;
        gate_completed_type = PARKING_GATE_COMPLETED_NONE;
    }
}

uint8_t parking_gate_entry_is_open(void)
{
    return gate_entry_open;
}

uint8_t parking_gate_exit_is_open(void)
{
    return gate_exit_open;
}

ParkingGateState_t parking_gate_get_state(void)
{
    return gate_state;
}

uint8_t parking_gate_get_event_slot(void)
{
    return gate_event_slot;
}

uint8_t parking_gate_has_completed_event(void)
{
    return gate_completed_event;
}

ParkingGateCompletedEvent_t parking_gate_get_completed_event(void)
{
    return gate_completed_type;
}

void parking_gate_clear_completed_event(void)
{
    gate_completed_event = 0U;
    gate_event_slot = PARKING_GATE_INVALID_SLOT;
    gate_completed_type = PARKING_GATE_COMPLETED_NONE;
}

static uint8_t parking_gate_find_changed_slot(uint8_t old_mask,
                                               uint8_t new_mask,
                                               uint8_t want_occupied)
{
    uint8_t changed = (uint8_t)(old_mask ^ new_mask);
    uint8_t index;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        uint8_t bit = (uint8_t)(1U << index);

        if((changed & bit) != 0U)
        {
            uint8_t occupied = ((new_mask & bit) != 0U) ? 1U : 0U;
            if(occupied == want_occupied)
            {
                return index;
            }
        }
    }

    return PARKING_GATE_INVALID_SLOT;
}

static uint8_t parking_gate_read_pressed(const ParkingGateButton_t *button)
{
    return (HAL_GPIO_ReadPin(button->port, button->pin) ==
            PARKING_GATE_PRESSED_LEVEL) ? 1U : 0U;
}

static uint8_t parking_gate_update_button(ParkingGateButton_t *button)
{
    uint8_t pressed = parking_gate_read_pressed(button);
    uint32_t now = HAL_GetTick();

    if(pressed != button->raw_pressed)
    {
        button->raw_pressed = pressed;
        button->raw_changed_tick = now;
        return 0U;
    }

    if((pressed != button->stable_pressed) &&
       ((uint32_t)(now - button->raw_changed_tick) >=
        PARKING_GATE_DEBOUNCE_MS))
    {
        uint8_t new_press = (pressed != 0U) &&
                            (button->stable_pressed == 0U);

        button->stable_pressed = pressed;
        return new_press;
    }

    return 0U;
}
