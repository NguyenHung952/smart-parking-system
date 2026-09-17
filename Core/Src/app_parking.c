#include "app_parking.h"

#include "button.h"
#include "button_event.h"
#include "buzzer.h"
#include "led.h"
#include "main.h"
#include "parking_alert.h"
#include "parking_gate.h"
#include "parking_slot.h"
#include "parking_storage.h"
#include "parking_ui.h"
#include "timer.h"

#define APP_SELF_TEST_LED_TIME_MS       100U
#define APP_SELF_TEST_BUZZER_TIME_MS    150U
#define APP_HEARTBEAT_TIME_MS           500U
#define APP_HEARTBEAT_LED_INDEX         6U  /* LED7 */
#define APP_MODE_LED_INDEX              7U  /* LED8 */
#define APP_DEMO_STEP_COUNT             6U

/* Kich ban DEMO: trong -> lan luot day -> day -> o 2 trong lai. */
static const uint8_t demo_mask[APP_DEMO_STEP_COUNT] =
{
    0x00U, 0x01U, 0x03U, 0x07U, 0x0FU, 0x0DU
};

static const uint16_t demo_time_ms[APP_DEMO_STEP_COUNT] =
{
    1600U, 1600U, 1600U, 1600U, 2500U, 4000U
};

typedef enum
{
    APP_MODE_AUTO = 0,
    APP_MODE_TEST,
    APP_MODE_DEMO
} AppParkingMode_t;

static AppParkingMode_t current_mode;
static uint8_t self_test_active;
static uint8_t self_test_step;
static uint32_t self_test_deadline;
static uint8_t heartbeat_on;
static uint32_t heartbeat_tick;
static uint8_t demo_step;
static uint32_t demo_deadline;

static void app_parking_process_button(ButtonEvent_t event);
static AppParkingMode_t app_parking_detect_boot_mode(void);
static void app_parking_start_self_test(void);
static void app_parking_update_self_test(void);
static void app_parking_finish_self_test(void);
static void app_parking_show_mode_message(void);
static void app_parking_heartbeat_update(void);
static void app_parking_demo_init(void);
static void app_parking_demo_update(void);

void app_parking_init(void)
{
    timer_init();
    led_init();
    buzzer_init();
    parking_slot_init();
    parking_gate_init();

    /* Cho muc dien va 74HC165 on dinh truoc khi doc to hop khoi dong. */
    HAL_Delay(80U);
    current_mode = app_parking_detect_boot_mode();

    /*
     * GIAI DOAN TEST PHAN CUNG:
     *   DI.0-DI.3 = nut nhan mo phong 4 cam bien o do.
     *   Moi lan nhan dao trang thai E <-> F.
     *
     * Khi thay bang cam bien that, chi can doi ve:
     *   parking_slot_set_input_mode(PARKING_INPUT_SENSOR);
     *   parking_slot_set_button_test_enabled(0U);
     */
    if(current_mode == APP_MODE_AUTO)
    {
        /* Khong reset virtual_state sau khi da nap slot_mask tu 24C16. */
        parking_slot_set_input_mode(PARKING_INPUT_MANUAL);
        parking_slot_set_virtual_mask(parking_storage_get_last_mask());
        parking_slot_update();
        parking_storage_set_baseline_mask(parking_slot_get_stable_mask());
        parking_slot_set_button_test_enabled(1U);
    }
    else
    {
        /* TEST/DEMO dung trang thai ao. */
        parking_slot_set_input_mode(PARKING_INPUT_MANUAL);
        parking_slot_set_button_test_enabled(0U);
    }

    if(current_mode == APP_MODE_AUTO)
    {
        /* Moi lan khoi dong lai bang RESET: CHIEM/TRẢ ve 0, slot state giu lai. */
        parking_storage_init();
    }
    else
    {
        parking_storage_init_read_only();
    }

    parking_ui_init();

    if(current_mode == APP_MODE_TEST)
    {
        parking_ui_set_mode(PARKING_UI_MODE_TEST);
    }
    else if(current_mode == APP_MODE_DEMO)
    {
        parking_ui_set_mode(PARKING_UI_MODE_DEMO);
    }
    else
    {
        parking_ui_set_mode(PARKING_UI_MODE_AUTO);
    }

    /* Dong bo nut dang duoc giu, tranh tao su kien gia khi tha tay. */
    button_event_init();
    parking_alert_init();

    heartbeat_on = 0U;
    heartbeat_tick = HAL_GetTick();
    demo_step = 0U;
    demo_deadline = HAL_GetTick();

    app_parking_start_self_test();
}

