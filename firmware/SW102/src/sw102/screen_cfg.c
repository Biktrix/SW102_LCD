#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"
#include "buttons.h"
#include <stdlib.h>
#include <stdio.h>

#define UP_PRESS_OR_REPEAT (UP_PRESS|UP_REPEAT)
#define DOWN_PRESS_OR_REPEAT (DOWN_PRESS|DOWN_REPEAT)

const
#include "font_full.xbm"
DEFINE_FONT(full, 
	" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
	5,7,11,20,26,37,46,48,52,56,62,70,72,76,78,83,90,96,103,110,117,124,131,138,145,152,154,156,165,174,183,189,201,210,217,224,232,239,245,253,261,263,267,274,280,289,297,305,312,320,328,335,343,351,360,372,380,388,396,399,404,407,414,421,424,431,438,444,451,458,463,470,477,479,482,488,490,500,507,514,521,528,533,539,544,551,558,568,575,582,588,594,596,602
);
extern const struct screen screen_main;

enum {
	F_SUBMENU=1,
	F_BUTTON,
	F_NUMERIC,
	F_OPTIONS,
	F_TYPEMASK=0xff,
	F_RO=0x100
};

struct configtree_t;

static void do_reset_trip_a(const struct configtree_t *ign);
static void do_reset_trip_b(const struct configtree_t *ign);

typedef void (cfgaction_t)(const struct configtree_t *ign);

struct cfgptr_t {
	void *ptr; int size;
};

struct cfgnumeric_t {
	struct cfgptr_t ptr;
	int decimals;
	const char *unit;
	int min, max, step;
};

static int ptr_get(const struct cfgptr_t *it)
{
	if(it->size == 1) {
		return *((unsigned char*)it->ptr);
	} else if(it->size == 2) {
		return *((unsigned short*)it->ptr);
	} else if(it->size == 4) {
		return *((unsigned int*)it->ptr);
	}
	return 0;
}

static void ptr_set(const struct cfgptr_t *it, int v)
{
	if(it->size == 1) {
		*((unsigned char*)it->ptr) = v;
	} else if(it->size == 2) {
		*((unsigned short*)it->ptr) = v;
	} else if(it->size == 4) {
		*((unsigned int*)it->ptr) = v;
	}
}

static void numeric2string(const struct cfgnumeric_t *num, int v, char *out, bool include_unit)
{
	int draw_decimals = num->decimals;
	if(num->step >= 100 && (num->step % 100) == 0)
		draw_decimals-=2;
	else if(num->step >= 10 && (num->step % 10) == 0)
		draw_decimals--;

	if(draw_decimals < 0)
		draw_decimals = 0;

	// drop unneeded decimals
	for(int i=0;i < num->decimals - draw_decimals;i++)
		v /= 10;

	if(draw_decimals > 0) {
		int div = 1;
		for(int i=0;i<draw_decimals;i++) div*=10;
		if(include_unit)
			sprintf(out, "%d.%d %s", v/div, v%div, num->unit);
		else
			sprintf(out, "%d.%d", v/div, v%div);

	} else {
		if(include_unit)
			sprintf(out, "%d %s", v, num->unit);
		else
			sprintf(out, "%d", v);
	}
}

struct cfgoptions_t {
	struct cfgptr_t ptr;
	const char **options;
};

#define PTRSIZE(a) 	{ &(a), sizeof(a) }

struct scroller_config;

struct configtree_t {
	const char *text;
	int flags;
	union {
		const struct scroller_config *submenu;
		cfgaction_t *action;
		const struct cfgptr_t *ptr;
		const struct cfgnumeric_t *numeric;
		const struct cfgoptions_t *options;
	};
};

static const char *disable_enable[] = { "disable", "enable", 0 };
static const char *off_on[] = { "off", "on", 0 };

typedef const char *(scroller_item_callback)(const struct scroller_config *cfg, int index);

struct scroller_config {
	int pitch, winy, winh;
	const struct configtree_t *list;
	scroller_item_callback *cb;
};

struct scroller_state {
	int xscroll, yscroll, cidx;
};

