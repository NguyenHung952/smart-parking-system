#ifndef EEPROM_24C16_H_
#define EEPROM_24C16_H_

#include <stdint.h>

#define EEPROM_24C16_SIZE_BYTES       2048U

typedef enum
{
    EEPROM_24C16_ERROR = 0,
    EEPROM_24C16_OK = 1
} Eeprom24c16Status_t;

typedef enum
{
    EEPROM_24C16_TEST_OK = 0,
    EEPROM_24C16_TEST_I2C_BUS_ERROR = 1,
    EEPROM_24C16_TEST_NO_ACK = 2,
    EEPROM_24C16_TEST_READ_ERROR = 3,
    EEPROM_24C16_TEST_WRITE_ERROR = 4,
    EEPROM_24C16_TEST_VERIFY_ERROR = 5,
    EEPROM_24C16_TEST_RESTORE_ERROR = 6
} Eeprom24c16TestResult_t;

void eeprom_24c16_init(void);
Eeprom24c16Status_t eeprom_24c16_scan(uint8_t *found_address);
Eeprom24c16Status_t eeprom_24c16_is_ready(void);
Eeprom24c16Status_t eeprom_24c16_write(uint16_t address,
                                      const uint8_t *data,
                                      uint16_t length);
Eeprom24c16Status_t eeprom_24c16_read(uint16_t address,
                                     uint8_t *data,
                                     uint16_t length);
Eeprom24c16Status_t eeprom_24c16_write_byte(uint16_t address, uint8_t data);
Eeprom24c16Status_t eeprom_24c16_read_byte(uint16_t address, uint8_t *data);
Eeprom24c16TestResult_t eeprom_24c16_self_test(void);

#endif /* EEPROM_24C16_H_ */
