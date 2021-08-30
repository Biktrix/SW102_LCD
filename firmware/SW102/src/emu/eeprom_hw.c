/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Copyright (C) lowPerformer, 2019.
 *
 * Released under the GPL License, Version 3
 */

#include <string.h>
#include "eeprom_hw.h"
#include "common.h"
#include "assert.h"

// returns true if our preferences were found
bool flash_read_words(void *dest, uint16_t length_words)
{
	return false;
}

bool flash_write_words(const void *value, uint16_t length_words)
{
	return true;
}


/**
 * @brief Init eeprom emulation system
 */
void eeprom_hw_init(void)
{
}

