#ifndef PARKING_STORAGE_H_
#define PARKING_STORAGE_H_

#include <stdint.h>

/* AUTO: moi lan RESET MCU se xoa lich su CHIEM/TRẢ ve 0, nhung giu slot_mask. */
void parking_storage_init(void);
/* TEST/DEMO: doc EEPROM nhung khong dong bo/ghi mask mo phong. */
void parking_storage_init_read_only(void);
void parking_storage_update(void);

/* Dat trang thai hien tai lam baseline, khong tao IN/OUT. */
void parking_storage_set_baseline_mask(uint8_t mask);

/* Chi goi sau khi Gate State Machine da xac nhan xe vao/ra hop le. */
void parking_storage_record_entry(uint8_t slot);
void parking_storage_record_exit(uint8_t slot);

uint8_t parking_storage_is_ready(void);
uint8_t parking_storage_last_save_ok(void);

/*
 * Hai bo dem nay la SO LUOT XE QUA CONG:
 *   entry: chi tang sau CỔNG VÀO + SLOT hợp lệ.
 *   exit : chi tang sau CỔNG RA + SLOT hợp lệ.
 */
uint32_t parking_storage_get_slot_occupied_count(void);
uint32_t parking_storage_get_slot_released_count(void);
uint8_t parking_storage_get_last_mask(void);

/* API cu giu lai de khong lam hong code phu thuoc neu co. */
uint32_t parking_storage_get_total_entry(void);
uint32_t parking_storage_get_total_exit(void);

#endif /* PARKING_STORAGE_H_ */