const struct configtree_t cfgroot[] = {
	{ "Trip memory", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 18, (const struct configtree_t[]) {
		{ "Reset trip A", F_BUTTON, .action = do_reset_trip_a },
		{ "Reset trip B", F_BUTTON, .action = do_reset_trip_b },
		{},
	}}},
	{ "Wheel", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Max speed", F_NUMERIC, .numeric = &(const struct cfgnumeric_t){ PTRSIZE(ui_vars.wheel_max_speed_x10), 1, "km/h", 10, 990, 10 }},
		{ "Circumference", F_NUMERIC, .numeric = &(const struct cfgnumeric_t){ PTRSIZE(ui_vars.ui16_wheel_perimeter), 0, "mm", 750, 3000, 10 }},
		{},
	}}},
	{ "Battery", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Max current", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_battery_max_current), 0, "A", 1, 20 }},
 		{ "Cut-off voltage", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_battery_low_voltage_cut_off_x10), 1, "V", 160, 630 }},
		{ "Resistance", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_battery_pack_resistance_x1000), 0, "mohm", 0, 1000 }},
		{ "Voltage", F_NUMERIC|F_RO, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_battery_voltage_soc_x10), 1, "V" }},
		{ "Est. resistance", F_NUMERIC|F_RO, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_battery_pack_resistance_estimated_x1000), 0, "mohm" }},
		{ "Power loss", F_NUMERIC|F_RO, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_battery_power_loss), 0, "W" }},
		{},
	}}},
	{ "Charge", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Reset voltage", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_battery_voltage_reset_wh_counter_x10), 1, "V", 160, 630 }},
		{ "Total capacity", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui32_wh_x10_100_percent), 1, "Wh", 0, 9990, 100 }}, // FIXME callback
		{ "Used Wh", F_NUMERIC|F_RO,  .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui32_wh_x10), 1, "Wh", .step = 100 /* don't show decimals */ }},
		{},
	}}},
	{ "Motor", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Motor voltage", F_OPTIONS, .options = &(const struct cfgoptions_t){ PTRSIZE(ui_vars.ui8_motor_type), (const char*[]){ "48V", "36V", 0}}},
		{ "Max current", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_motor_max_current), 0, "A", 1, 20 }},
		{ "Current ramp", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_ramp_up_amps_per_second_x10), 1, "A", 4, 100 }},
		{ "Control mode", F_OPTIONS, .options = &(const struct cfgoptions_t){ PTRSIZE(ui_vars.ui8_motor_current_control_mode), (const char*[]){ "power", "torque", 0}}},
		{ "Min current", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_motor_current_min_adc), 0, "steps", 0, 13 }},
		{ "Field weakening", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_field_weakening), disable_enable } },
		{},
	}}},
	{ "Torque sensor", 0, },
	{ "Assist", 0, },
	{ "Walk assist", 0, },
	{ "Temperature", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Temp. sensor", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_temperature_limit_feature_enabled), disable_enable } },
		{ "Min limit", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_motor_temperature_min_value_to_limit), 0, "C", 30, 100 }},
		{ "Max limit", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_motor_temperature_max_value_to_limit), 0, "C", 30, 100 }},
		{},
	}}},
	{ "Street mode", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Feature", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_street_mode_function_enabled), disable_enable } },
		{ "Current status", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_street_mode_enabled), off_on } },
		{ "At startup", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_street_mode_enabled), (const char*[]){ "no change", "activate", 0 } }},
		{ "Hotkey", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_street_mode_hotkey_enabled), disable_enable } },
		{ "Speed limit", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_street_mode_speed_limit), 0, "km/h", 1, 99 }},
		{ "Power limit", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui16_street_mode_power_limit), 0, "W", 25, 1000, 25 }},
		{ "Throttle", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_street_mode_throttle_enabled), disable_enable } },
		{},
	}}},
	{ "Various", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{ "Fast stop", F_OPTIONS, .options = &(const struct cfgoptions_t) { PTRSIZE(ui_vars.ui8_pedal_cadence_fast_stop), disable_enable } },
		{ "Lights current", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui8_adc_lights_current_offset), 0, "steps", 1, 4 }},
//		{ "Odometer", F_NUMERIC, .numeric = &(const struct cfgnumeric_t) { PTRSIZE(ui_vars.ui32_odometer_x10), 1, "km", 1, 4 }}, FIXME
		{},
	}}},
	{ "Display", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{},
		{},
	}}},
	{ "Technical", F_SUBMENU, .submenu = &(const struct scroller_config){ 20, 58, 36, (const struct configtree_t[]) {
		{},
		{},
	}}},
	{ NULL, 0 },
};

static void scroller_reset(struct scroller_state *st)
{
	st->cidx = st->xscroll = st->yscroll = 0;
}

#define INVERTED_DRAW 1

static const char * default_scroller_item_callback(const struct scroller_config *cfg, int index)
{
	return cfg->list[index].text;
}