void app_parking_process(void)
{
    ButtonEvent_t event;
    ParkingAlertEvent_t alert_event;

    if(self_test_active != 0U)
    {
        app_parking_update_self_test();
        return;
    }

    button_event_update();
    event = button_event_get();

    if(current_mode == APP_MODE_DEMO)
    {
        app_parking_demo_update();
    }

    /*
     * Doc Gate truoc, sau do mo WAIT_PARK/WAIT_EXIT NGAY trong vong lap nay.
     * Nhu vay khi nut SLOT duoc nhan o vong lap tiep theo, Gate permission
     * da co san.
     */
    parking_gate_update();

    /* DI.4 = CỔNG VÀO, DI.5 = CỔNG RA. */
    if((current_mode == APP_MODE_AUTO) || (current_mode == APP_MODE_TEST))
    {
        if(parking_gate_in_event() != 0U)
        {
            if(parking_gate_is_entry_allowed() != 0U)
            {
                parking_gate_open_entry();
                parking_ui_show_message("CONG IN: MO",
                                        "CHO XE VAO",
                                        1200U);
            }
            else
            {
                parking_ui_show_message("BAI XE DA DAY",
                                        "KHONG CHO VAO",
                                        1800U);
            }
        }

        if(parking_gate_out_event() != 0U)
        {
            if(parking_gate_is_exit_allowed() != 0U)
            {
                parking_gate_open_exit();
                parking_ui_show_message("CONG OUT: MO",
                                        "CHO XE RA",
                                        1200U);
            }
            else
            {
                parking_ui_show_message("BAI DANG TRONG",
                                        "KHONG CO XE RA",
                                        1800U);
            }
        }

        parking_gate_clear_events();
    }

    /* Cap quyen thay doi SLOT theo state cua Gate truoc khi doc nut SLOT. */
    if(current_mode == APP_MODE_AUTO)
    {
        if(parking_gate_get_state() == PARKING_GATE_STATE_WAIT_PARK)
        {
            parking_slot_set_change_permission(PARKING_SLOT_CHANGE_ENTRY);
        }
        else if(parking_gate_get_state() == PARKING_GATE_STATE_WAIT_EXIT)
        {
            parking_slot_set_change_permission(PARKING_SLOT_CHANGE_EXIT);
        }
        else
        {
            parking_slot_set_change_permission(PARKING_SLOT_CHANGE_NONE);
        }
    }
    else
    {
        parking_slot_set_change_permission(PARKING_SLOT_CHANGE_NONE);
    }

    /*
     * Chi chap nhan thay doi SLOT khi dung quy trinh cua Gate.
     *
     * AUTO:
     *   IDLE + nhan SLOT              -> bo qua, khoi phuc slot cu.
     *   WAIT_PARK + slot TRONG -> CO XE -> chap nhan IN.
     *   WAIT_PARK + slot DA CO XE -> nhan lai -> bo qua, giu nguyen.
     *   WAIT_EXIT + slot CO XE -> TRONG -> chap nhan OUT.
     *   WAIT_EXIT + slot TRONG -> nhan lai -> bo qua, giu nguyen.
     *
     * Moi lan chi cho phep DUY NHAT 1 slot thay doi.
     */
    {
        uint8_t slot_mask_before = parking_slot_get_stable_mask();
        ParkingGateState_t gate_state_before_slot =
            parking_gate_get_state();

        parking_slot_update();

        if(current_mode == APP_MODE_AUTO)
        {
            uint8_t slot_mask_after = parking_slot_get_stable_mask();
            uint8_t changed_mask =
                (uint8_t)(slot_mask_before ^ slot_mask_after);
            uint8_t valid_change = 0U;

            /* Chi 1 bit duoc phep thay doi trong mot lan xe. */
            if((changed_mask != 0U) &&
               ((changed_mask & (uint8_t)(changed_mask - 1U)) == 0U))
            {
                if(gate_state_before_slot == PARKING_GATE_STATE_WAIT_PARK)
                {
                    /* IN: slot bat buoc phai 0 -> 1. */
                    if((slot_mask_after & changed_mask) != 0U)
                    {
                        valid_change = 1U;
                    }
                }
                else if(gate_state_before_slot == PARKING_GATE_STATE_WAIT_EXIT)
                {
                    /* OUT: slot bat buoc phai 1 -> 0. */
                    if((slot_mask_after & changed_mask) == 0U)
                    {
                        valid_change = 1U;
                    }
                }
            }

            if((changed_mask != 0U) && (valid_change == 0U))
            {
                /* Khong co CỔNG hop le hoac sai chieu -> khong doi slot. */
                parking_slot_restore_mask(slot_mask_before);
                parking_storage_set_baseline_mask(slot_mask_before);
            }
        }
    }

    /* Doc lai Gate sau khi SLOT da thay doi hop le. */
    parking_gate_update();

    if(parking_gate_has_completed_event() != 0U)
    {
        uint8_t completed_slot = parking_gate_get_event_slot();
        ParkingGateCompletedEvent_t completed_event =
            parking_gate_get_completed_event();

        if(completed_slot < PARKING_SLOT_COUNT)
        {
            if(completed_event == PARKING_GATE_COMPLETED_ENTRY)
            {
                /* Chi CỔNG VÀO + SLOT hop le moi tang IN. */
                parking_storage_record_entry(completed_slot);
                parking_ui_show_message("XE VAO",
                                        "DA NHAN SLOT",
                                        900U);
            }
            else if(completed_event == PARKING_GATE_COMPLETED_EXIT)
            {
                /* Chi CỔNG RA + SLOT dang co xe moi tang OUT. */
                parking_storage_record_exit(completed_slot);
                parking_ui_show_message("XE RA",
                                        "DA TRA SLOT",
                                        900U);
            }
        }

        parking_gate_clear_completed_event();
    }

    /* Sau khi xac nhan xong hoac timeout, khong cho SLOT tu doi trang thai. */
    if(current_mode == APP_MODE_AUTO)
    {
        if(parking_gate_get_state() == PARKING_GATE_STATE_WAIT_PARK)
        {
            parking_slot_set_change_permission(PARKING_SLOT_CHANGE_ENTRY);
        }
        else if(parking_gate_get_state() == PARKING_GATE_STATE_WAIT_EXIT)
        {
            parking_slot_set_change_permission(PARKING_SLOT_CHANGE_EXIT);
        }
        else
        {
            parking_slot_set_change_permission(PARKING_SLOT_CHANGE_NONE);
        }
    }

    /* Chi ghi EEPROM sau khi co thay doi hop le hoac dang retry save. */
    if(current_mode == APP_MODE_AUTO)
    {
        parking_storage_update();
    }

    app_parking_process_button(event);

    alert_event = parking_alert_update();

    if(alert_event == PARKING_ALERT_EVENT_BECAME_FULL)
    {
        parking_ui_show_message("BAI XE DA DAY",
                                "VUI LONG DOI",
                                2000U);
    }

    parking_ui_update();
    app_parking_heartbeat_update();
}

