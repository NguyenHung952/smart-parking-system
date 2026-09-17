#include "rs232_comm.h"

#include "main.h"
#include "parking_gate.h"
#include "parking_slot.h"
#include "parking_storage.h"

#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

#define RS232_LINE_MAX_LEN       96U
#define RS232_RX_QUEUE_SIZE      128U
#define RS232_TX_TIMEOUT_MS      100U

static volatile uint8_t rs232_rx_byte;
static volatile uint8_t rs232_rx_queue[RS232_RX_QUEUE_SIZE];
static volatile uint16_t rs232_rx_head;
static volatile uint16_t rs232_rx_tail;
static volatile uint8_t rs232_ready;

static char rs232_line[RS232_LINE_MAX_LEN];
static uint16_t rs232_line_length;

static void rs232_send_line(const char *text);
static void rs232_send_ok(const char *message);
static void rs232_send_error(const char *message);
static void rs232_process_byte(uint8_t byte);
static void rs232_process_command(const char *command);
static uint8_t rs232_command_is_complete(const char *command);
static const char *rs232_gate_state_to_string(ParkingGateState_t state);

/*
 * USART1 IRQ is intentionally kept inside this RS232 module so that the
 * rest of the project does not need to be modified for command reception.
 * stm32f2xx_it.c in the current project has no USART1_IRQHandler().
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* HAL callback: receive exactly one byte, then immediately arm RX again. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if((huart == &huart1) && (huart->Instance == USART1))
    {
        uint16_t next_head = (uint16_t)((rs232_rx_head + 1U) % RS232_RX_QUEUE_SIZE);

        if(next_head != rs232_rx_tail)
        {
            rs232_rx_queue[rs232_rx_head] = rs232_rx_byte;
            rs232_rx_head = next_head;
        }

        /* Re-arm RX immediately for the next byte. */
        (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&rs232_rx_byte, 1U);
    }
}

/* Re-arm reception after a UART error (overrun/noise/framing, etc.). */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if((huart == &huart1) && (huart->Instance == USART1))
    {
        (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&rs232_rx_byte, 1U);
    }
}

