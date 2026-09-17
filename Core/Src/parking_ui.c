#include "parking_ui.h"

#include <stdio.h>

#include "lcd.h"
#include "main.h"
#include "parking_guidance.h"
#include "parking_slot.h"
#include "parking_storage.h"

#define PARKING_UI_REFRESH_MS       200U
#define PARKING_UI_PAGE_TIME_MS     2500U
#define PARKING_UI_SESSION_MAX_COUNT  999U

typedef enum
{
    PARKING_UI_PAGE_STATUS = 0,
    PARKING_UI_PAGE_GUIDANCE,
    PARKING_UI_PAGE_COUNTER,
    PARKING_UI_PAGE_FLOW,
    PARKING_UI_PAGE_COUNT
} ParkingUiPage_t;

static uint32_t last_refresh_tick;
static uint32_t message_end_tick;
static uint8_t message_active;
static char message_line_1[17];
static char message_line_2[17];
static ParkingUiPage_t current_page;
static uint32_t page_changed_tick;
static ParkingUiMode_t current_mode;
static uint8_t diagnostic_enabled;
static uint32_t session_vao;
static uint32_t session_ra;
static uint8_t current_occupied_mask;
static uint8_t last_released_mask;

static void parking_ui_write_line(uint8_t row, const char *text);
static void parking_ui_show_normal(void);
static void parking_ui_show_status(void);
static void parking_ui_show_guidance(void);
static void parking_ui_show_counter(void);
static void parking_ui_show_flow(void);
static void parking_ui_show_diagnostic(void);
static void parking_ui_build_slot_line(char line[17]);
static void parking_ui_build_mask_line(char line[17],
                                       const char *prefix,
                                       uint8_t mask);
static const char *parking_ui_get_mode_text(void);
static void parking_ui_copy_line(char destination[17], const char *source);

void parking_ui_init(void)
{
    last_refresh_tick = 0U;
    message_end_tick = 0U;
    message_active = 0U;
    message_line_1[0] = '\0';
    message_line_2[0] = '\0';
    current_page = PARKING_UI_PAGE_STATUS;
    page_changed_tick = HAL_GetTick();
    current_mode = PARKING_UI_MODE_AUTO;
    diagnostic_enabled = 0U;
    session_vao = 0U;
    session_ra = 0U;
    current_occupied_mask = (uint8_t)(parking_slot_get_stable_mask() & 0x0FU);
    last_released_mask = 0U;

    lcd_cursor_off();
    lcd_blink_off();
    parking_ui_show_normal();
    parking_slot_clear_changed();
}

void parking_ui_set_mode(ParkingUiMode_t mode)
{
    if((mode != PARKING_UI_MODE_AUTO) &&
       (mode != PARKING_UI_MODE_TEST) &&
       (mode != PARKING_UI_MODE_DEMO))
    {
        return;
    }

    current_mode = mode;
    diagnostic_enabled = 0U;
    current_page = PARKING_UI_PAGE_STATUS;
    page_changed_tick = HAL_GetTick();

    if(message_active == 0U)
    {
        parking_ui_show_normal();
        last_refresh_tick = HAL_GetTick();
    }
}

void parking_ui_toggle_diagnostic(void)
{
    diagnostic_enabled = (diagnostic_enabled == 0U) ? 1U : 0U;
    current_page = PARKING_UI_PAGE_STATUS;
    page_changed_tick = HAL_GetTick();
    parking_slot_clear_changed();

    if(message_active == 0U)
    {
        parking_ui_show_normal();
        last_refresh_tick = HAL_GetTick();
    }
}

uint8_t parking_ui_is_diagnostic_enabled(void)
{
    return diagnostic_enabled;
}