static void app_parking_process_button(ButtonEvent_t event)
{
    if(current_mode == APP_MODE_TEST)
    {
        if(event == BUTTON_EVENT_SET)
        {
            parking_slot_toggle_virtual(0U);
        }
        else if(event == BUTTON_EVENT_NEXT)
        {
            parking_slot_toggle_virtual(1U);
        }
        else if(event == BUTTON_EVENT_UP)
        {
            parking_slot_toggle_virtual(2U);
        }
        else if(event == BUTTON_EVENT_DOWN)
        {
            parking_slot_toggle_virtual(3U);
        }
    }
    else if(current_mode == APP_MODE_AUTO)
    {
        if(event == BUTTON_EVENT_SET)
        {
            /* SW1: tat chuoi coi hien tai. */
            parking_alert_silence();
        }
        else if(event == BUTTON_EVENT_NEXT)
        {
            /* SW2: chu thich LED, tach rieng voi trang RAW/STAB. */
            parking_ui_show_message("LED1-4: TRANG O",
                                    "L5:DAY L6:CON",
                                    2000U);
        }
        else if(event == BUTTON_EVENT_UP)
        {
            /* SW3: bat/tat chan doan cam bien. */
            parking_ui_toggle_diagnostic();
        }
        else if(event == BUTTON_EVENT_DOWN)
        {
            /* SW4: chua su dung. Danh rieng cho tinh nang mo rong sau nay. */
        }
    }
    else
    {
        /* DEMO: SW1 tat coi, SW3 cho phep xem RAW/STAB cua trang thai ao. */
        if(event == BUTTON_EVENT_SET)
        {
            parking_alert_silence();
        }
        else if(event == BUTTON_EVENT_UP)
        {
            parking_ui_toggle_diagnostic();
        }
    }
}

