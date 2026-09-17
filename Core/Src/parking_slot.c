#include "parking_slot.h"

#include "main.h"
#include "led.h"

/*
 * ARM KIT 1:
 *   DI.0 -> PE13
 *   DI.1 -> PE12
 *   DI.2 -> PE11
 *   DI.3 -> PE10
 *
 * Neu cam bien cua ban co logic nguoc, chi doi GPIO_PIN_SET thanh
 * GPIO_PIN_RESET tai PARKING_SENSOR_OCCUPIED_LEVEL.
 */
#define PARKING_SENSOR_OCCUPIED_LEVEL   GPIO_PIN_RESET

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} ParkingSensorHardware_t;

static const ParkingSensorHardware_t sensor_hardware[PARKING_SLOT_COUNT] =
{
    {GPIOE, GPIO_PIN_13},
    {GPIOE, GPIO_PIN_12},
    {GPIOE, GPIO_PIN_11},
    {GPIOE, GPIO_PIN_10}
};

static ParkingSlotState_t stable_state[PARKING_SLOT_COUNT];
static ParkingSlotState_t raw_state[PARKING_SLOT_COUNT];
static ParkingSlotState_t virtual_state[PARKING_SLOT_COUNT];
static uint32_t raw_changed_tick[PARKING_SLOT_COUNT];
static uint8_t state_changed;
static ParkingInputMode_t input_mode;
static uint8_t button_test_enabled;
static ParkingSlotChangePermission_t change_permission;
static uint8_t button_raw_pressed[PARKING_SLOT_COUNT];
static uint8_t button_stable_pressed[PARKING_SLOT_COUNT];
static uint32_t button_raw_changed_tick[PARKING_SLOT_COUNT];

static ParkingSlotState_t parking_slot_read_sensor(uint8_t slot_index);
static void parking_slot_update_led(uint8_t slot_index);
static uint8_t parking_slot_read_button_pressed(uint8_t slot_index);
static void parking_slot_update_button_test(void);

void parking_slot_init(void)
{
    uint8_t index;
    uint32_t now = HAL_GetTick();

    state_changed = 1U;
    input_mode = PARKING_INPUT_MANUAL;
    button_test_enabled = 0U;
    change_permission = PARKING_SLOT_CHANGE_NONE;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        virtual_state[index] = PARKING_SLOT_EMPTY;
        button_raw_pressed[index] = parking_slot_read_button_pressed(index);
        button_stable_pressed[index] = button_raw_pressed[index];
        button_raw_changed_tick[index] = now;
        raw_state[index] = parking_slot_read_sensor(index);
        stable_state[index] = raw_state[index];
        raw_changed_tick[index] = now;
        parking_slot_update_led(index);
    }
}

void parking_slot_set_input_mode(ParkingInputMode_t mode)
{
    uint8_t index;
    uint32_t now = HAL_GetTick();

    if((mode != PARKING_INPUT_MANUAL) &&
       (mode != PARKING_INPUT_SENSOR))
    {
        return;
    }

    input_mode = mode;
    change_permission = PARKING_SLOT_CHANGE_NONE;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        if(input_mode == PARKING_INPUT_MANUAL)
        {
            virtual_state[index] = PARKING_SLOT_EMPTY;
        }

        raw_state[index] = parking_slot_read_sensor(index);
        stable_state[index] = raw_state[index];
        raw_changed_tick[index] = now;
        parking_slot_update_led(index);
    }

    state_changed = 1U;
}

ParkingInputMode_t parking_slot_get_input_mode(void)
{
    return input_mode;
}

void parking_slot_set_change_permission(ParkingSlotChangePermission_t permission)
{
    if((permission != PARKING_SLOT_CHANGE_NONE) &&
       (permission != PARKING_SLOT_CHANGE_ENTRY) &&
       (permission != PARKING_SLOT_CHANGE_EXIT))
    {
        permission = PARKING_SLOT_CHANGE_NONE;
    }

    change_permission = permission;
}

