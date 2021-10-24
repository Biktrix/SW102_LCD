#pragma once

struct screen {
	void (*enter)();
	void (*idle)();
	void (*button)(int btn);
	void (*leave)();
};

void showScreen(const struct screen *new_screen);
void ui_update();

extern const struct screen *activeScreen;