void parking_ui_update(void)
{
    uint32_t now = HAL_GetTick();

    if(message_active != 0U)
    {
        if(message_active == 2U)
        {
            return;
        }

        if((int32_t)(now - message_end_tick) < 0)
        {
            return;
        }

        message_active = 0U;
        current_page = PARKING_UI_PAGE_STATUS;
        page_changed_tick = now;
        parking_slot_clear_changed();
        parking_ui_show_normal();
        last_refresh_tick = now;
        return;
    }

    if(diagnostic_enabled != 0U)
    {
        if((uint32_t)(now - last_refresh_tick) >= PARKING_UI_REFRESH_MS)
        {
            parking_ui_show_diagnostic();
            last_refresh_tick = now;
        }
        return;
    }

    if(parking_slot_has_changed() != 0U)
    {
        uint8_t new_mask = (uint8_t)(parking_slot_get_stable_mask() & 0x0FU);
        uint8_t occupied_now = (uint8_t)(new_mask & (uint8_t)(~current_occupied_mask) & 0x0FU);
        uint8_t released_now = (uint8_t)(current_occupied_mask & (uint8_t)(~new_mask) & 0x0FU);
        uint8_t i;

        /*
         * CHIEM/TRA luon phan anh trang thai 4 o.
         * Chi AUTO moi dem thay doi E->F/F->E vao VAO/RA.
         * TEST/DEMO duoc phep mo phong to hop o ma khong lam tang
         * bo dem luu luong thuc te.
         */
        if(current_mode == PARKING_UI_MODE_AUTO)
        {
            for(i = 0U; i < 4U; i++)
            {
                if((occupied_now & (uint8_t)(1U << i)) != 0U)
                {
                    if(session_vao < PARKING_UI_SESSION_MAX_COUNT) session_vao++;
                }
                if((released_now & (uint8_t)(1U << i)) != 0U)
                {
                    if(session_ra < PARKING_UI_SESSION_MAX_COUNT) session_ra++;
                }
            }
        }

        if(released_now != 0U)
        {
            last_released_mask = released_now;
        }
        current_occupied_mask = new_mask;
        current_page = PARKING_UI_PAGE_STATUS;
        page_changed_tick = now;
    }
    else if((uint32_t)(now - page_changed_tick) >= PARKING_UI_PAGE_TIME_MS)
    {
        current_page = (ParkingUiPage_t)(((uint8_t)current_page + 1U) %
                                         (uint8_t)PARKING_UI_PAGE_COUNT);
        page_changed_tick = now;
    }

    if((parking_slot_has_changed() == 0U) &&
       ((uint32_t)(now - last_refresh_tick) < PARKING_UI_REFRESH_MS))
    {
        return;
    }

    parking_ui_show_normal();
    parking_slot_clear_changed();
    last_refresh_tick = now;
}

void parking_ui_show_message(const char *line_1,
                             const char *line_2,
                             uint32_t duration_ms)
{
    parking_ui_copy_line(message_line_1, line_1);
    parking_ui_copy_line(message_line_2, line_2);

    parking_ui_write_line(0U, message_line_1);
    parking_ui_write_line(1U, message_line_2);

    if(duration_ms == 0U)
    {
        message_end_tick = 0U;
        message_active = 2U;
    }
    else
    {
        message_end_tick = HAL_GetTick() + duration_ms;
        message_active = 1U;
    }
}

static void parking_ui_show_normal(void)
{
    if(diagnostic_enabled != 0U)
    {
        parking_ui_show_diagnostic();
        return;
    }

    if(current_page == PARKING_UI_PAGE_GUIDANCE)
    {
        parking_ui_show_guidance();
    }
    else if(current_page == PARKING_UI_PAGE_COUNTER)
    {
        parking_ui_show_counter();
    }
    else if(current_page == PARKING_UI_PAGE_FLOW)
    {
        parking_ui_show_flow();
    }
    else
    {
        parking_ui_show_status();
    }
}

static void parking_ui_show_status(void)
{
    char line_1[17];
    char line_2[17];

    if(parking_slot_is_full() != 0U)
    {
        (void)snprintf(line_1,
                       sizeof(line_1),
                       "BAI DAY %s",
                       parking_ui_get_mode_text());
    }
    else
    {
        (void)snprintf(line_1,
                       sizeof(line_1),
                       "TRONG:%u/4 %s",
                       parking_slot_get_free_count(),
                       parking_ui_get_mode_text());
    }

    parking_ui_build_slot_line(line_2);
    parking_ui_write_line(0U, line_1);
    parking_ui_write_line(1U, line_2);
}

static void parking_ui_show_guidance(void)
{
    char line_1[17];
    char line_2[17];

    if(parking_slot_is_full() != 0U)
    {
        (void)snprintf(line_1, sizeof(line_1), "BAI XE DA DAY");
    }
    else
    {
        uint8_t suggested_slot = parking_guidance_find_available();

        (void)snprintf(line_1,
                       sizeof(line_1),
                       "MOI VAO O SO %u",
                       (uint8_t)(suggested_slot + 1U));
    }

    parking_ui_build_slot_line(line_2);
    parking_ui_write_line(0U, line_1);
    parking_ui_write_line(1U, line_2);
}

static void parking_ui_show_counter(void)
{
    char line_1[17];
    char line_2[17];

    (void)snprintf(line_1,
                   sizeof(line_1),
                   "VAO:%lu RA:%lu",
                   (unsigned long)session_vao,
                   (unsigned long)session_ra);

    (void)snprintf(line_2,
                   sizeof(line_2),
                   "24C16:%s",
                   ((parking_storage_is_ready() != 0U) &&
                    (parking_storage_last_save_ok() != 0U)) ? "OK" : "ERR");

    parking_ui_write_line(0U, line_1);
    parking_ui_write_line(1U, line_2);
}

