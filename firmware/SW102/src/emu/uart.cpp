/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Copyright (C) lowPerformer, 2019.
 *
 * Released under the GPL License, Version 3
 */
extern "C" {
#include <string.h>
#include "common.h"
#include "uart.h"
#include "utils.h"
#include "assert.h"

extern uint16_t emu_voltage;
}

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>

static QSerialPort port;

void uart_init() 
{
	for(auto & pi: QSerialPortInfo::availablePorts()) {
		if(pi.portName().startsWith("ttyUSB")) {
			port.setPort(pi);
			port.setBaudRate(19200);
			if(port.open(QIODevice::ReadWrite)) {
				qDebug() << "Opened" << pi.portName();
				emu_voltage = 480;
				return;
			}
		}
	}

	qDebug() << "No USB UART connected";
}
static uint8_t ui8_rx[UART_NUMBER_DATA_BYTES_TO_RECEIVE];
static uint8_t ui8_rx_cnt = 0;
static uint8_t ui8_tx_buffer[UART_NUMBER_DATA_BYTES_TO_SEND];
volatile uint8_t ui8_received_package_flag = 0;

uint8_t* uart_get_tx_buffer(void)
{
	return ui8_tx_buffer;
}

/**
 * @brief Send TX buffer over UART.
 */
void uart_send_tx_buffer(uint8_t *tx_buffer, uint8_t ui8_len)
{
	port.write((char*)tx_buffer, ui8_len);
}

static uint8_t ui8_state_machine;

static const uint8_t *uart_process_rx(uint8_t ui8_byte_received)
{
	switch (ui8_state_machine)
	{
		case 0:
		if (ui8_byte_received == 0x43) { // see if we get start package byte
			ui8_rx[0] = ui8_byte_received;
			ui8_state_machine = 1;
		}
		else {
			ui8_state_machine = 0;
		}

		ui8_rx_cnt = 0;
		break;

		case 1:
			ui8_rx[1] = ui8_byte_received;
			ui8_state_machine = 2;
		break;

		case 2:
		ui8_rx[ui8_rx_cnt + 2] = ui8_byte_received;
		++ui8_rx_cnt;

		// reset if it is the last byte of the package and index is out of bounds
		if (ui8_rx_cnt >= ui8_rx[1])
		{
			ui8_state_machine = 0;

			// just to make easy next calculations
			uint16_t ui16_crc_rx = 0xffff;
			for (uint8_t ui8_i = 0; ui8_i < ui8_rx[1]; ui8_i++)
			{
				crc16(ui8_rx[ui8_i], &ui16_crc_rx);
			}

			// if CRC is correct read the package
			if (((((uint16_t) ui8_rx[ui8_rx[1] + 1]) << 8) +
						((uint16_t) ui8_rx[ui8_rx[1]])) == ui16_crc_rx)
			{
				// store the received data to rx_buffer
				return ui8_rx;
			}
		}
		break;

		default:
			ui8_state_machine = 0;
			break;
	}

	return NULL;
}
#include <unistd.h>
const uint8_t* uart_get_rx_buffer_rdy(void)
{
	char c;
	if(!port.isOpen())
		return NULL;

	port.waitForReadyRead(0);
	while(port.getChar(&c)) {
		const uint8_t *r = uart_process_rx((uint8_t)c);
		if(r)
			return r;
	}
	return NULL;
}