static void scroller_draw_list(struct scroller_state *st, const struct scroller_config *cfg)
{
	scroller_item_callback *cb = cfg->cb ? cfg->cb : default_scroller_item_callback;

	if(st->yscroll > 0)
		st->yscroll-=2;
	if(st->yscroll < 0)
		st->yscroll+=2;

	for(int i=-1;i + st->cidx >= 0 && i > -5;i--){
		int y = cfg->winy + cfg->pitch * i + st->yscroll;
		font_text(&font_full, 32, y, cb(cfg, st->cidx + i), AlignCenter);
	}

	for(int i=1;i < 5;i++){
		const char *text = cb(cfg, st->cidx+i);
		if(!text)
			break;
		int y = cfg->winy + cfg->winh + cfg->pitch * (i-1) + st->yscroll;
		font_text(&font_full, 32, y, text, AlignCenter);
	}
}

static void scroller_draw_item(struct scroller_state *st, const struct scroller_config *cfg)
{
	scroller_item_callback *cb = cfg->cb ? cfg->cb : default_scroller_item_callback;

	// frame for the current selection
#if !INVERTED_DRAW
	fill_rect(1,cfg->winy-2,62,1, true);
	fill_rect(0,cfg->winy-2,1,cfg->winh, true);
	fill_rect(63,cfg->winy-2,1,cfg->winh, true);
	fill_rect(1,cfg->winy-1,62,cfg->winh-2, false);
	fill_rect(1,cfg->winy-2+cfg->winh-1,62,1, true);
#else	
	fill_rect(0,cfg->winy-2,64,cfg->winh, true);
#endif

	// current selection, scrolling
	int fl = font_length(&font_full, cfg->list[st->cidx].text);
	if(fl < 60) { 
		st->xscroll = (fl-64)/2;
	} else {
		if(-st->xscroll+fl < 50)
			st->xscroll = -10;
		if(!(tick&3)) 
			st->xscroll++;
	}
	
	font_text(&font_full, -st->xscroll, cfg->winy, cb(cfg, st->cidx), AlignLeft | (INVERTED_DRAW ? DrawInvert : 0));
}

static int scroller_button(struct scroller_state *st, const struct scroller_config *cfg, int but, int *speedup)
{
	static int up_hold = 0, down_hold = 0;
	scroller_item_callback *cb = cfg->cb ? cfg->cb : default_scroller_item_callback;

	if(but & UP_PRESS_OR_REPEAT) {
		up_hold++;
		if(st->cidx > 0) {
			--st->cidx;
			st->yscroll = -cfg->pitch;
			st->xscroll = 0;
			but &= ~UP_PRESS_OR_REPEAT;
		}
	}

	if(but & UP_RELEASE)
		up_hold=0;

	if(but & DOWN_PRESS_OR_REPEAT) {
		down_hold++;
		if(cb(cfg, st->cidx+1)) {
			++st->cidx;
			st->yscroll = cfg->pitch;
			st->xscroll = 0;
			but &= ~DOWN_PRESS_OR_REPEAT;
		}
	}

	if(but & DOWN_RELEASE)
		down_hold = 0;

	if(speedup) {
		if(up_hold > 10 || down_hold > 10)
			*speedup = 20;
		else if(up_hold > 5 || down_hold > 5)
			*speedup = 5;
		else
			*speedup = 1;
	}

	return but;
}
		
static const struct configtree_t *scroller_get(struct scroller_state *st, const struct scroller_config *cfg)
{
	return &cfg->list[st->cidx];
}

#define STACK_MAX 3
const struct scroller_config root = { 20, 58, 18, cfgroot };
static struct scroller_state sst[STACK_MAX];
static const struct scroller_config *current[STACK_MAX];
static int stackdepth = 0;

static void stack_pop()
{
	int i;
	for(int i=0;i<stackdepth;i++){
		sst[i]=sst[i+1];
		current[i]=current[i+1];
	}

	--stackdepth;
}

static void stack_push(const struct scroller_config *it)
{
	++stackdepth;
	for(int i=stackdepth;i>0;i--){
		sst[i] = sst[i-1];
		current[i] = current[i-1];
	}

	*current = it;
	scroller_reset(sst);
}

static void cfg_enter()
{
	*current = &root;
	stackdepth = 0;
	scroller_reset(sst);
}

static const struct configtree_t *edit_item;
static int edit_step, edit_base;
static int edit_value;
static struct scroller_config edit_cfg;
static struct scroller_state edit_sst;

static const char * edit_options_cb(const struct scroller_config *cfg, int index)
{
	return edit_item->options->options[index];
}

static int get_editable_numeric_value(int index)
{
	if(index == 0)
		return edit_item->numeric->min;

	if(edit_base + edit_step * index > edit_item->numeric->max && edit_base + edit_step * (index-1) < edit_item->numeric->max)
		return edit_item->numeric->max;

	return edit_base + edit_step * index;
}

