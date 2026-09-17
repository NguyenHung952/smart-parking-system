#include "eeprom_24c16.h"

#include "main.h"
#include "soft_i2c.h"

#define EEPROM_24C16_CONTROL_BASE       0xA0U
#define EEPROM_24C16_WRITE_TIMEOUT_MS   20U
#define EEPROM_24C16_PAGE_SIZE          16U
#define EEPROM_24C16_TEST_ADDRESS       0x07FFU
#define EEPROM_24C16_TEST_VALUE         0xA5U

static uint8_t eeprom_24c16_control_byte(uint16_t address, uint8_t read_bit);
static Eeprom24c16Status_t eeprom_24c16_write_page(uint16_t address,
                                                   const uint8_t *data,
                                                   uint8_t length);
static Eeprom24c16Status_t eeprom_24c16_wait_ready(uint16_t address);

void eeprom_24c16_init(void)
{
    soft_i2c_init();
}

Eeprom24c16Status_t eeprom_24c16_scan(uint8_t *found_address)
{
    uint8_t address;

    if(found_address == 0)
    {
        return EEPROM_24C16_ERROR;
    }

    *found_address = 0xFFU;

    if(soft_i2c_bus_is_idle() != SOFT_I2C_OK)
    {
        return EEPROM_24C16_ERROR;
    }

    for(address = 0x50U; address <= 0x57U; address++)
    {
        if(soft_i2c_start() == SOFT_I2C_OK)
        {
            if(soft_i2c_write_byte((uint8_t)(address << 1U)) == SOFT_I2C_OK)
            {
                soft_i2c_stop();
                *found_address = address;
                return EEPROM_24C16_OK;
            }

            soft_i2c_stop();
        }

        HAL_Delay(1U);
    }

    return EEPROM_24C16_ERROR;
}

Eeprom24c16Status_t eeprom_24c16_is_ready(void)
{
    Eeprom24c16Status_t status = EEPROM_24C16_ERROR;

    if(soft_i2c_start() == SOFT_I2C_OK)
    {
        if(soft_i2c_write_byte(EEPROM_24C16_CONTROL_BASE) == SOFT_I2C_OK)
        {
            status = EEPROM_24C16_OK;
        }

        soft_i2c_stop();
    }

    return status;
}

Eeprom24c16Status_t eeprom_24c16_write_byte(uint16_t address, uint8_t data)
{
    return eeprom_24c16_write(address, &data, 1U);
}

Eeprom24c16Status_t eeprom_24c16_read_byte(uint16_t address, uint8_t *data)
{
    return eeprom_24c16_read(address, data, 1U);
}

Eeprom24c16Status_t eeprom_24c16_write(uint16_t address,
                                      const uint8_t *data,
                                      uint16_t length)
{
    uint16_t remaining = length;

    if((data == 0) || (length == 0U) ||
       (address >= EEPROM_24C16_SIZE_BYTES) ||
       (length > (uint16_t)(EEPROM_24C16_SIZE_BYTES - address)))
    {
        return EEPROM_24C16_ERROR;
    }

    while(remaining > 0U)
    {
        uint8_t page_offset = (uint8_t)(address % EEPROM_24C16_PAGE_SIZE);
        uint8_t page_space = (uint8_t)(EEPROM_24C16_PAGE_SIZE - page_offset);
        uint8_t chunk = (remaining < page_space) ?
                        (uint8_t)remaining : page_space;

        if(eeprom_24c16_write_page(address, data, chunk) != EEPROM_24C16_OK)
        {
            return EEPROM_24C16_ERROR;
        }

        address = (uint16_t)(address + chunk);
        data += chunk;
        remaining = (uint16_t)(remaining - chunk);
    }

    return EEPROM_24C16_OK;
}

Eeprom24c16Status_t eeprom_24c16_read(uint16_t address,
                                     uint8_t *data,
                                     uint16_t length)
{
    uint16_t index;

    if((data == 0) || (length == 0U) ||
       (address >= EEPROM_24C16_SIZE_BYTES) ||
       (length > (uint16_t)(EEPROM_24C16_SIZE_BYTES - address)))
    {
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_start() != SOFT_I2C_OK)
    {
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_write_byte(eeprom_24c16_control_byte(address, 0U)) !=
       SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_write_byte((uint8_t)address) != SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_start() != SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_write_byte(eeprom_24c16_control_byte(address, 1U)) !=
       SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return EEPROM_24C16_ERROR;
    }

    for(index = 0U; index < length; index++)
    {
        uint8_t send_ack = (index < (uint16_t)(length - 1U)) ? 1U : 0U;

        data[index] = soft_i2c_read_byte(send_ack);
    }

    soft_i2c_stop();

    return EEPROM_24C16_OK;
}

