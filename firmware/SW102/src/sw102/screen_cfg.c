#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"
#include "buttons.h"
#include "eeprom.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "screen_cfg_utils.h"

extern const struct screen screen_main;

static void cfg_button_repeat(bool speedup);

struct ss_cfg_list {
	const struct scroller_config *cfg;
	struct scroller_state sst;
};

struct ss_cfg_edit {
	struct ss_cfg_list *parent;
	struct scroller_config cfg;
	struct scroller_state sst;
	const struct configtree_t *item;
	int step, value;
};

// this makes sense only if the scroller doesn't use a callback
// or the callback returns data consistent with scroller_configtree_get()
static void cfg_draw_common(struct scroller_state *sst, const struct scroller_config *cfg, int *edit_value)
{
	const struct configtree_t * it = scroller_configtree_get(sst, cfg);
	int type = it->flags & F_TYPEMASK;
	if(type == F_NUMERIC) {
		char buf[20];
		int y = cfg->winy + 16;
		int val = edit_value ? *edit_value : ptr_get(it->ptr);
		numeric2string(it->numeric, val, buf, true);
		font_text(&font_full, 32, y, buf, AlignCenter | DrawInvert);
	}

	if(type == F_OPTIONS) {
		int y = cfg->winy + 16;
		int val = edit_value ? *edit_value : ptr_get(it->ptr);
		font_text(&font_full, 32, y, it->options->options[val], AlignCenter | DrawInvert);
	}
}

static void cfg_list_idle(void *_it)
{
	struct ss_cfg_list *scr = _it;
	cfg_button_repeat(false);
	scroller_draw_list(&scr->sst, scr->cfg);
	scroller_draw_item(&scr->sst, scr->cfg);

	cfg_draw_common(&scr->sst, scr->cfg, NULL);
}

static void cfg_list_push(const struct scroller_config *it);
static void cfg_edit_push(const struct configtree_t *it, const struct scroller_config *cfg);
static void cfg_list_button(void *_it, int but, int increment)
{
	struct ss_cfg_list *scr = _it;
	(void)increment;
	but = scroller_button(&scr->sst, scr->cfg, but, 1);

	if(but & ONOFF_CLICK) {
		sstack_pop();
		if(!sstack_current) {
			showScreen(&screen_main);
			return;
		}
	}

	if(but & M_CLICK) {
		const struct configtree_t * it = scroller_configtree_get(&scr->sst, scr->cfg);
		int type = it->flags & F_TYPEMASK;
		if(it->flags & F_RO) {
			// nop
		} else if(type == F_SUBMENU) {
			cfg_list_push(it->submenu);

		} else if(type == F_BUTTON) {
			it->action(it);

		} else if(type == F_NUMERIC || type == F_OPTIONS) {
			cfg_edit_push(it, scr->cfg);
		}
	}
}

static const struct stack_class cfg_list_class = {
	sizeof(struct ss_cfg_list),
	.idle = cfg_list_idle,
	.button = cfg_list_button,
};

static void cfg_list_push_class(const struct scroller_config *it, const struct stack_class *klass)
{
	struct ss_cfg_list *ss_it = sstack_alloc(klass);
	scroller_reset(&ss_it->sst);
	ss_it->cfg = it;
	sstack_push();
}

static void cfg_list_push(const struct scroller_config *it)
{
	cfg_list_push_class(it, &cfg_list_class);
}

static void cfg_edit_idle(void *_it)
{
	struct ss_cfg_edit *scr = _it;
	cfg_button_repeat(true);
	scroller_draw_list(&scr->sst, &scr->cfg);
	scroller_draw_item(&scr->parent->sst, scr->parent->cfg);
	cfg_draw_common(&scr->parent->sst, scr->parent->cfg, &scr->value);
}

#include <stddef.h>
#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member)                 \
        (type *)((char *)ptr - offsetof(type, member))
#endif

static bool edit_options_cb(const struct scroller_config *cfg, int index, const struct scroller_item_t **it)
{
	struct ss_cfg_edit *owner = CONTAINER_OF(cfg, struct ss_cfg_edit, cfg);
	static struct scroller_item_t tmpitem;
	tmpitem.text = owner->item->options->options[index];
	if(!tmpitem.text)
		return false;
	if(*it)
		*it = &tmpitem;
	return true;
}