static int get_editable_numeric_index(int value)
{
	if(value == edit_item->numeric->min)
		return 0;

	return (value - edit_base) / edit_step;
}

static const char *edit_numeric_cb(const struct scroller_config *cfg, int index)
{
	static char buf[20];
	int v = get_editable_numeric_value(index);
	if(v > edit_item->numeric->max)
		return NULL;

	numeric2string(edit_item->numeric, v, buf, false);
	return buf;
}

static void cfg_idle()
{
	clear_all();
	if(edit_item) {
		scroller_draw_list(&edit_sst, &edit_cfg);
	} else {
		scroller_draw_list(sst, *current);
	}

	scroller_draw_item(sst, *current);

	const struct configtree_t * it = scroller_get(sst, *current);
	int type = it->flags & F_TYPEMASK;
	if(type == F_NUMERIC) {
		char buf[20];
		int y = (*current)->winy + 16;
		int val = edit_item ? edit_value : ptr_get(it->ptr);
		numeric2string(it->numeric, val, buf, true);
		font_text(&font_full, 32, y, buf, AlignCenter | (INVERTED_DRAW ? DrawInvert : 0));
	}

	if(type == F_OPTIONS) {
		int y = (*current)->winy + 16;
		int val = edit_item ? edit_value : ptr_get(it->ptr);
		font_text(&font_full, 32, y, it->options->options[val], AlignCenter | (INVERTED_DRAW ? DrawInvert : 0));
	}
	lcd_refresh();
}

static void cfg_button(int but)
{
	if(edit_item) {
		int type = edit_item->flags & F_TYPEMASK;
		int speedup;
		but = scroller_button(&edit_sst, &edit_cfg, but, &speedup);

		if(type == F_NUMERIC) {
			edit_value = get_editable_numeric_value(edit_sst.cidx);

			if(speedup) {
				edit_step = edit_item->numeric->step;
				if(!edit_step)
					edit_step = 1;
				edit_step *= speedup;

				edit_base = edit_item->numeric->min + (edit_value) % edit_step;

				edit_sst.cidx = get_editable_numeric_index(edit_value);
			}

		} else {
			edit_value = edit_sst.cidx;
		}

		if(but & ONOFF_CLICK) {
			edit_item = 0;
		}

		if(but & M_CLICK) {
			ptr_set(edit_item->ptr, edit_value);
			edit_item = 0;
		}


	} else {
		but = scroller_button(sst, *current, but, NULL);

		if(but & ONOFF_CLICK) {
			// back
			if(stackdepth == 0) {
				showScreen(&screen_main);
				return;

			} else {
				stack_pop();
			}
		}

		if(but & M_CLICK) {
			const struct configtree_t * it = scroller_get(sst, *current);
			int type = it->flags & F_TYPEMASK;
			if(it->flags & F_RO) {
				// nop
			} else if(type == F_SUBMENU) {
				stack_push(it->submenu);

			} else if(type == F_BUTTON) {
				it->action(it);

			} else if(type == F_NUMERIC) {
				edit_item = it;
				edit_step = edit_item->numeric->step;
				if(!edit_step)
					edit_step = 1;
				edit_base = edit_item->numeric->min;

				edit_cfg = **current;
				edit_cfg.cb = edit_numeric_cb;
				scroller_reset(&edit_sst);
				edit_value = ptr_get(it->ptr);
				edit_sst.cidx = get_editable_numeric_index(edit_value);

			} else if(type == F_OPTIONS) {
				edit_item = it;
				edit_cfg = **current;
				edit_cfg.cb = edit_options_cb;
				scroller_reset(&edit_sst);
				edit_value = ptr_get(it->ptr);
				edit_sst.cidx = edit_value;
			}
		}
	}
}

static void do_reset_trip_a(const struct configtree_t *ign)
{
	// FIXME is accessing rt_vars safe here?
	rt_vars.ui32_trip_a_distance_x1000 = 0;
	rt_vars.ui32_trip_a_time = 0;
	rt_vars.ui16_trip_a_avg_speed_x10 = 0;
	rt_vars.ui16_trip_a_max_speed_x10 = 0;
	stack_pop();
}

static void do_reset_trip_b(const struct configtree_t *ign)
{
	rt_vars.ui32_trip_b_distance_x1000 = 0;
	rt_vars.ui32_trip_b_time = 0;
	rt_vars.ui16_trip_b_avg_speed_x10 = 0;
	rt_vars.ui16_trip_b_max_speed_x10 = 0;
	stack_pop();
}


const struct screen screen_cfg = {
	.enter = cfg_enter,
	.idle = cfg_idle,
	.button = cfg_button
};
