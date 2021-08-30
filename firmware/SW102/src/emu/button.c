/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Copyright (C) lowPerformer, 2019.
 *
 * Released under the GPL License, Version 3
 */
#include "button.h"

/**
 * @brief Process button struct. Call every 10 ms.
 */
bool PollButton(Button* button)
{
	return button->is_pressed;
}