static int get_editable_numeric_value(struct ss_cfg_edit *owner, int index)
{
	const struct cfgnumeric_t *num = owner->item->numeric;
	if(index == 0)
		return num->max;

	int max_aligned = num->max - ((num->max - num->min) % owner->step) + owner->step;
	return max_aligned - owner->step * index;
}

static int get_editable_numeric_index(struct ss_cfg_edit *owner, int value)
{
	const struct cfgnumeric_t *num = owner->item->numeric;
	int max_aligned = num->max - ((num->max - num->min) % owner->step) + owner->step;

	if(value == num->max)
		return 0;

	return (max_aligned - value) / owner->step;
}

static bool edit_numeric_cb(const struct scroller_config *cfg, int index, const struct scroller_item_t **it)
{
	static char buf[20];
	static const struct scroller_item_t tmpitem = { buf };

	struct ss_cfg_edit *owner = CONTAINER_OF(cfg, struct ss_cfg_edit, cfg);
	int v = get_editable_numeric_value(owner, index);
	if(v > owner->item->numeric->max || v < owner->item->numeric->min)
		return false;

	if(it) {
		numeric2string(owner->item->numeric, v, buf, false);
		*it = &tmpitem;
	}

	return true;
}

static void cfg_edit_button(void *_it, int but, int increment)
{
	struct ss_cfg_edit *scr = _it;
	int type = scr->item->flags & F_TYPEMASK;
	int oldval = scr->value;
	but = scroller_button(&scr->sst, &scr->cfg, but, increment);
	if(type == F_NUMERIC) {
		scr->value = get_editable_numeric_value(scr, scr->sst.cidx);

	} else {
		scr->value = scr->sst.cidx;
	}

	if(scr->value != oldval && scr->item->flags & F_CALLBACK) {
		if(scr->item->numeric_cb->preview)
			scr->item->numeric_cb->preview(scr->item, scr->value);
	}

	if(but & ONOFF_CLICK) {
		if(scr->item->flags & F_CALLBACK) {
			if(scr->item->numeric_cb->revert)
				scr->item->numeric_cb->revert(scr->item);
		}
		sstack_pop();
		return;
	}

	if(but & M_CLICK) {
		bool pop = true;
		if(scr->item->flags & F_CALLBACK) {
			pop = scr->item->numeric_cb->update(scr->item, scr->value);
		} else {
			ptr_set(scr->item->ptr, scr->value);
		}

		if(pop)
			sstack_pop();
		return;
	}
}

static const struct stack_class cfg_edit_class = {
	sizeof(struct ss_cfg_edit),
	.idle = cfg_edit_idle,
	.button = cfg_edit_button,
};

static void cfg_edit_push_class(const struct configtree_t *it, const struct scroller_config *cfg, const struct stack_class *klass)
{
	struct ss_cfg_edit *ss_it = sstack_alloc(klass);
	ss_it->parent = (struct ss_cfg_list*)sstack_current->userdata;

	int type = it->flags & F_TYPEMASK;

	if(type == F_NUMERIC) {
		ss_it->item = it;
		ss_it->step = it->numeric->step;
		if(!ss_it->step)
			ss_it->step = 1;
		ss_it->cfg = *cfg;
		ss_it->cfg.cb = edit_numeric_cb;
		scroller_reset(&ss_it->sst);
		ss_it->value = ptr_get(it->ptr);
		ss_it->sst.cidx = get_editable_numeric_index(ss_it, ss_it->value);

	} else if(type == F_OPTIONS) {
		ss_it->item = it;
		ss_it->cfg = *cfg;
		ss_it->cfg.cb = edit_options_cb;
		scroller_reset(&ss_it->sst);
		ss_it->value = ptr_get(it->ptr);
		ss_it->sst.cidx = ss_it->value;
	}

	sstack_push();
}

static void cfg_edit_push(const struct configtree_t *it, const struct scroller_config *cfg)
{
	cfg_edit_push_class(it, cfg, &cfg_edit_class);
}

