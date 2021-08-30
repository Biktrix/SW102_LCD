/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Copyright (C) lowPerformer, 2019.
 *
 * Released under the GPL License, Version 3
 */

#include <string.h>
#include "common.h"
#include "uart.h"
#include "utils.h"
#include "assert.h"


uint8_t ui8_rx_buffer[UART_NUMBER_DATA_BYTES_TO_RECEIVE];
uint8_t ui8_tx_buffer[UART_NUMBER_DATA_BYTES_TO_SEND];
volatile uint8_t ui8_received_package_flag = 0;

uint8_t* uart_get_tx_buffer(void)
{
  return ui8_tx_buffer;
}

uint8_t* usart1_get_rx_buffer(void)
{
  return ui8_rx_buffer;
}

uint8_t usart1_received_package(void)
{
  return ui8_received_package_flag;
}

void usart1_reset_received_package(void)
{
  ui8_received_package_flag = 0;
}

const uint8_t* uart_get_rx_buffer_rdy(void)
{
    return NULL;
}

/**
 * @brief Send TX buffer over UART.
 */
void uart_send_tx_buffer(uint8_t *tx_buffer, uint8_t ui8_len)
{
}
