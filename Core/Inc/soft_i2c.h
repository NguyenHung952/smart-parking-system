#ifndef SOFT_I2C_H_
#define SOFT_I2C_H_

#include <stdint.h>

typedef enum
{
    SOFT_I2C_ERROR = 0,
    SOFT_I2C_OK = 1
} SoftI2cStatus_t;

void soft_i2c_init(void);
SoftI2cStatus_t soft_i2c_bus_is_idle(void);
SoftI2cStatus_t soft_i2c_start(void);
void soft_i2c_stop(void);
SoftI2cStatus_t soft_i2c_write_byte(uint8_t data);
uint8_t soft_i2c_read_byte(uint8_t send_ack);

#endif /* SOFT_I2C_H_ */