static AppParkingMode_t app_parking_detect_boot_mode(void)
{
    uint8_t data = Read74HC165();
    uint8_t sw1_pressed = ButtonPressed(data, 0U);
    uint8_t sw2_pressed = ButtonPressed(data, 1U);
    uint8_t sw3_pressed = ButtonPressed(data, 2U);
    uint8_t sw4_pressed = ButtonPressed(data, 3U);

    /* Uu tien TEST neu co nhieu hon mot to hop bi giu luc Reset. */
    if((sw1_pressed != 0U) && (sw4_pressed != 0U))
    {
        return APP_MODE_TEST;
    }

    if((sw2_pressed != 0U) && (sw3_pressed != 0U))
    {
        return APP_MODE_DEMO;
    }

    return APP_MODE_AUTO;
}

static void app_parking_start_self_test(void)
{
    uint8_t index;

    for(index = 0U; index < 8U; index++)
    {
        led_set(index, 0U);
    }

    buzzer_off();
    self_test_active = 1U;
    self_test_step = 0U;
    self_test_deadline = HAL_GetTick();

    parking_ui_show_message("DANG KIEM TRA",
                            "LED - LCD - COI",
                            0U);
}

static void app_parking_update_self_test(void)
{
    uint32_t now = HAL_GetTick();

    if((int32_t)(now - self_test_deadline) < 0)
    {
        return;
    }

    if(self_test_step < 8U)
    {
        if(self_test_step > 0U)
        {
            led_set((uint8_t)(self_test_step - 1U), 0U);
        }

        led_set(self_test_step, 1U);
        self_test_step++;
        self_test_deadline = now + APP_SELF_TEST_LED_TIME_MS;
        return;
    }

    if(self_test_step == 8U)
    {
        led_set(7U, 0U);
        buzzer_on();
        self_test_step++;
        self_test_deadline = now + APP_SELF_TEST_BUZZER_TIME_MS;
        return;
    }

    app_parking_finish_self_test();
}

static void app_parking_finish_self_test(void)
{
    buzzer_off();
    led_off();

    /* Doc/dong bo lai 4 o de khoi phuc LED1...LED4 sau bai kiem tra. */
    if(current_mode == APP_MODE_AUTO)
    {
        parking_slot_set_input_mode(PARKING_INPUT_MANUAL);
        parking_slot_set_button_test_enabled(1U);
    }
    else
    {
        parking_slot_set_input_mode(PARKING_INPUT_MANUAL);
        parking_slot_set_button_test_enabled(0U);
    }

    if(current_mode == APP_MODE_DEMO)
    {
        app_parking_demo_init();
    }

    /* Khoi phuc LED DAY/CON sau bai kiem tra. */
    parking_alert_init();
    parking_gate_init();
    led_set(APP_MODE_LED_INDEX,
            (current_mode == APP_MODE_AUTO) ? 0U : 1U);
    led_set(APP_HEARTBEAT_LED_INDEX, 0U);

    heartbeat_on = 0U;
    heartbeat_tick = HAL_GetTick();
    self_test_active = 0U;

    app_parking_show_mode_message();
}

static void app_parking_show_mode_message(void)
{
    if(current_mode == APP_MODE_TEST)
    {
        parking_ui_show_message("CHE DO KIEM THU",
                                "SW1-SW4: O1-O4",
                                1500U);
    }
    else if(current_mode == APP_MODE_DEMO)
    {
        parking_ui_show_message("CHE DO DEMO",
                                "TU DONG MO PHONG",
                                1500U);
    }
    else
    {
        parking_ui_show_message("SMART PARKING",
                                "TEST: DI0-DI5",
                                1500U);
    }
}

static void app_parking_heartbeat_update(void)
{
    uint32_t now = HAL_GetTick();

    if((uint32_t)(now - heartbeat_tick) < APP_HEARTBEAT_TIME_MS)
    {
        return;
    }

    heartbeat_tick = now;
    heartbeat_on = (heartbeat_on == 0U) ? 1U : 0U;
    led_set(APP_HEARTBEAT_LED_INDEX, heartbeat_on);
}

static void app_parking_demo_init(void)
{
    demo_step = 0U;
    parking_slot_set_virtual_mask(demo_mask[demo_step]);
    demo_deadline = HAL_GetTick() + demo_time_ms[demo_step];
}

static void app_parking_demo_update(void)
{
    uint32_t now = HAL_GetTick();

    if((int32_t)(now - demo_deadline) < 0)
    {
        return;
    }

    demo_step++;
    if(demo_step >= APP_DEMO_STEP_COUNT)
    {
        demo_step = 0U;
    }

    parking_slot_set_virtual_mask(demo_mask[demo_step]);
    demo_deadline = now + demo_time_ms[demo_step];
}
