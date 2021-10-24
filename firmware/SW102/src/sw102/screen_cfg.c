#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"

const
#include "font_full.xbm"
DEFINE_FONT(full, 
	" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
	5,7,11,20,26,37,46,48,52,56,62,70,72,76,78,83,90,96,103,110,117,124,131,138,145,152,154,156,165,174,183,189,201,210,217,224,232,239,245,253,261,263,267,274,280,289,297,305,312,320,328,335,343,351,360,372,380,388,396,399,404,407,414,421,424,431,438,444,451,458,463,470,477,479,482,488,490,500,507,514,521,528,533,539,544,551,558,568,575,582,588,594,596,602
);
extern const struct screen screen_main;

static void cfg_enter()
{
	clear_all();
	font_text(&font_full, 0, 0, "Hello world", AlignLeft);
	lcd_refresh();
}

static void cfg_idle()
{
}

const struct screen screen_cfg = {
	.enter = cfg_enter,
	.idle = cfg_idle
};
