#ifndef PARKING_GATE_H_
#define PARKING_GATE_H_

#include <stdint.h>

/*
 * GIAI DOAN TEST:
 *   DI.4 / PE9 -> CỔNG IN
 *   DI.5 / PE8 -> CỔNG OUT
 *
 * DI.0-DI.3 vẫn là 4 ô đỗ. Gate chỉ khởi động quy trình IN/OUT;
 * module parking_slot chỉ cho phép đúng chiều 0->1 hoặc 1->0 theo Gate.
 */
void parking_gate_init(void);
void parking_gate_update(void);

uint8_t parking_gate_in_event(void);
uint8_t parking_gate_out_event(void);
void parking_gate_clear_events(void);

uint8_t parking_gate_is_entry_allowed(void);
uint8_t parking_gate_is_exit_allowed(void);

/* Mô phỏng mở cổng; sau này có thể thay bằng driver servo. */
void parking_gate_open_entry(void);
void parking_gate_open_exit(void);
uint8_t parking_gate_entry_is_open(void);
uint8_t parking_gate_exit_is_open(void);

/* Sự kiện hoàn tất: chỉ phát sinh sau khi đúng CỔNG + SLOT. */
typedef enum
{
    PARKING_GATE_COMPLETED_NONE = 0,
    PARKING_GATE_COMPLETED_ENTRY,
    PARKING_GATE_COMPLETED_EXIT
} ParkingGateCompletedEvent_t;

/* State Machine cổng. */
typedef enum
{
    PARKING_GATE_STATE_IDLE = 0,
    PARKING_GATE_STATE_WAIT_PARK,
    PARKING_GATE_STATE_WAIT_EXIT
} ParkingGateState_t;

ParkingGateState_t parking_gate_get_state(void);
uint8_t parking_gate_get_event_slot(void); /* 0..3, hoặc 0xFF nếu chưa có */
uint8_t parking_gate_has_completed_event(void);
ParkingGateCompletedEvent_t parking_gate_get_completed_event(void);
void parking_gate_clear_completed_event(void);

#endif /* PARKING_GATE_H_ */
