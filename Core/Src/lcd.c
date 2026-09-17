/*
 * lcd.c
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */


#include "main.h"

//lcd.c
//│
//├── lcd_send_cmd()
//├── lcd_send_data()
//├── lcd_write_bus()
//└── lcd_enable_pulse()

static GPIO_TypeDef * const ports[] =
{
    GPIOD,
    GPIOD,
    GPIOD,
    GPIOD
};

static const uint16_t pins[] =
{
    GPIO_PIN_0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3
};

/*=========================================================
 * Private Functions
 *========================================================*/

static void lcd_write_bus(uint8_t nibble)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t bit = (nibble >> i) & 0x01;

        HAL_GPIO_WritePin(
            ports[i],
            pins[i],
            (GPIO_PinState)bit);
    }
}

static void lcd_enable_pulse(void)
{
    HAL_GPIO_WritePin(E_GPIO_Port, E_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(E_GPIO_Port, E_Pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(E_GPIO_Port, E_Pin, GPIO_PIN_RESET);
}

static void lcd_send_byte(uint8_t value)
{
    uint8_t high_nibble = value >> 4;
    uint8_t low_nibble  = value & 0x0F;

    lcd_write_bus(high_nibble);
    lcd_enable_pulse();

    lcd_write_bus(low_nibble);
    lcd_enable_pulse();
}

static void lcd_send_cmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(RS_GPIO_Port, RS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RW_GPIO_Port, RW_Pin, GPIO_PIN_RESET);

    lcd_send_byte(cmd);
}

static void lcd_send_data(uint8_t data)
{
    HAL_GPIO_WritePin(RS_GPIO_Port, RS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RW_GPIO_Port, RW_Pin, GPIO_PIN_RESET);

    lcd_send_byte(data);
}


//     Public API
//────────────────────────────────────
void lcd_init(void)
{
    HAL_Delay(40);

    HAL_GPIO_WritePin(RS_GPIO_Port, RS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RW_GPIO_Port, RW_Pin, GPIO_PIN_RESET);

    // LCD đang ở mode 8-bit
    lcd_write_bus(0x03);
    lcd_enable_pulse();
    HAL_Delay(5);

    lcd_write_bus(0x03);
    lcd_enable_pulse();
    HAL_Delay(1);

    lcd_write_bus(0x03);
    lcd_enable_pulse();
    HAL_Delay(1);

    // Chuyển sang mode 4-bit
    lcd_write_bus(0x02);
    lcd_enable_pulse();
    HAL_Delay(1);

    // Từ đây mới được dùng lcd_send_cmd()
    lcd_send_cmd(0x28);
    lcd_send_cmd(0x0C);
    lcd_send_cmd(0x06);
    lcd_send_cmd(0x01);

    HAL_Delay(2);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);

    HAL_Delay(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if(row == 0)
    {
        address = 0x80 + col;
    }
    else
    {
        address = 0xC0 + col;
    }

    lcd_send_cmd(address);
}


void lcd_putc(char c)
{
    lcd_send_data((uint8_t)c);
}

void lcd_print(const char *str)
{
    while(*str)
    {
        lcd_putc(*str);

        str++;
    }
}

/*==============================*/
/* Display Control              */
/*==============================*/

void lcd_display_on(void)
{
    lcd_send_cmd(0x0C);
}

void lcd_display_off(void)
{
    lcd_send_cmd(0x08);
}

/*==============================*/
/* Cursor Control               */
/*==============================*/

void lcd_cursor_on(void)
{
    lcd_send_cmd(0x0E);
}

void lcd_cursor_off(void)
{
    lcd_send_cmd(0x0C);
}

/*==============================*/
/* Blink Control                */
/*==============================*/

void lcd_blink_on(void)
{
    lcd_send_cmd(0x0F);
}

void lcd_blink_off(void)
{
    lcd_send_cmd(0x0E);
}
