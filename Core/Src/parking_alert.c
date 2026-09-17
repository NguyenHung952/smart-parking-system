#include "parking_alert.h"

#include "buzzer.h"
#include "led.h"
#include "main.h"
#include "parking_slot.h"

#define PARKING_FULL_LED_INDEX      4U  /* LED5 */
#define PARKING_FREE_LED_INDEX      5U  /* LED6 */
#define PARKING_BEEP_TIME_MS        150U
#define PARKING_BEEP_PAUSE_MS       150U

typedef enum
{
    BEEP_IDLE = 0,
    BEEP_FIRST_ON,
    BEEP_PAUSE,
    BEEP_SECOND_ON
} ParkingBeepState_t;

static uint8_t previous_full_state;
static ParkingBeepState_t beep_state;
static uint32_t beep_deadline;

static void parking_alert_start_full_beep(void);
static void parking_alert_update_beep(void);

void parking_alert_init(void)
{
    previous_full_state = parking_slot_is_full();
    beep_state = BEEP_IDLE;
    beep_deadline = 0U;
    buzzer_off();

    led_set(PARKING_FULL_LED_INDEX, previous_full_state);
    led_set(PARKING_FREE_LED_INDEX,
            (previous_full_state == 0U) ? 1U : 0U);
}

ParkingAlertEvent_t parking_alert_update(void)
{
    uint8_t current_full_state = parking_slot_is_full();
    ParkingAlertEvent_t event = PARKING_ALERT_EVENT_NONE;

    led_set(PARKING_FULL_LED_INDEX, current_full_state);
    led_set(PARKING_FREE_LED_INDEX,
            (current_full_state == 0U) ? 1U : 0U);

    if((current_full_state != 0U) && (previous_full_state == 0U))
    {
        parking_alert_start_full_beep();
        event = PARKING_ALERT_EVENT_BECAME_FULL;
    }
    else if((current_full_state == 0U) && (previous_full_state != 0U))
    {
        parking_alert_silence();
    }

    previous_full_state = current_full_state;
    parking_alert_update_beep();

    return event;
}

void parking_alert_silence(void)
{
    buzzer_off();
    beep_state = BEEP_IDLE;
    beep_deadline = 0U;
}

static void parking_alert_start_full_beep(void)
{
    buzzer_on();
    beep_state = BEEP_FIRST_ON;
    beep_deadline = HAL_GetTick() + PARKING_BEEP_TIME_MS;
}

static void parking_alert_update_beep(void)
{
    uint32_t now = HAL_GetTick();

    if((beep_state == BEEP_IDLE) ||
       ((int32_t)(now - beep_deadline) < 0))
    {
        return;
    }

    if(beep_state == BEEP_FIRST_ON)
    {
        buzzer_off();
        beep_state = BEEP_PAUSE;
        beep_deadline = now + PARKING_BEEP_PAUSE_MS;
    }
    else if(beep_state == BEEP_PAUSE)
    {
        buzzer_on();
        beep_state = BEEP_SECOND_ON;
        beep_deadline = now + PARKING_BEEP_TIME_MS;
    }
    else
    {
        parking_alert_silence();
    }
}
