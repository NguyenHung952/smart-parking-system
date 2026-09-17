#ifndef RS232_COMM_H_
#define RS232_COMM_H_

#include <stdint.h>

/*
 * RS232 PC communication using the ARM KIT USART1 interface.
 *
 * ARM KIT:
 *   PA9  = USART1_TX -> RS232 DB9 pin 2 (ARM TXD)
 *   PA10 = USART1_RX -> RS232 DB9 pin 3 (ARM RXD)
 *   GND              -> RS232 DB9 pin 5
 *
 * UART: 115200, 8 data bits, no parity, 1 stop bit, no flow control.
 *
 * RX uses the existing USART1 peripheral in interrupt mode. This keeps
 * command reception independent of the parking application loop timing.
 * No parking module or CubeMX peripheral configuration is changed here.
 */

void rs232_init(void);
void rs232_process(void);

/* Send a text line to the PC. CR/LF is appended automatically. */
void rs232_send_text(const char *text);

/* Send the current parking status. */
void rs232_send_status(void);

/* Non-zero after USART1 RS232 communication has been initialized. */
uint8_t rs232_is_ready(void);

#endif /* RS232_COMM_H_ */
