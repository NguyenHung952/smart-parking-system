/*
 * lcd.h
 *
 *  Created on: Jul 8, 2026
 *      Author: nguye
 */

#ifndef __LCD_H
#define __LCD_H

#include "main.h"
void lcd_display_on(void);
void lcd_display_off(void);

void lcd_cursor_on(void);
void lcd_cursor_off(void);

void lcd_blink_on(void);
void lcd_blink_off(void);

void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_putc(char c);
void lcd_print(const char *str);
#endif
