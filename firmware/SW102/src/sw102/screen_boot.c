#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"

const
#include "logo.xbm"
const
#include "logo_anim.xbm"

DEFINE_IMAGE(logo);
DEFINE_IMAGE(logo_anim);

extern const struct screen screen_main;

static void boot_idle()
{
	switch (g_motor_init_state) {
		case MOTOR_INIT_SIMULATING:
			if(tick < 50)
				break;

		case MOTOR_INIT_WAIT_GOT_CONFIGURATIONS_OK:
		case MOTOR_INIT_READY:
			showScreen(&screen_main);
			return;

		// any error state will block here and avoid leave the boot screen
		default:
			break;
	}

	if(tick&3)
		return;

	static int q=1;
	fill_rect(0,0,64,69, false);
	img_draw(&img_logo, 16, 17);
	img_draw_clip(&img_logo_anim, 8, 29, 0, q*18, 18, 18, 0);
	q=(q+3)&3;
	img_draw_clip(&img_logo_anim, 38, 29, 0, q*18, 18, 18, 0);
	lcd_refresh();
}

void ui_show_motor_status()
{
}

const struct screen screen_boot = {
	.idle = boot_idle
};
