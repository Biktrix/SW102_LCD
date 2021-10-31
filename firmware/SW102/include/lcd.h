/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Copyright (C) lowPerformer, 2019.
 *
 * Released under the GPL License, Version 3
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void lcd_init(void);
void lcd_refresh(void); // Call to flush framebuffer to SPI device
void lcd_set_backlight_intensity(uint8_t level);

// for now this has sufficient performance
// as an improvement, we could change OLED RAM organization to vertical addressing mode
// then the framebuffer would be entirely linear, albeit in a vertical orientation
// this would allow for efficient blit routines
extern uint8_t frameBuffer[16][64];

static __attribute__((always_inline)) inline void lcd_pset(unsigned int x, unsigned int y, bool v) {
	uint8_t page = y / 8;
	uint8_t pixel = y % 8;

	if (v)
		frameBuffer[page][x] |= 1<<pixel;
	else
		frameBuffer[page][x] &= ~(1<<pixel);
}
