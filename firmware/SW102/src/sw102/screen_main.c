#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "buttons.h"
#include <stdio.h>

#include "state.h"

const
#include "icon_brake.xbm"
DEFINE_IMAGE(icon_brake);
const
#include "icon_battery_frame.xbm"
DEFINE_IMAGE(icon_battery_frame);
const
#include "icon_battery.xbm"
DEFINE_IMAGE(icon_battery);
const
#include "icon_radio.xbm"
DEFINE_IMAGE(icon_radio);
const
#include "icon_walk.xbm"
DEFINE_IMAGE(icon_walk);
const
#include "icon_light.xbm"
DEFINE_IMAGE(icon_light);

// fonts based on DejaVu Sans, some hand-modified
const
#include "font_speed.xbm"
DEFINE_FONT(speed, ".0123456789", 2, 19, 35, 51, 66, 83, 99, 116, 133, 151);

const
#include "font_2nd.xbm"
DEFINE_FONT(2nd, " ./0123456789Whkm", 2, 3, 7, 16, 23, 31, 40, 49, 58, 67, 76, 85, 94, 103, 111, 118);

const
#include "font_battery.xbm"
DEFINE_FONT(battery, "%.0123456789V", 5, 7, 12, 17, 22, 27, 32, 37, 42, 47, 52, 57);

#define GRAPH_DEPTH 64
struct GraphData {
	uint8_t data[GRAPH_DEPTH];
};
static int graph_head;
static struct GraphData graph_speed;
static struct GraphData graph_pedal_power;
static struct GraphData graph_motor_power;
static struct GraphData graph_cadence;

static void graph_append(struct GraphData *gd, int v)
{
	if(v<0) 
		v=0;
	if(v>255)
		v=255;
	gd->data[graph_head] = v;
}

static void graph_paint(struct GraphData *gd, int x_left, int y_bot, int w, int scale_h, int clip_h)
{
	int x;
	for(x=0; x < w;x++) {
		int v = gd->data[(graph_head-w+x+GRAPH_DEPTH) % GRAPH_DEPTH] * scale_h / 256;
		if(v < clip_h) {
			int vv;
			lcd_pset(x + x_left, y_bot - v - 1, true);
			for(vv=((x+graph_head)&1);vv<v;vv+=2)
				lcd_pset(x + x_left, y_bot - vv - 1, true);
		}
	}
}

enum display_mode_t {
	ModeOdometer,
	ModeTripDistance,
	ModeTripTime,
	ModeTripAVS,
	ModePedalPower,
	ModeMotorPower,
	ModeLast,
} display_mode;

static struct GraphData * const mode_graph[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&graph_pedal_power,
	&graph_motor_power,
};

static void draw_main_speed(ui_vars_t *ui, int y)
{
	char buf[20];
	int speed_x10 = ui->ui16_wheel_speed_x10;
	sprintf(buf, "%d.%01d", speed_x10/10, speed_x10 % 10);
	font_text(&font_speed, 32, y, buf, AlignCenter);
}

static void draw_2nd_field(ui_vars_t *ui, int y)
{
	char buf[20];
	unsigned m;

	switch(display_mode) {
	case ModeOdometer:
		sprintf(buf, "%d km", ui_vars.ui32_odometer_x10/10);
		break;

	case ModeTripDistance:
		m = ui->ui32_trip_a_distance_x1000;
		if(m < 1000) // <0m-999m
			sprintf(buf, "%d m", m);
		else if(m < 10000) // <1.000km-9.999km
			sprintf(buf, "%d.%03d km", m/1000, m % 1000);
		else if(m < 100000) // 10.00km-99.99km
			sprintf(buf, "%d.%02d km", m/1000, (m/10) % 100);
		else if(m < 100000) // 100.0km-999.9km
			sprintf(buf, "%d.%01d km", m/1000, (m/100) % 10);
		else // 1000km+
			sprintf(buf, "%d km", m/1000);
		break;

	case ModeTripTime:
		m = ui->ui32_trip_a_time/60;
		if(m < 60 * 99)
			sprintf(buf, "%dh %dm", m/60, m % 60);
		else
			sprintf(buf, "%d h", m/3600);
		break;

	case ModeTripAVS:
		m = ui->ui16_trip_a_avg_speed_x10;
		sprintf(buf, "%d.%01d km/h", m/10, m%10);
		break;

	case ModePedalPower:
		m = ui->ui16_pedal_power;
		sprintf(buf, "%d W", m);
		break;

	case ModeMotorPower:
		m = ui->ui16_battery_power;
		sprintf(buf, "m %dW", m);
		break;
	default:
		buf[0]=0;
	}
		
	font_text(&font_2nd, 32, y, buf, AlignCenter);
}

static void draw_battery_indicator(ui_vars_t *ui)
{
	char buf[10];
	int batpx = ui8_g_battery_soc / 5;
	img_draw(&img_icon_battery_frame, 0, 1);

	img_draw_clip(&img_icon_battery, 0, 1, 0, 0, batpx+3, img_icon_battery.h, 0);

	switch (ui_vars.ui8_battery_soc_enable) {
	case 0:
		break;
	case 1:
		sprintf(buf, "%d%%", ui8_g_battery_soc);
		break;
	case 2:
		sprintf(buf, "%d.%dV", ui->ui16_battery_voltage_soc_x10/10,  ui->ui16_battery_voltage_soc_x10 % 10);
		break;
	}
	font_text(&font_battery, 62, 3, buf, AlignRight);
}

