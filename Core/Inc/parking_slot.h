#ifndef PARKING_SLOT_H_
#define PARKING_SLOT_H_

#include <stdint.h>

#define PARKING_SLOT_COUNT              4U

/*
 * Thoi gian tin hieu cam bien phai on dinh truoc khi chap nhan thay doi.
 * Co the tang len 100...200 ms neu cam bien thuc te bi nhieu.
 */
#define PARKING_SENSOR_DEBOUNCE_MS      50U

typedef enum
{
    PARKING_SLOT_EMPTY = 0,
    PARKING_SLOT_OCCUPIED
} ParkingSlotState_t;

typedef enum
{
    PARKING_INPUT_MANUAL = 0,
    PARKING_INPUT_SENSOR
} ParkingInputMode_t;

void parking_slot_init(void);
void parking_slot_update(void);

void parking_slot_set_input_mode(ParkingInputMode_t mode);
ParkingInputMode_t parking_slot_get_input_mode(void);

/* Moi lan goi se dao trang thai o: TRONG <-> CO XE. */
void parking_slot_toggle_virtual(uint8_t slot_index);

/* Dat dong thoi 4 o mo phong. Bit 0...3 tuong ung o 1...4. */
void parking_slot_set_virtual_mask(uint8_t occupied_mask);

/*
 * Khoi phuc stable/raw/virtual theo mask da duoc chap nhan.
 * Dung de loai bo thay doi o xay ra khi chua duoc phep boi Gate State Machine.
 */
void parking_slot_restore_mask(uint8_t occupied_mask);

/* Quyền thay đổi slot do Gate cấp: chỉ cho phép đúng chiều IN/OUT. */
typedef enum
{
    PARKING_SLOT_CHANGE_NONE = 0,
    PARKING_SLOT_CHANGE_ENTRY,
    PARKING_SLOT_CHANGE_EXIT
} ParkingSlotChangePermission_t;

void parking_slot_set_change_permission(ParkingSlotChangePermission_t permission);

/* Test bang nut vat ly DI.0-DI.3: moi lan nhan dao trang thai 1 o. */
void parking_slot_set_button_test_enabled(uint8_t enabled);

ParkingSlotState_t parking_slot_get_state(uint8_t slot_index);
uint8_t parking_slot_get_raw_mask(void);
uint8_t parking_slot_get_stable_mask(void);
uint8_t parking_slot_get_free_count(void);
uint8_t parking_slot_get_occupied_count(void);
uint8_t parking_slot_is_full(void);

/* Tra ve 1 khi co it nhat mot o vua thay doi trang thai. */
uint8_t parking_slot_has_changed(void);
void parking_slot_clear_changed(void);

#endif /* PARKING_SLOT_H_ */