extern const struct scroller_config cfg_root;
static void cfg_enter()
{
	sstack_reset();
	cfg_list_push(&cfg_root);
}

static void cfg_idle()
{
	clear_all();
	sstack_idle();
	lcd_refresh();
}

static void cfg_handle_button(int but, int increment)
{
	sstack_button(but, increment);
}

// button repeat logic ======================================================================
// we move this here instead of buttons.c, since we need variable-rate button repeat
static int up_hold=-1, down_hold=-1;
static void cfg_button(int but)
{
	if(but & UP_PRESS)
		up_hold = 0;
	if(but & UP_RELEASE)
		up_hold = -1;
	if(but & DOWN_PRESS)
		down_hold = 0;
	if(but & DOWN_RELEASE)
		down_hold = -1;

	cfg_handle_button(but, 1);
}

static int repeat_increments[] = { 
	1, 12, 60,  	// repeat every 240ms until 1.2s
	1, 8, 120, 	// then repeat every 160ms until 2.4s
	1, 4, 180,	// then repeat every 80ms until 3.6s
	1, 1, 250,	// then repeat every 20ms until 5s
	10, 1, INT32_MAX	// then repeat every 20ms, by 10 increments. Odometer setting mode :)
};

static int repeat_button(int counter, bool speedup)
{
	if(!speedup)
		return (counter % 14)? 0 : 1; // regular repeat: 280ms
	else {
		for(int i=0;;i+=3) {
			if(counter <= repeat_increments[i+2]) {
				if(!(counter % repeat_increments[i+1]))
					return repeat_increments[i];
				else
					return 0;
			}
		}
	}
}

static void cfg_button_repeat(bool speedup)
{
	if(up_hold >= 0) {
		int n = repeat_button(++up_hold, speedup);
		if(n) cfg_handle_button(UP_PRESS, n);
	}

	if(down_hold >= 0) {
		int n = repeat_button(++down_hold, speedup);
		if(n) cfg_handle_button(DOWN_PRESS, n);
	}
}

// assist screen ======================================================================
static const int assist_menu_items = 2;
static unsigned short preview_alevels[ASSIST_LEVEL_NUMBER];

static void assist_draw_levels(int current)
{
	ui_vars_t *ui = get_ui_vars();
	const int y0 = 76;
	// clear some overflown parts of the scroller
	fill_rect(0, y0, 64, 20, false);
	
	int topv_limit = INT32_MAX;
	int topv_min = preview_alevels[ui->ui8_number_of_assist_levels-1];

	if(current >= 0 && current < ui->ui8_number_of_assist_levels && preview_alevels[current] > 0) {
		topv_limit = preview_alevels[current] * 10;
	}

	int topv = 2;
	while(topv < topv_min) {
		if(topv * 2 > topv_limit)
			break;
		topv *= 2;
	}

	char buf[10];
	sprintf(buf, "%d", topv);
	int x1 = font_text(&font_full, 0, y0, buf, AlignLeft);
	for(int x=x1&(~1);x<64;x+=2)
		lcd_pset(x, y0, true);

	int bar_width = 65 / ui->ui8_number_of_assist_levels;
	int bar_left = (64 - bar_width* ui->ui8_number_of_assist_levels)/2;
	int bar_fill = bar_width - (bar_width > 16 ? 2 : 1);

	for(int i=0;i < ui->ui8_number_of_assist_levels;i++) {
		int v = preview_alevels[i];
		if(v > topv) v = topv;
		int height = v * (128-y0) / topv;
		if(i == current && (tick & 8))
			fill_rect(bar_left + i * bar_width, 128 - height, bar_fill, 1, true); // flashing
		else
			fill_rect(bar_left + i * bar_width, 128 - height, bar_fill, height, true);
	}

}

extern const struct scroller_config cfg_assist;

static void cfg_assist_idle(void *_it)
{
	struct ss_cfg_list *scr = _it;
	cfg_list_idle(_it);
	
	int current = -1;
	if(scr->cfg == &cfg_assist) // compute this only for the toplevel assist menu
		current = ui_vars.ui8_number_of_assist_levels - 1 - (scr->sst.cidx - assist_menu_items);

	assist_draw_levels(current);
}