Eeprom24c16TestResult_t eeprom_24c16_self_test(void)
{
    uint8_t original_value;
    uint8_t read_value;
    Eeprom24c16TestResult_t test_result = EEPROM_24C16_TEST_OK;

    if(soft_i2c_bus_is_idle() != SOFT_I2C_OK)
    {
        return EEPROM_24C16_TEST_I2C_BUS_ERROR;
    }

    if(eeprom_24c16_is_ready() != EEPROM_24C16_OK)
    {
        return EEPROM_24C16_TEST_NO_ACK;
    }

    if(eeprom_24c16_read_byte(EEPROM_24C16_TEST_ADDRESS,
                              &original_value) != EEPROM_24C16_OK)
    {
        return EEPROM_24C16_TEST_READ_ERROR;
    }

    if(eeprom_24c16_write_byte(EEPROM_24C16_TEST_ADDRESS,
                               EEPROM_24C16_TEST_VALUE) != EEPROM_24C16_OK)
    {
        return EEPROM_24C16_TEST_WRITE_ERROR;
    }

    if(eeprom_24c16_read_byte(EEPROM_24C16_TEST_ADDRESS,
                              &read_value) != EEPROM_24C16_OK)
    {
        test_result = EEPROM_24C16_TEST_READ_ERROR;
    }
    else if(read_value != EEPROM_24C16_TEST_VALUE)
    {
        test_result = EEPROM_24C16_TEST_VERIFY_ERROR;
    }

    if(eeprom_24c16_write_byte(EEPROM_24C16_TEST_ADDRESS,
                               original_value) != EEPROM_24C16_OK)
    {
        return EEPROM_24C16_TEST_RESTORE_ERROR;
    }

    return test_result;
}

static uint8_t eeprom_24c16_control_byte(uint16_t address, uint8_t read_bit)
{
    uint8_t block_bits = (uint8_t)((address >> 7U) & 0x0EU);

    return (uint8_t)(EEPROM_24C16_CONTROL_BASE |
                     block_bits |
                     (read_bit & 0x01U));
}

static Eeprom24c16Status_t eeprom_24c16_write_page(uint16_t address,
                                                   const uint8_t *data,
                                                   uint8_t length)
{
    uint8_t index;

    if((data == 0) || (length == 0U) ||
       (length > EEPROM_24C16_PAGE_SIZE) ||
       (((address % EEPROM_24C16_PAGE_SIZE) + length) >
        EEPROM_24C16_PAGE_SIZE))
    {
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_start() != SOFT_I2C_OK)
    {
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_write_byte(eeprom_24c16_control_byte(address, 0U)) !=
       SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return EEPROM_24C16_ERROR;
    }

    if(soft_i2c_write_byte((uint8_t)address) != SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return EEPROM_24C16_ERROR;
    }

    for(index = 0U; index < length; index++)
    {
        if(soft_i2c_write_byte(data[index]) != SOFT_I2C_OK)
        {
            soft_i2c_stop();
            return EEPROM_24C16_ERROR;
        }
    }

    soft_i2c_stop();

    return eeprom_24c16_wait_ready(address);
}

static Eeprom24c16Status_t eeprom_24c16_wait_ready(uint16_t address)
{
    uint32_t start_tick = HAL_GetTick();

    while((uint32_t)(HAL_GetTick() - start_tick) <
          EEPROM_24C16_WRITE_TIMEOUT_MS)
    {
        if(soft_i2c_start() == SOFT_I2C_OK)
        {
            if(soft_i2c_write_byte(eeprom_24c16_control_byte(address, 0U)) ==
               SOFT_I2C_OK)
            {
                soft_i2c_stop();
                return EEPROM_24C16_OK;
            }

            soft_i2c_stop();
        }

        HAL_Delay(1U);
    }

    return EEPROM_24C16_ERROR;
}