void parking_slot_restore_mask(uint8_t occupied_mask)
{
    uint8_t index;
    uint8_t mask = (uint8_t)(occupied_mask & 0x0FU);
    uint32_t now = HAL_GetTick();

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        ParkingSlotState_t state =
            ((mask & (uint8_t)(1U << index)) != 0U) ?
            PARKING_SLOT_OCCUPIED : PARKING_SLOT_EMPTY;

        raw_state[index] = state;
        stable_state[index] = state;
        virtual_state[index] = state;
        raw_changed_tick[index] = now;
        button_raw_pressed[index] =
            (state == PARKING_SLOT_OCCUPIED) ? 1U : 0U;
        button_stable_pressed[index] = button_raw_pressed[index];
        button_raw_changed_tick[index] = now;
    }

    state_changed = 0U;
}

void parking_slot_set_button_test_enabled(uint8_t enabled)
{
    uint8_t index;
    uint32_t now = HAL_GetTick();

    button_test_enabled = (enabled != 0U) ? 1U : 0U;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        button_raw_pressed[index] = parking_slot_read_button_pressed(index);
        button_stable_pressed[index] = button_raw_pressed[index];
        button_raw_changed_tick[index] = now;
    }
}

void parking_slot_toggle_virtual(uint8_t slot_index)
{
    if(slot_index >= PARKING_SLOT_COUNT)
    {
        return;
    }

    if(input_mode == PARKING_INPUT_MANUAL)
    {
        virtual_state[slot_index] =
            (virtual_state[slot_index] == PARKING_SLOT_EMPTY) ?
            PARKING_SLOT_OCCUPIED : PARKING_SLOT_EMPTY;
    }
}

void parking_slot_set_virtual_mask(uint8_t occupied_mask)
{
    uint8_t index;

    if(input_mode != PARKING_INPUT_MANUAL)
    {
        return;
    }

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        virtual_state[index] =
            ((occupied_mask & (uint8_t)(1U << index)) != 0U) ?
            PARKING_SLOT_OCCUPIED : PARKING_SLOT_EMPTY;
    }
}

void parking_slot_update(void)
{
    uint8_t index;
    uint32_t now = HAL_GetTick();

    if((input_mode == PARKING_INPUT_MANUAL) &&
       (button_test_enabled != 0U))
    {
        parking_slot_update_button_test();
        return;
    }

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        ParkingSlotState_t sample = parking_slot_read_sensor(index);

        if(sample != raw_state[index])
        {
            raw_state[index] = sample;
            raw_changed_tick[index] = now;
        }
        else if((sample != stable_state[index]) &&
                ((uint32_t)(now - raw_changed_tick[index]) >=
                 PARKING_SENSOR_DEBOUNCE_MS))
        {
            stable_state[index] = sample;
            state_changed = 1U;
            parking_slot_update_led(index);
        }
    }
}

ParkingSlotState_t parking_slot_get_state(uint8_t slot_index)
{
    if(slot_index >= PARKING_SLOT_COUNT)
    {
        return PARKING_SLOT_EMPTY;
    }

    return stable_state[slot_index];
}

uint8_t parking_slot_get_raw_mask(void)
{
    uint8_t index;
    uint8_t mask = 0U;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        if(raw_state[index] == PARKING_SLOT_OCCUPIED)
        {
            mask |= (uint8_t)(1U << index);
        }
    }

    return mask;
}

uint8_t parking_slot_get_stable_mask(void)
{
    uint8_t index;
    uint8_t mask = 0U;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        if(stable_state[index] == PARKING_SLOT_OCCUPIED)
        {
            mask |= (uint8_t)(1U << index);
        }
    }

    return mask;
}

uint8_t parking_slot_get_free_count(void)
{
    uint8_t index;
    uint8_t free_count = 0U;

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        if(stable_state[index] == PARKING_SLOT_EMPTY)
        {
            free_count++;
        }
    }

    return free_count;
}

