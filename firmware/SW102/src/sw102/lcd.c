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

#include "lcd.h"
#include "common.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"


/* Function prototype */
static void set_cmd(void);
static void set_data(void);
// static void send_byte(uint8_t byte);
static void spi_init(void);


/* Variable definition */

/* Frame buffer in RAM with same structure as LCD memory, aligned vertically.
 * 128 consecutive bits represent a single column of the display */
union framebuffer_t framebuffer;

/* Init sequence sampled by casainho from original SW102 display
 * Adjusted for vertical image orientation. */
static const uint8_t init_array[] = {
    0xAE, // 11. display on
    0xA8, 0x3F, // set multiplex ratio 3f
    0xD5, 0x50, // set display divite/oscillator ratios
    0xC0, // set common scan dir
    0xD3, 0x60, // ???
    0xDC, 0x00,  // set display start line
    0x21, // set memory address mode
    0x81, 0xFF, // Set contrast level (POR value is 0x80, but closed source software uses 0xBF
    0xA0, // set segment remap
    0xA4, // set normal display mode
    0xA6, // not inverted
    0xAD, 0x8A, // set DC-DC converter
    0xD9, 0x1F, // set discharge/precharge period
    0xDB, 0x30, // set common output voltage
    0xAF // turn display on
    };


/* SPI instance */
const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(LCD_SPI_INSTANCE);

static void set_cmd(void)
{
  nrf_gpio_pin_clear(LCD_COMMAND_DATA__PIN);
  //nrf_delay_us(1);  // Max. setup time (~150 ns)
}

static void set_data(void)
{
  nrf_gpio_pin_set(LCD_COMMAND_DATA__PIN);
  //nrf_delay_us(1);  // Max. setup time (~150 ns)
}

/**
 * @brief Sends single command byte
 */
static void send_cmd(const uint8_t *cmds, size_t numcmds)
{
  set_cmd();
  APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, cmds, numcmds, NULL, 0));
}

/**
 * @brief LCD initialization including hardware layer.
 */
void lcd_init(void)
{
  spi_init();

  // LCD hold in reset since gpio_init
  // SH1107 reset time (p. 55) doubled
  nrf_delay_us(20);
  nrf_gpio_pin_set(LCD_RES__PIN);
  nrf_delay_us(4);

  // Power On Sequence SH1107 data sheet p. 44

  // Set up initialization sequence
  send_cmd(init_array, sizeof(init_array));

  // Clear internal RAM
  lcd_refresh(); // Is already initialized to zero in bss segment.

  // Wait 100 ms
  nrf_delay_ms(100);  // Doesn't have to be exact this delay.

  // kevinh - I've moved this to be an explicit call, because calling lcd_refresh on each operation is super expensive
  // UG_SetRefresh(lcd_refresh); // LCD refresh function
}



static int lcdBacklight = -1; // -1 means unset
static int oldBacklight = -1;

/**
 * @brief Start transfer of frameBuffer to LCD
 */
void lcd_refresh(void)
{
  if(lcdBacklight != oldBacklight) {
    oldBacklight = lcdBacklight;

    uint8_t level = 255 * (100 - lcdBacklight);
    uint8_t cmd[] = { 0x81, level };

    send_cmd(cmd, sizeof(cmd));
  }

  static uint8_t pagecmd[] = { 0xb0, 0x00, 0x10 };

  for (uint8_t i = 0; i < 64; i++)
  {
    // New column address
    pagecmd[1] = (i&15);
    pagecmd[2] = 0x10|(i>>4);
    send_cmd(pagecmd, sizeof(pagecmd));

    // send page data
    set_data();
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, framebuffer.u8+(128/8)*i, 128/8, NULL, 0));
  }
}

/**
 * @brief SPI driver initialization.
 */
static void spi_init(void)
{
  nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
  spi_config.ss_pin = LCD_CHIP_SELECT__PIN;
  spi_config.mosi_pin = LCD_DATA__PIN;
  spi_config.sck_pin = LCD_CLOCK__PIN;
  /* DEFAULT_CONFIG may change */
  spi_config.frequency = NRF_SPI_FREQ_4M; // SH1107 data sheet p. 52 (Vdd 3.3 V)
  spi_config.mode = NRF_SPI_MODE_0;
  spi_config.bit_order = NRF_SPI_BIT_ORDER_MSB_FIRST;

  APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL));
}




//SW102 version, we are an oled so if the user asks for lots of backlight we really want to dim instead
// Note: This routine might be called from an ISR, so do not do slow SPI operations (especially because
// you might muck up other threads).  Instead just change the desired intensity and wait until the next
// screen redraw.
void lcd_set_backlight_intensity(uint8_t pct) {
  lcdBacklight = pct;
}

