#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"
#include "logo_biktrix.xbm"

DEFINE_IMAGE(logo_biktrix);

extern const struct screen screen_main;

/* Displays boot screen until motor communication is recieved */
static void boot_idle()
{
	// Check motor state
	switch (g_motor_init_state) {
		case MOTOR_INIT_SIMULATING:
			if(tick < 100)
				break;

		case MOTOR_INIT_WAIT_GOT_CONFIGURATIONS_OK:
		case MOTOR_INIT_READY:
			showScreen(&screen_main);
			return;

		// any error state will block here and avoid leave the boot screen
		default:
			break;
	}

	// Update the flipbook 
	if(tick%10 != 0)return;
	static int q=1;
	q=(q+1)&1;
	fill_rect(0,0,64,128, false);
	img_draw_clip(&img_logo_biktrix, 1, 22, q*58, 0, 58, 75, 0);
	lcd_refresh();
}

void ui_show_motor_status(){
}

const struct screen screen_boot = {
	.idle = boot_idle
};