static void cfg_assist_sub_idle(void *_it)
{
	cfg_list_idle(_it);
	assist_draw_levels(-1);
}

struct ss_cfg_edit_assist {
	struct ss_cfg_edit edit;
	int current;
};

static void cfg_assist_edit_idle(void *_it)
{
	struct ss_cfg_edit_assist *e = _it;
	cfg_edit_idle(_it);
	assist_draw_levels(e->current);
}

static const struct stack_class cfg_assist_edit_class = {
	sizeof(struct ss_cfg_edit_assist),
	.idle = cfg_assist_edit_idle,
	.button = cfg_edit_button,
};

static void cfg_assist_button(void *_it, int but, int increment)
{
	struct ss_cfg_list *scr = _it;
	if(but & M_CLICK) {
		// ensure that submenus receive the current assist level so that the highlight can follow it
		// the rest of the magic happens in enumerate_assist_levels called by scroller_configtree_get
		int current = ui_vars.ui8_number_of_assist_levels - 1 - (scr->sst.cidx-assist_menu_items);
		
		const struct configtree_t * it = scroller_configtree_get(&scr->sst, scr->cfg);
		cfg_edit_push_class(it, scr->cfg, &cfg_assist_edit_class);
		((struct ss_cfg_edit_assist*)sstack_current->userdata)->current = current;

		return;
	}

	cfg_list_button(_it, but, increment);
}

static const struct stack_class cfg_assist_class = {
	sizeof(struct ss_cfg_list),
	.idle = cfg_assist_idle,
	.button = cfg_assist_button,
};

static const struct stack_class cfg_assist_sub_class = {
	sizeof(struct ss_cfg_list),
	.idle = cfg_assist_sub_idle,
	.button = cfg_list_button,
};

void cfg_push_assist_screen(const struct configtree_t *ign)
{
	memcpy(preview_alevels, ui_vars.ui16_assist_level_factor, sizeof(preview_alevels));
	cfg_list_push_class(&cfg_assist, &cfg_assist_class);
}

static bool alevel_update(const struct configtree_t *it, int value)
{
	memcpy(ui_vars.ui16_assist_level_factor, preview_alevels, sizeof(preview_alevels));
}

static void alevel_preview(const struct configtree_t *it, int value)
{
	int level = (unsigned short*)it->numeric->ptr.ptr - ui_vars.ui16_assist_level_factor;
	for(int i=0;i < ASSIST_LEVEL_NUMBER;i++) {
		preview_alevels[i] = ui_vars.ui16_assist_level_factor[i];
		if(i <= level && preview_alevels[i] >= value)
			preview_alevels[i] = value;
		if(i >= level && preview_alevels[i] <= value)
			preview_alevels[i] = value;
	}
}

static void alevel_revert(const struct configtree_t *it)
{
	memcpy(preview_alevels, ui_vars.ui16_assist_level_factor, sizeof(preview_alevels));
}

bool rescale_update(const struct configtree_t *it, int value)
{
	for(int i=0;i < ASSIST_LEVEL_NUMBER;i++) 
		preview_alevels[i] = ui_vars.ui16_assist_level_factor[i] = ui_vars.ui16_assist_level_factor[i] * value / 100;
}

void rescale_preview(const struct configtree_t *it, int value)
{
	for(int i=0;i < ASSIST_LEVEL_NUMBER;i++) 
		preview_alevels[i] = ui_vars.ui16_assist_level_factor[i] * value / 100;
}

void rescale_revert(const struct configtree_t *it)
{
	memcpy(preview_alevels, ui_vars.ui16_assist_level_factor, sizeof(preview_alevels));
}