static void parking_ui_show_flow(void)
{
    char line_1[17];
    char line_2[17];
    uint8_t occupied_mask = (uint8_t)(current_occupied_mask & 0x0FU);
    uint8_t released_mask = (uint8_t)(last_released_mask & 0x0FU);

    if(occupied_mask == 0U)
    {
        (void)snprintf(line_1, sizeof(line_1), "CHIEM:0");
    }
    else
    {
        uint8_t pos = 6U;
        uint8_t i;

        (void)snprintf(line_1, sizeof(line_1), "CHIEM:");
        for(i = 0U; i < 4U; i++)
        {
            if((occupied_mask & (uint8_t)(1U << i)) != 0U)
            {
                /* Hien thi ngan gon: CHIEM:1,2,3,4 */
                if(pos > 6U)
                {
                    if(pos < 16U)
                    {
                        line_1[pos++] = ',';
                    }
                }
                if(pos < 16U)
                {
                    line_1[pos++] = (char)('1' + i);
                }
            }
        }
        line_1[pos] = '\0';
    }

    if(released_mask == 0U)
    {
        (void)snprintf(line_2, sizeof(line_2), "TRA:0");
    }
    else
    {
        uint8_t pos = 4U;
        uint8_t i;

        (void)snprintf(line_2, sizeof(line_2), "TRA:");
        for(i = 0U; i < 4U; i++)
        {
            if((released_mask & (uint8_t)(1U << i)) != 0U)
            {
                /* Hien thi ngan gon: TRA:1,2,3,4 */
                if(pos > 4U)
                {
                    if(pos < 16U)
                    {
                        line_2[pos++] = ',';
                    }
                }
                if(pos < 16U)
                {
                    line_2[pos++] = (char)('1' + i);
                }
            }
        }
        line_2[pos] = '\0';
    }

    parking_ui_write_line(0U, line_1);
    parking_ui_write_line(1U, line_2);
}

static void parking_ui_show_diagnostic(void)
{
    char line_1[17];
    char line_2[17];

    parking_ui_build_mask_line(line_1,
                               "RAW :",
                               parking_slot_get_raw_mask());
    parking_ui_build_mask_line(line_2,
                               "STAB:",
                               parking_slot_get_stable_mask());

    parking_ui_write_line(0U, line_1);
    parking_ui_write_line(1U, line_2);
}

static void parking_ui_build_slot_line(char line[17])
{
    (void)snprintf(line,
                   17U,
                   "1:%c 2:%c 3:%c 4:%c",
                   (parking_slot_get_state(0U) == PARKING_SLOT_OCCUPIED) ? 'F' : 'E',
                   (parking_slot_get_state(1U) == PARKING_SLOT_OCCUPIED) ? 'F' : 'E',
                   (parking_slot_get_state(2U) == PARKING_SLOT_OCCUPIED) ? 'F' : 'E',
                   (parking_slot_get_state(3U) == PARKING_SLOT_OCCUPIED) ? 'F' : 'E');
}

static void parking_ui_build_mask_line(char line[17],
                                       const char *prefix,
                                       uint8_t mask)
{
    (void)snprintf(line,
                   17U,
                   "%s%c%c%c%c",
                   prefix,
                   ((mask & (1U << 0)) != 0U) ? '1' : '0',
                   ((mask & (1U << 1)) != 0U) ? '1' : '0',
                   ((mask & (1U << 2)) != 0U) ? '1' : '0',
                   ((mask & (1U << 3)) != 0U) ? '1' : '0');
}

static const char *parking_ui_get_mode_text(void)
{
    if(current_mode == PARKING_UI_MODE_TEST)
    {
        return "TEST";
    }

    if(current_mode == PARKING_UI_MODE_DEMO)
    {
        return "DEMO";
    }

    return "AUTO";
}

static void parking_ui_write_line(uint8_t row, const char *text)
{
    char padded[17];
    uint8_t index = 0U;

    while((index < 16U) && (text != 0) && (text[index] != '\0'))
    {
        padded[index] = text[index];
        index++;
    }

    while(index < 16U)
    {
        padded[index] = ' ';
        index++;
    }

    padded[16] = '\0';

    lcd_set_cursor(row, 0U);
    lcd_print(padded);
}

static void parking_ui_copy_line(char destination[17], const char *source)
{
    uint8_t index = 0U;

    if(source != 0)
    {
        while((index < 16U) && (source[index] != '\0'))
        {
            destination[index] = source[index];
            index++;
        }
    }

    destination[index] = '\0';
}
