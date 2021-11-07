#pragma once

struct screen {
	void (*enter)();
	void (*idle)();
	void (*button)(int btn);
	void (*leave)();
};

// we extend buttons.h
enum {
	ONOFF_PRESS  =0x0002000, 
	UP_PRESS     =0x0004000,
	DOWN_PRESS   =0x0008000,
	M_PRESS      =0x0010000,
	
	ONOFF_RELEASE=0x0020000,
	UP_RELEASE   =0x0040000,
	DOWN_RELEASE =0x0080000,
	M_RELEASE    =0x0100000,
};

void showScreen(const struct screen *new_screen);
void ui_update();

extern int tick;
extern const struct screen *activeScreen;
