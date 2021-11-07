/*
 * Bafang LCD SW102 Bluetooth firmware
 *
 * Copyright (C) lowPerformer, 2019.
 *
 * Released under the GPL License, Version 3
 */

/* SPI timings SH1107 data sheet p. 52 (we are on Vdd 3.3V) */

/* Transferring the frame buffer by none-blocking SPI Transaction Manager showed that the CPU is blocked for the period of transaction
 * by ISR and library management because of very fast IRQ cadence.
 * Therefore we use standard blocking SPI transfer right away and save some complexity and flash space.
 */
extern "C"  {
#include "lcd.h"
#include "common.h"
#include "button.h"
union framebuffer_t framebuffer;
}

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QKeyEvent>
extern Button buttonM, buttonDWN, buttonUP, buttonPWR;

class OLEDWidget: public QWidget {
public:
	QImage display;
	bool display_on = true;
	OLEDWidget(): display(64, 128, QImage::Format_Grayscale8) {
		setMinimumSize(display.size());
		setMaximumSize(display.size());
	}
	~OLEDWidget() {
	}

protected:
	virtual void paintEvent(QPaintEvent *) {
		QPainter painter(this);
		painter.drawImage(QRect(0, 0, width(), height()), display, QRect(0, 0, display.width(), display.height()));
	}

	Button *getButton(QKeyEvent *evt) {
		if(evt->key() == Qt::Key_Up)
			return &buttonUP;
		if(evt->key() == Qt::Key_Down)
			return &buttonDWN;
		if(evt->key() == Qt::Key_M)
			return &buttonM;
		if(evt->key() == Qt::Key_P)
			return &buttonPWR;

		return NULL;
	}

	virtual void keyPressEvent(QKeyEvent *evt) {
		Button *btn = getButton(evt);
		if(btn)
			btn->is_pressed = true;
	}

	virtual void keyReleaseEvent(QKeyEvent *evt) {
		Button *btn = getButton(evt);
		if(btn)
			btn->is_pressed = false;
	}
} *oled;

/**
 * @brief LCD initialization including hardware layer.
 */
extern "C" void lcd_init(void)
{
	oled = new OLEDWidget;
	oled->setVisible(true);
}


extern "C" void lcd_refresh(void)
{
	auto optr = (uint8_t*)oled->display.bits();

	for(int y=0;y<128;y++) {
		for(int x=0;x<64;x++) {
			*optr++ = (framebuffer.u32[x*(128/32)+(y/32)] >> (y&31)) & 1 ? 255 : 0;
		}
	}
	oled->repaint();
}


extern "C" void lcd_set_backlight_intensity(uint8_t pct) {
}

