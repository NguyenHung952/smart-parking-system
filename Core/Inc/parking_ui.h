#ifndef PARKING_UI_H_
#define PARKING_UI_H_

#include <stdint.h>

typedef enum
{
    PARKING_UI_MODE_AUTO = 0,
    PARKING_UI_MODE_TEST,
    PARKING_UI_MODE_DEMO
} ParkingUiMode_t;

void parking_ui_init(void);
void parking_ui_update(void);
void parking_ui_set_mode(ParkingUiMode_t mode);

/* Bat/tat trang chan doan RAW/STAB. */
void parking_ui_toggle_diagnostic(void);
uint8_t parking_ui_is_diagnostic_enabled(void);

/* Hien thong bao tam thoi, moi dong toi da 16 ky tu. */
void parking_ui_show_message(const char *line_1,
                             const char *line_2,
                             uint32_t duration_ms);

#endif /* PARKING_UI_H_ */