bool enumerate_assist_levels(const struct scroller_config *cfg, int index, const struct scroller_item_t **it)
{
	if(index < assist_menu_items) {// regular menu
		return configtree_scroller_item_callback(cfg, index, it);

	} else {
		int current = ui_vars.ui8_number_of_assist_levels - 1 - (index - assist_menu_items);
		if(current < 0)
			return false;

		// assist levels
		static struct cfgnumeric_cb_t alevel_numeric = {
			{ PTRSIZE(ui_vars.ui16_assist_level_factor[0]), 0, "", 1, 2048 },
			.update = alevel_update,
			.preview = alevel_preview,
			.revert = alevel_revert,
		};
		static char buf[10];
		static const struct configtree_t alevel = {
			{ buf } , F_NUMERIC | F_CALLBACK, .numeric_cb = &alevel_numeric
		};

		if(it) {
			alevel_numeric.numeric.ptr.ptr = &ui_vars.ui16_assist_level_factor[current];
			sprintf(buf, "Level %d", current+1);
			*it = &alevel.scrollitem;
		}
		return true;
	}
	return false;
}

extern const struct scroller_config cfg_levels_extend, cfg_levels_truncate;
static int tmp_assist_levels;

bool do_change_assist_levels(const struct configtree_t *ign, int newv)
{
	int oldv = ui_vars.ui8_number_of_assist_levels;

	if(newv == oldv)
		return true;

	if(newv == 1) { // it doesn't make much sense, but just crop in this case
		ui_vars.ui8_number_of_assist_levels = 1;
		return true;

	} else  if(oldv == 1) { // same here, just duplicate the first level
		for(int i=1;i<newv;i++)
			preview_alevels[i] = ui_vars.ui16_assist_level_factor[i] = ui_vars.ui16_assist_level_factor[0];
		ui_vars.ui8_number_of_assist_levels = newv;
		return true;

	} else if(newv > ui_vars.ui8_number_of_assist_levels)
		cfg_list_push_class(&cfg_levels_extend, &cfg_assist_sub_class);
	else
		cfg_list_push_class(&cfg_levels_truncate, &cfg_assist_sub_class);

	tmp_assist_levels = newv;
	return false;
}

void do_resize_assist_levels(const struct configtree_t *ign)
{
	int oldv = ui_vars.ui8_number_of_assist_levels;
	int newv = tmp_assist_levels;

	for(int i = oldv; i < newv;i++) { // if extending, extrapolate linearly
		// linear extrapolation
		ui_vars.ui16_assist_level_factor[i] = ui_vars.ui16_assist_level_factor[i-1] * 2 - ui_vars.ui16_assist_level_factor[i-2];
	}
	ui_vars.ui8_number_of_assist_levels = newv;

	memcpy(preview_alevels, ui_vars.ui16_assist_level_factor, sizeof(preview_alevels));
	sstack_pop(); // extend/interpolate menu
	sstack_pop(); // assist level spinner
}

void do_interpolate_assist_levels(const struct configtree_t *ign)
{
	// use preview_alevels as temporary
	int oldv = ui_vars.ui8_number_of_assist_levels;
	int newv = tmp_assist_levels;
	preview_alevels[0] = ui_vars.ui16_assist_level_factor[0];

	for(int i=1;i < newv-1;i++) {
		int j = i * 256 * (oldv-1) / (newv-1);
		int a = ui_vars.ui16_assist_level_factor[j>>8];
		int b = ui_vars.ui16_assist_level_factor[(j>>8)+1];
		int frac = j&255;
		preview_alevels[i] = (a * (256-frac) + b * frac) >> 8;
	}

	preview_alevels[newv-1] = ui_vars.ui16_assist_level_factor[oldv-1];
	memcpy(ui_vars.ui16_assist_level_factor, preview_alevels, sizeof(preview_alevels));

	ui_vars.ui8_number_of_assist_levels = newv;
	sstack_pop(); // extend/interpolate menu
	sstack_pop(); // assist level spinner
}

// store the configuration on exit ======================================================================

static void cfg_leave()
{
	// copied from common/configscreen
	prepare_torque_sensor_calibration_table();

	// save the variables on EEPROM
	eeprom_write_variables();

	// send the configurations to TSDZ2
	if (g_motor_init_state == MOTOR_INIT_READY)
		g_motor_init_state = MOTOR_INIT_SET_CONFIGURATIONS;
}

const struct screen screen_cfg = {
	.enter = cfg_enter,
	.idle = cfg_idle,
	.button = cfg_button,
	.leave = cfg_leave
};