uint8_t parking_slot_get_occupied_count(void)
{
    return (uint8_t)(PARKING_SLOT_COUNT - parking_slot_get_free_count());
}

uint8_t parking_slot_is_full(void)
{
    return parking_slot_get_free_count() == 0U;
}

uint8_t parking_slot_has_changed(void)
{
    return state_changed;
}

void parking_slot_clear_changed(void)
{
    state_changed = 0U;
}

static uint8_t parking_slot_read_button_pressed(uint8_t slot_index)
{
    static const uint16_t button_pins[PARKING_SLOT_COUNT] =
    {
        GPIO_PIN_13, GPIO_PIN_12, GPIO_PIN_11, GPIO_PIN_10
    };

    if(slot_index >= PARKING_SLOT_COUNT)
    {
        return 0U;
    }

    return (HAL_GPIO_ReadPin(GPIOE, button_pins[slot_index]) ==
            GPIO_PIN_RESET) ? 1U : 0U;
}

static void parking_slot_update_button_test(void)
{
    uint8_t index;
    uint32_t now = HAL_GetTick();

    for(index = 0U; index < PARKING_SLOT_COUNT; index++)
    {
        uint8_t pressed = parking_slot_read_button_pressed(index);

        if(pressed != button_raw_pressed[index])
        {
            button_raw_pressed[index] = pressed;
            button_raw_changed_tick[index] = now;
        }
        else if((pressed != button_stable_pressed[index]) &&
                ((uint32_t)(now - button_raw_changed_tick[index]) >=
                 PARKING_SENSOR_DEBOUNCE_MS))
        {
            button_stable_pressed[index] = pressed;

            /* Chi xu ly canh nhan: moi lan bam dao E <-> F. */
            if(pressed != 0U)
            {
                /* AUTO: Gate phai cap quyen dung chieu truoc khi slot duoc doi.
                 *
                 * ENTRY: chi cho 0 -> 1.
                 * EXIT : chi cho 1 -> 0.
                 * NONE : bam slot mot minh => bo qua.
                 *
                 * Quan trong: neu CỔNG VÀO + bam slot da co xe,
                 * khong dao trang thai 1 -> 0.
                 * Neu CỔNG RA + bam slot trong, cung khong dao 0 -> 1.
                 */
                if((change_permission == PARKING_SLOT_CHANGE_ENTRY) &&
                   (virtual_state[index] == PARKING_SLOT_EMPTY))
                {
                    virtual_state[index] = PARKING_SLOT_OCCUPIED;
                }
                else if((change_permission == PARKING_SLOT_CHANGE_EXIT) &&
                        (virtual_state[index] == PARKING_SLOT_OCCUPIED))
                {
                    virtual_state[index] = PARKING_SLOT_EMPTY;
                }
                else
                {
                    /* Sai quy trinh Gate: khong thay doi slot. */
                    continue;
                }

                stable_state[index] = virtual_state[index];
                raw_state[index] = virtual_state[index];
                state_changed = 1U;
                parking_slot_update_led(index);
            }
        }
    }
}

static ParkingSlotState_t parking_slot_read_sensor(uint8_t slot_index)
{
    if(slot_index >= PARKING_SLOT_COUNT)
    {
        return PARKING_SLOT_EMPTY;
    }

    if(input_mode == PARKING_INPUT_MANUAL)
    {
        return virtual_state[slot_index];
    }

    GPIO_PinState pin_state;

    pin_state = HAL_GPIO_ReadPin(sensor_hardware[slot_index].port,
                                 sensor_hardware[slot_index].pin);

    return (pin_state == PARKING_SENSOR_OCCUPIED_LEVEL) ?
           PARKING_SLOT_OCCUPIED : PARKING_SLOT_EMPTY;
}

static void parking_slot_update_led(uint8_t slot_index)
{
    /* LED1...LED4 sang khi o tuong ung dang co xe. */
    led_set(slot_index,
            stable_state[slot_index] == PARKING_SLOT_OCCUPIED);
}
