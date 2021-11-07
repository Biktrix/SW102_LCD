#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"
#include "buttons.h"
#include "eeprom.h"
#include <stdlib.h>
#include <stdio.h>

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
	int step, base, value;
};

static void cfg_draw_common(struct scroller_state *sst, const struct scroller_config *cfg, int *edit_value)
{
	const struct configtree_t * it = scroller_get(sst, cfg);
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
		const struct configtree_t * it = scroller_get(&scr->sst, scr->cfg);
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

static void cfg_list_push(const struct scroller_config *it)
{
	struct ss_cfg_list *ss_it = sstack_alloc(&cfg_list_class);
	scroller_reset(&ss_it->sst);
	ss_it->cfg = it;
	sstack_push();
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

static const char * edit_options_cb(const struct scroller_config *cfg, int index, bool testonly)
{
	struct ss_cfg_edit *owner = CONTAINER_OF(cfg, struct ss_cfg_edit, cfg);
	return owner->item->options->options[index];
}

static int get_editable_numeric_value(struct ss_cfg_edit *owner, int index)
{
	if(index == 0)
		return owner->item->numeric->min;

	if(owner->base + owner->step * index > owner->item->numeric->max 
			&& owner->base + owner->step * (index-1) < owner->item->numeric->max)
		return owner->item->numeric->max;

	return owner->base + owner->step * index;
}

static int get_editable_numeric_index(struct ss_cfg_edit *owner, int value)
{
	if(value == owner->item->numeric->min)
		return 0;

	return (value - owner->base) / owner->step;
}

static const char *edit_numeric_cb(const struct scroller_config *cfg, int index, bool testonly)
{
	struct ss_cfg_edit *owner = CONTAINER_OF(cfg, struct ss_cfg_edit, cfg);
	static char buf[20];
	int v = get_editable_numeric_value(owner, index);
	if(v > owner->item->numeric->max)
		return NULL;

	if(testonly)
		return "";

	numeric2string(owner->item->numeric, v, buf, false);
	return buf;
}

static void cfg_edit_button(void *_it, int but, int increment)
{
	struct ss_cfg_edit *scr = _it;
	int type = scr->item->flags & F_TYPEMASK;
	but = scroller_button(&scr->sst, &scr->cfg, but, increment);

	if(type == F_NUMERIC) {
		scr->value = get_editable_numeric_value(scr, scr->sst.cidx);

	} else {
		scr->value = scr->sst.cidx;
	}

	if(but & ONOFF_CLICK) {
		sstack_pop();
		return;
	}

	if(but & M_CLICK) {
		if(scr->item->flags & F_CALLBACK) {
			scr->item->numeric_cb->callback(scr->value);
		} else {
			ptr_set(scr->item->ptr, scr->value);
		}
		sstack_pop();
		return;
	}
}

static const struct stack_class cfg_edit_class = {
	sizeof(struct ss_cfg_edit),
	.idle = cfg_edit_idle,
	.button = cfg_edit_button,
};

static void cfg_edit_push(const struct configtree_t *it, const struct scroller_config *cfg)
{
	struct ss_cfg_edit *ss_it = sstack_alloc(&cfg_edit_class);
	ss_it->parent = (struct ss_cfg_list*)sstack_current->userdata;

	int type = it->flags & F_TYPEMASK;

	if(type == F_NUMERIC) {
		ss_it->item = it;
		ss_it->step = it->numeric->step;
		if(!ss_it->step)
			ss_it->step = 1;
		ss_it->base = it->numeric->min;
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

// button repeat logic
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