void rs232_init(void)
{
    rs232_rx_head = 0U;
    rs232_rx_tail = 0U;
    rs232_line_length = 0U;
    rs232_ready = 0U;
    memset(rs232_line, 0, sizeof(rs232_line));

    if(huart1.Instance != USART1)
    {
        return;
    }

    /* Enable only the USART1 interrupt required by this RS232 module. */
    HAL_NVIC_SetPriority(USART1_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    /* Arm reception of the first byte. */
    if(HAL_UART_Receive_IT(&huart1, (uint8_t *)&rs232_rx_byte, 1U) != HAL_OK)
    {
        return;
    }

    rs232_ready = 1U;

    rs232_send_line("SMART_PARKING_RS232_READY");
    rs232_send_line("BAUD=115200,8N1");
    rs232_send_line("TYPE=HELP_FOR_COMMANDS");
    rs232_send_status();
}

void rs232_process(void)
{
    uint8_t byte;

    if(rs232_ready == 0U)
    {
        return;
    }

    /* Move received bytes from the interrupt queue into the command parser. */
    while(rs232_rx_tail != rs232_rx_head)
    {
        byte = rs232_rx_queue[rs232_rx_tail];
        rs232_rx_tail = (uint16_t)((rs232_rx_tail + 1U) % RS232_RX_QUEUE_SIZE);
        rs232_process_byte(byte);
    }
}

void rs232_send_text(const char *text)
{
    if((rs232_ready == 0U) || (text == NULL))
    {
        return;
    }

    rs232_send_line(text);
}

void rs232_send_status(void)
{
    char line[RS232_LINE_MAX_LEN];
    uint8_t mask;
    uint8_t slot1;
    uint8_t slot2;
    uint8_t slot3;
    uint8_t slot4;
    uint8_t occupied;
    uint8_t free_count;
    uint32_t entry_count;
    uint32_t exit_count;
    ParkingGateState_t gate_state;

    if(rs232_ready == 0U)
    {
        return;
    }

    mask = parking_slot_get_stable_mask();

    slot1 = ((mask & 0x01U) != 0U) ? 1U : 0U;
    slot2 = ((mask & 0x02U) != 0U) ? 1U : 0U;
    slot3 = ((mask & 0x04U) != 0U) ? 1U : 0U;
    slot4 = ((mask & 0x08U) != 0U) ? 1U : 0U;

    occupied = parking_slot_get_occupied_count();
    free_count = parking_slot_get_free_count();
    entry_count = parking_storage_get_total_entry();
    exit_count = parking_storage_get_total_exit();
    gate_state = parking_gate_get_state();

    (void)snprintf(line,
                   sizeof(line),
                   "STATUS,S1=%u,S2=%u,S3=%u,S4=%u,FREE=%u,OCC=%u,IN=%lu,OUT=%lu,GATE=%s",
                   (unsigned)slot1,
                   (unsigned)slot2,
                   (unsigned)slot3,
                   (unsigned)slot4,
                   (unsigned)free_count,
                   (unsigned)occupied,
                   (unsigned long)entry_count,
                   (unsigned long)exit_count,
                   rs232_gate_state_to_string(gate_state));

    rs232_send_line(line);
}

uint8_t rs232_is_ready(void)
{
    return rs232_ready;
}

static void rs232_process_byte(uint8_t byte)
{
    /* Hercules may send CR, LF, or CR+LF when Enter is pressed. */
    if((byte == '\r') || (byte == '\n'))
    {
        if(rs232_line_length > 0U)
        {
            rs232_line[rs232_line_length] = '\0';
            rs232_process_command(rs232_line);
            rs232_line_length = 0U;
            rs232_line[0] = '\0';
        }
        return;
    }

    /* Ignore control bytes other than CR/LF. */
    if((byte < 0x20U) || (byte > 0x7EU))
    {
        return;
    }

    /* Normalize lower-case ASCII to upper-case. */
    if((byte >= 'a') && (byte <= 'z'))
    {
        byte = (uint8_t)(byte - ('a' - 'A'));
    }

    if(rs232_line_length >= (RS232_LINE_MAX_LEN - 1U))
    {
        rs232_line_length = 0U;
        rs232_line[0] = '\0';
        rs232_send_error("COMMAND_TOO_LONG");
        return;
    }

    rs232_line[rs232_line_length++] = (char)byte;
    rs232_line[rs232_line_length] = '\0';

    /* Commands are executed immediately, so Hercules does not need EOL. */
    if(rs232_command_is_complete(rs232_line) != 0U)
    {
        rs232_process_command(rs232_line);
        rs232_line_length = 0U;
        rs232_line[0] = '\0';
    }
}

static uint8_t rs232_command_is_complete(const char *command)
{
    if(command == NULL)
    {
        return 0U;
    }

    return (strcmp(command, "PING") == 0) ||
           (strcmp(command, "GET_STATUS") == 0) ||
           (strcmp(command, "STATUS") == 0) ||
           (strcmp(command, "HELP") == 0) ||
           (strcmp(command, "OPEN_GATE_IN") == 0) ||
           (strcmp(command, "OPEN_GATE_OUT") == 0);
}

static void rs232_process_command(const char *command)
{
    if(command == NULL)
    {
        return;
    }

    if(strcmp(command, "PING") == 0)
    {
        rs232_send_ok("PONG");
    }
    else if((strcmp(command, "GET_STATUS") == 0) ||
            (strcmp(command, "STATUS") == 0))
    {
        rs232_send_status();
    }
    else if(strcmp(command, "HELP") == 0)
    {
        rs232_send_line("COMMANDS:");
        rs232_send_line("PING");
        rs232_send_line("GET_STATUS");
        rs232_send_line("OPEN_GATE_IN");
        rs232_send_line("OPEN_GATE_OUT");
        rs232_send_line("HELP");
    }
    else if(strcmp(command, "OPEN_GATE_IN") == 0)
    {
        if(parking_gate_is_entry_allowed() != 0U)
        {
            parking_gate_open_entry();
            rs232_send_ok("GATE_IN_OPEN");
            rs232_send_status();
        }
        else
        {
            rs232_send_error("GATE_IN_NOT_ALLOWED");
        }
    }
    else if(strcmp(command, "OPEN_GATE_OUT") == 0)
    {
        if(parking_gate_is_exit_allowed() != 0U)
        {
            parking_gate_open_exit();
            rs232_send_ok("GATE_OUT_OPEN");
            rs232_send_status();
        }
        else
        {
            rs232_send_error("GATE_OUT_NOT_ALLOWED");
        }
    }
    else
    {
        rs232_send_error("UNKNOWN_COMMAND");
        rs232_send_line("TYPE=HELP");
    }
}

static void rs232_send_line(const char *text)
{
    char buffer[RS232_LINE_MAX_LEN + 2U];
    size_t length;

    if((rs232_ready == 0U) || (text == NULL))
    {
        return;
    }

    length = strlen(text);
    if(length > RS232_LINE_MAX_LEN)
    {
        length = RS232_LINE_MAX_LEN;
    }

    memcpy(buffer, text, length);
    buffer[length++] = '\r';
    buffer[length++] = '\n';

    (void)HAL_UART_Transmit(&huart1,
                           (uint8_t *)buffer,
                           (uint16_t)length,
                           RS232_TX_TIMEOUT_MS);
}

static void rs232_send_ok(const char *message)
{
    char line[RS232_LINE_MAX_LEN];

    if(message == NULL)
    {
        return;
    }

    (void)snprintf(line, sizeof(line), "OK,%s", message);
    rs232_send_line(line);
}

static void rs232_send_error(const char *message)
{
    char line[RS232_LINE_MAX_LEN];

    if(message == NULL)
    {
        return;
    }

    (void)snprintf(line, sizeof(line), "ERROR,%s", message);
    rs232_send_line(line);
}

static const char *rs232_gate_state_to_string(ParkingGateState_t state)
{
    switch(state)
    {
        case PARKING_GATE_STATE_WAIT_PARK:
            return "WAIT_PARK";

        case PARKING_GATE_STATE_WAIT_EXIT:
            return "WAIT_EXIT";

        case PARKING_GATE_STATE_IDLE:
        default:
            return "IDLE";
    }
}
