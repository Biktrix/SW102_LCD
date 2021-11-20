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
#include <stdio.h>

FILE *eeprom;

// returns true if our preferences were found
bool flash_read_words(void *dest, uint16_t length_words)
{
	if(!eeprom)
		return false;

	fseek(eeprom, 0, 0);
	fread(dest, length_words, 4, eeprom);
	return true;
}

bool flash_write_words(const void *value, uint16_t length_words)
{
	if(!eeprom)
		return false;
	fseek(eeprom, 0, 0);
	fwrite(value, length_words, 4, eeprom);
	fflush(eeprom);
	return true;
}


/**
 * @brief Init eeprom emulation system
 */
void eeprom_hw_init(void)
{
	eeprom = fopen("eeprom.bin", "r+b");
	if(!eeprom)
		eeprom = fopen("eeprom.bin", "w+b");
}

