#ifndef PARKING_ALERT_H_
#define PARKING_ALERT_H_

#include <stdint.h>

typedef enum
{
    PARKING_ALERT_EVENT_NONE = 0,
    PARKING_ALERT_EVENT_BECAME_FULL
} ParkingAlertEvent_t;

void parking_alert_init(void);

/*
 * Cap nhat LED5, LED6 va chuoi coi canh bao khong dung HAL_Delay().
 * LED5 sang khi bai day; LED6 sang khi bai con cho.
 */
ParkingAlertEvent_t parking_alert_update(void);

/* Tat chuoi coi hien tai, khong lam thay doi trang thai LED. */
void parking_alert_silence(void);

#endif /* PARKING_ALERT_H_ */
