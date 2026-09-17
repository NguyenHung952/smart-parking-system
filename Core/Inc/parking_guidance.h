#ifndef PARKING_GUIDANCE_H_
#define PARKING_GUIDANCE_H_

#include <stdint.h>

#define PARKING_GUIDANCE_NO_SLOT    0xFFU

/*
 * Tra ve chi so o trong duoc de xuat (0...3).
 * Uu tien o co so thu tu nho nhat de thuat toan de kiem tra va trinh bay.
 * Neu bai xe day, ham tra ve PARKING_GUIDANCE_NO_SLOT.
 */
uint8_t parking_guidance_find_available(void);

#endif /* PARKING_GUIDANCE_H_ */
