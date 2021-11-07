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

extern union framebuffer_t {
	uint8_t u8[128*64/8];
	uint32_t u32[128*64/32];
} framebuffer;

static __attribute__((always_inline)) inline void lcd_pset(unsigned int x, unsigned int y, bool v) {
	if(v)
		framebuffer.u8[x*(128/8) + (y/8)] |= 1<<(y&7);
	else
		framebuffer.u8[x*(128/8) + (y/8)] &= ~(1<<(y&7));
}
