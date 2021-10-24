/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Released under the GPL License, Version 3
 */
extern "C" {
#include "main.h"
#include "button.h"
#include "buttons.h"
#include "lcd.h"
#include "ugui.h"
#include "fonts.h"
#include "uart.h"
#include "utils.h"
#include "rtc.h"
#include "eeprom.h"
#include "state.h"
#include "adc.h"
#include "fault.h"
#include "rtc.h"
#include "timer.h"
#include "ui.h"
}

#include <QApplication>
#include <QTimer>

extern const struct screen screen_boot;

/* Variable definition */

/* Buttons */
Button buttonM, buttonDWN, buttonUP, buttonPWR;

#define MSEC_PER_TICK 20

volatile uint32_t gui_ticks;

void SW102_rt_processing_stop(void) {
}

void SW102_rt_processing_start(void) {
}

void lcd_power_off(uint8_t updateDistanceOdo)
{
	QApplication::exit();
}


/**
 * Check if we should use the softdevice.
 */
void init_softdevice() {

}

static void gui_timer_timeout();

/**
 * @brief Application main entry.
 */
int main(int ac, char ** av)
{
	QApplication app(ac, av);
	
	lcd_init();
	uart_init();

	eeprom_init();


	QTimer idle;
	idle.setInterval(20);
	idle.start();
	QObject::connect(&idle, &QTimer::timeout, ui_update);

	showScreen(&screen_boot);

	QTimer isr;
	isr.setInterval(MSEC_PER_TICK);
	isr.start();
	QObject::connect(&isr, &QTimer::timeout, gui_timer_timeout);
	app.exec();
}

/* Hardware Initialization */
static void gui_timer_timeout()
{
	gui_ticks++;

	if(gui_ticks % (1000 / MSEC_PER_TICK) == 0)
		ui32_seconds_since_startup++;
	
	if((gui_ticks % (100 / MSEC_PER_TICK) == 0)) {
		rt_processing();
		// if we ever emulate BT, we should add this here
		// send_bluetooth(&rt_vars);
	}
}


/// msecs since boot (note: will roll over every 50 days)
uint32_t get_time_base_counter_1ms() {
	return gui_ticks * MSEC_PER_TICK;
}

uint32_t get_seconds() {
	return ui32_seconds_since_startup;
}

extern "C" {
void rt_graph_process()
{
}
void ui_motor_stabilized()
{
}

void set_conversions()
{
}

uint8_t g_showNextScreenIndex, g_showNextScreenPreviousIndex;
}
