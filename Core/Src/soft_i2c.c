#include "soft_i2c.h"

#include "main.h"

#define SOFT_I2C_DELAY_US              20U
#define SOFT_I2C_SCL_TIMEOUT_LOOPS     10000U

static void soft_i2c_delay_us(uint32_t microseconds);
static SoftI2cStatus_t soft_i2c_release_scl(void);
static void soft_i2c_bus_recovery(void);

static void soft_i2c_sda_low(void)
{
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);
}

static void soft_i2c_sda_release(void)
{
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
}

static void soft_i2c_scl_low(void)
{
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
}

void soft_i2c_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    soft_i2c_sda_release();
    (void)soft_i2c_release_scl();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    if(HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) == GPIO_PIN_RESET)
    {
        soft_i2c_bus_recovery();
    }
}

SoftI2cStatus_t soft_i2c_bus_is_idle(void)
{
    soft_i2c_sda_release();

    if(soft_i2c_release_scl() != SOFT_I2C_OK)
    {
        return SOFT_I2C_ERROR;
    }

    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    if((HAL_GPIO_ReadPin(SCL_GPIO_Port, SCL_Pin) == GPIO_PIN_RESET) ||
       (HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) == GPIO_PIN_RESET))
    {
        return SOFT_I2C_ERROR;
    }

    return SOFT_I2C_OK;
}

SoftI2cStatus_t soft_i2c_start(void)
{
    soft_i2c_sda_release();

    if(soft_i2c_release_scl() != SOFT_I2C_OK)
    {
        return SOFT_I2C_ERROR;
    }

    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    if(HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) == GPIO_PIN_RESET)
    {
        return SOFT_I2C_ERROR;
    }

    soft_i2c_sda_low();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);
    soft_i2c_scl_low();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    return SOFT_I2C_OK;
}

void soft_i2c_stop(void)
{
    soft_i2c_sda_low();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    (void)soft_i2c_release_scl();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    soft_i2c_sda_release();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);
}

SoftI2cStatus_t soft_i2c_write_byte(uint8_t data)
{
    uint8_t bit_index;
    GPIO_PinState ack_state;

    for(bit_index = 0U; bit_index < 8U; bit_index++)
    {
        if((data & 0x80U) != 0U)
        {
            soft_i2c_sda_release();
        }
        else
        {
            soft_i2c_sda_low();
        }

        soft_i2c_delay_us(SOFT_I2C_DELAY_US);

        if(soft_i2c_release_scl() != SOFT_I2C_OK)
        {
            soft_i2c_stop();
            return SOFT_I2C_ERROR;
        }

        soft_i2c_delay_us(SOFT_I2C_DELAY_US);
        soft_i2c_scl_low();
        soft_i2c_delay_us(SOFT_I2C_DELAY_US);
        data <<= 1U;
    }

    soft_i2c_sda_release();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    if(soft_i2c_release_scl() != SOFT_I2C_OK)
    {
        soft_i2c_stop();
        return SOFT_I2C_ERROR;
    }

    soft_i2c_delay_us(SOFT_I2C_DELAY_US);
    ack_state = HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin);
    soft_i2c_scl_low();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    return (ack_state == GPIO_PIN_RESET) ? SOFT_I2C_OK : SOFT_I2C_ERROR;
}

uint8_t soft_i2c_read_byte(uint8_t send_ack)
{
    uint8_t bit_index;
    uint8_t data = 0U;

    soft_i2c_sda_release();

    for(bit_index = 0U; bit_index < 8U; bit_index++)
    {
        data <<= 1U;

        if(soft_i2c_release_scl() != SOFT_I2C_OK)
        {
            soft_i2c_stop();
            return 0xFFU;
        }

        soft_i2c_delay_us(SOFT_I2C_DELAY_US);

        if(HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) == GPIO_PIN_SET)
        {
            data |= 0x01U;
        }

        soft_i2c_scl_low();
        soft_i2c_delay_us(SOFT_I2C_DELAY_US);
    }

    if(send_ack != 0U)
    {
        soft_i2c_sda_low();
    }
    else
    {
        soft_i2c_sda_release();
    }

    soft_i2c_delay_us(SOFT_I2C_DELAY_US);
    (void)soft_i2c_release_scl();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);
    soft_i2c_scl_low();
    soft_i2c_sda_release();
    soft_i2c_delay_us(SOFT_I2C_DELAY_US);

    return data;
}

static SoftI2cStatus_t soft_i2c_release_scl(void)
{
    uint32_t timeout = SOFT_I2C_SCL_TIMEOUT_LOOPS;

    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);

    while(HAL_GPIO_ReadPin(SCL_GPIO_Port, SCL_Pin) == GPIO_PIN_RESET)
    {
        if(timeout == 0U)
        {
            return SOFT_I2C_ERROR;
        }

        timeout--;
    }

    return SOFT_I2C_OK;
}

static void soft_i2c_bus_recovery(void)
{
    uint8_t pulse;

    soft_i2c_sda_release();

    for(pulse = 0U; pulse < 9U; pulse++)
    {
        soft_i2c_scl_low();
        soft_i2c_delay_us(SOFT_I2C_DELAY_US);
        (void)soft_i2c_release_scl();
        soft_i2c_delay_us(SOFT_I2C_DELAY_US);
    }

    soft_i2c_stop();
}

static void soft_i2c_delay_us(uint32_t microseconds)
{
    uint32_t cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
    uint32_t wait_cycles = cycles_per_us * microseconds;
    uint32_t start = DWT->CYCCNT;

    while((uint32_t)(DWT->CYCCNT - start) < wait_cycles)
    {
        /* Busy wait for Software I2C timing. */
    }
}