static void draw_power_indicator(ui_vars_t *ui)
{
	int tmp = ui->ui16_battery_power;
	int max_current = ui->ui8_motor_max_current;
	if(ui->ui8_battery_max_current > max_current)
		max_current = ui->ui8_battery_max_current;

	// estimate max. power from current limit & max voltage
	int max_power = ui->ui16_battery_voltage_reset_wh_counter_x10 * max_current / 10;
	if(tmp > max_power)
		tmp = max_power;
	tmp = tmp * 99 / max_power;
	fill_rect(62, 114 - tmp, 2, tmp, true);
}

static void draw_misc_indicators(ui_vars_t *ui)
{
	if(ui->ui8_walk_assist)
		img_draw(&img_icon_walk, 0, 119);
	if(ui->ui8_braking)
		img_draw(&img_icon_brake, 23, 119);

	img_draw(&img_icon_radio, 41, 119);

	if(ui->ui8_lights)
		img_draw(&img_icon_light, 53, 118);
}

static void draw_assist_indicator(ui_vars_t *ui)
{
	int bar_height = 100 / ui->ui8_number_of_assist_levels;
	int bar_bottom = 14 + ui->ui8_number_of_assist_levels * bar_height;
	int bar_fill = bar_height - (bar_height > 16 ? 2 : 1);
	int i;

	for(i=0;i < ui->ui8_assist_level;i++) 
		fill_rect(0, bar_bottom - bar_height * i - bar_fill, 2, bar_fill, true);
}

static bool draw_fault_states(ui_vars_t *ui)
{
	extern const struct font font_full;
	const char *e1="FAULT:", *e2 = "";

	if(ui->ui8_error_states & 2)
		e1="initializing", e2 = "motor";
	else if(ui->ui8_error_states & 4)
		e1 = "motor",e2="blocked";
	else if(ui->ui8_error_states & 8)
		e2 = "torque";
	else if(ui->ui8_error_states & 16)
		e2 = "brake";
	else if(ui->ui8_error_states & 32)
		e2 = "throttle";
	else if(ui->ui8_error_states & 64)
		e2 = "speed sns";
	else if(ui->ui8_error_states & 128)
		e2 = "comm";
	else
		return false;

	font_text(&font_full, 32, 80, e1, AlignCenter);
	font_text(&font_full, 32, 96, e2, AlignCenter);
	return true;
}

static void main_idle()
{
	char *ptr;
	ui_vars_t *ui = get_ui_vars();
	clear_all();

	if(!(tick&15)){
		if (ui->ui16_wheel_speed_x10 > 0) {
			graph_append(&graph_speed, ui->ui16_wheel_speed_x10/3); 	// 0- 76
			graph_append(&graph_pedal_power, ui->ui16_pedal_power/2);	// 0- 512W
			graph_append(&graph_motor_power, ui->ui16_battery_power/4);	// 0-1024W
			graph_append(&graph_cadence, ui->ui8_pedal_cadence_filtered);	// 0-255
			graph_head=(graph_head+1) % GRAPH_DEPTH;
		}
	}

	if(ui->ui8_street_mode_enabled) {
		draw_hline(0, 64, 13);
		draw_hline(0, 64, 115);
	}

	draw_battery_indicator(ui);

	draw_misc_indicators(ui);
	draw_power_indicator(ui);

	draw_assist_indicator(ui);

	if(mode_graph[display_mode]) {
		draw_main_speed(ui, 28);
		draw_2nd_field(ui, 65);
	} else {
		draw_2nd_field(ui, 22);
		draw_main_speed(ui, 44);
	}

	if(!draw_fault_states(ui)) {
		struct GraphData *gd = mode_graph[display_mode];
		if(!gd) 
			gd = &graph_speed;

		graph_paint(gd, 3, 114, 58, 50, 114-72);
	}
	lcd_refresh();
}

extern const struct screen screen_cfg;
static void main_button(int but)
{
	ui_vars_t *ui = get_ui_vars();
	if(but & UP_CLICK) {
		if(ui->ui8_assist_level < ui->ui8_number_of_assist_levels) 
			ui->ui8_assist_level++;
	}

	if((but & DOWN_CLICK) && !ui->ui8_walk_assist) {
		if(ui->ui8_assist_level > 0)
			ui->ui8_assist_level--;
	}

	if(but & ONOFF_CLICK) {
		ui->ui8_lights = !ui->ui8_lights;
		set_lcd_backlight();
	}

	if((but & UP_LONG_CLICK) && ui->ui8_street_mode_function_enabled) 
		ui->ui8_street_mode_enabled =! ui->ui8_street_mode_enabled;

	if ((but & DOWN_LONG_CLICK) && ui->ui8_walk_assist_feature_enabled) {
		ui_vars.ui8_walk_assist = 1;
	}

	if(but & DOWN_RELEASE)
		ui_vars.ui8_walk_assist = 0;

	if(but & M_CLICK) {
		display_mode = (enum display_mode_t)(((int)display_mode + 1) % ModeLast);
	}
	
	if(but & M_LONG_CLICK) {
		// enter cfg screen
		showScreen(&screen_cfg);
	}
}

const struct screen screen_main = {
	.idle = main_idle,
	.button = main_button
};
