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
		if (it->flags & F_RO) {}
			// nop
		else if (type == F_SUBMENU)
			cfg_list_push(it->submenu);
		else if (type == F_BUTTON)
			it->action(it);
		else if (type == F_NUMERIC || type == F_OPTIONS)
			cfg_edit_push(it, scr->cfg);
	}
}

static const struct stack_class cfg_list_class = {
	sizeof(struct ss_cfg_list),
	.idle = cfg_list_idle,
	.button = cfg_list_button,
};

static void cfg_list_push_class(const struct scroller_config *it, const struct stack_class *klass){
	struct ss_cfg_list *ss_it = sstack_alloc(klass);
	scroller_reset(&ss_it->sst);
	ss_it->cfg = it;
	sstack_push();
}

static void cfg_list_push(const struct scroller_config *it){
	cfg_list_push_class(it, &cfg_list_class);
}

static void cfg_edit_idle(void *_it){
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
	if(it)
		*it = &tmpitem;
	return true;
}

static int get_editable_numeric_value(struct ss_cfg_edit *owner, int index)
{
	const struct cfgnumeric_t *num = owner->item->numeric;
	if(index == 0)
		return num->max;

	int k = (num->max - num->min) % owner->step;
	if(k)
		k = owner->step - k;
	int max_aligned = num->max + k;
	return max_aligned - owner->step * index;
}

static int get_editable_numeric_index(struct ss_cfg_edit *owner, int value)
{
	const struct cfgnumeric_t *num = owner->item->numeric;
	int k = (num->max - num->min) % owner->step;
	if(k)
		k = owner->step - k;
	int max_aligned = num->max + k;

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
	
	if(type == F_NUMERIC)
		scr->value = get_editable_numeric_value(scr, scr->sst.cidx);
	else
		scr->value = scr->sst.cidx;

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
		if(scr->item->flags & F_CALLBACK)
			pop = scr->item->numeric_cb->update(scr->item, scr->value);
		else
			ptr_set(scr->item->ptr, scr->value);

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

static void cfg_edit_push(const struct configtree_t *it, const struct scroller_config *cfg){
	cfg_edit_push_class(it, cfg, &cfg_edit_class);
}

extern const struct scroller_config cfg_root;
static void cfg_enter(){
	sstack_reset();
	cfg_list_push(&cfg_root);
}

static void cfg_idle(){
	clear_all();
	sstack_idle();
	lcd_refresh();
}

static void cfg_handle_button(int but, int increment){
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

static void cfg_button_repeat(bool speedup){
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
// original firmware uses defines assist level as a factor:
// 	output current = ADC_scale * assist level * pedal power (or equivalent pedal power in torque mode)
// ADC_scale is 0.15625A, apparently this is the unit returned by the ADC
//
// we want:
// 	output power = assist_level_new * pedal power
// 	output current = assist_level_new * pedal power / output_voltage
// this gives:
// 	assist_level_new = ADC_scale * assist_level * output_voltage
//
// assist_level_new can be simply understood as a power factor
//
// we will compute the assist level (as understood by the motor controller) at runtime based
// and current battery voltage
// 
// it would be nice if the values stored in flash were approximately compatible with the old firmware
// to achieve this, we scale them by a reference voltage, taken from the motor variant (36/48V)

// the values in preview_alevels are simple power percentage factors
// (a value of 100 means that the motor power equals pedal power)
//
// in case of walk assist, we don't change anything, the levels specify motor duty cycle
static bool editing_walk_assist;
static unsigned int preview_alevels[ASSIST_LEVEL_NUMBER];
static void copy_alevels_to_preview()
{
	if(editing_walk_assist) {
		for(int i=0;i<ASSIST_LEVEL_NUMBER;i++)
			preview_alevels[i] = ui_vars.ui8_walk_assist_level_factor[i];
	} else {
		int voltage = ui_vars.ui8_motor_type ? 36 : 48 ;
		for(int i=0;i<ASSIST_LEVEL_NUMBER;i++)
			preview_alevels[i] = ((unsigned int)ui_vars.ui16_assist_level_factor[i] * voltage + 32/* rounding*/) / 64;
	}
}

static void copy_preview_to_alevels()
{
	if(editing_walk_assist) {
		for(int i=0;i<ASSIST_LEVEL_NUMBER;i++)
			ui_vars.ui8_walk_assist_level_factor[i] = preview_alevels[i];
	} else {
		int voltage = ui_vars.ui8_motor_type ? 36 : 48;
		for(int i=0;i<ASSIST_LEVEL_NUMBER;i++)
			ui_vars.ui16_assist_level_factor[i] = preview_alevels[i] * 64 / voltage;
	}
}

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

	int topv = 25;
	while(topv < topv_min) {
		if(topv * 2 > topv_limit)
			break;
		topv *= 2;
	}

	char buf[10];
	if(topv > 100)
		sprintf(buf, "%dx", topv/100);
	else
		sprintf(buf, "%d%%", topv);
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

extern const struct assist_scroller_config cfg_assist, cfg_walk_assist;

static void cfg_assist_idle(void *_it)
{
	struct ss_cfg_list *scr = _it;
	cfg_list_idle(_it);
		
	int assist_menu_items = ((const struct assist_scroller_config*)scr->cfg)->n_menuitems;
	
	int current = -1;
	if(scr->cfg == &cfg_assist.scroller) // compute this only for the toplevel assist menu
		current = ui_vars.ui8_number_of_assist_levels - 1 - (scr->sst.cidx - assist_menu_items);
	else if(scr->cfg == &cfg_walk_assist.scroller) // or this one
		current = ui_vars.ui8_number_of_assist_levels - 1 - (scr->sst.cidx - assist_menu_items);

	assist_draw_levels(current);
}

static void cfg_assist_sub_idle(void *_it){
	cfg_list_idle(_it);
	assist_draw_levels(-1);
}

struct ss_cfg_edit_assist {
	struct ss_cfg_edit edit;
	int current;
};

static void cfg_assist_edit_idle(void *_it){
	struct ss_cfg_edit_assist *e = _it;
	cfg_edit_idle(_it);
	assist_draw_levels(e->current);
}

static const struct stack_class cfg_assist_edit_class = {
	sizeof(struct ss_cfg_edit_assist),
	.idle = cfg_assist_edit_idle,
	.button = cfg_edit_button,
};

static void cfg_assist_button(void *_it, int but, int increment){
	struct ss_cfg_list *scr = _it;
	if(but & M_CLICK) {
		// ensure that submenus receive the current assist level so that the highlight can follow it
		// the rest of the magic happens in enumerate_assist_levels called by scroller_configtree_get
		int assist_menu_items = ((const struct assist_scroller_config*)scr->cfg)->n_menuitems;
		int current = ui_vars.ui8_number_of_assist_levels - 1 - (scr->sst.cidx - assist_menu_items);
		
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

void cfg_push_assist_screen(const struct configtree_t *ign){
	editing_walk_assist = false;
	copy_alevels_to_preview();
	cfg_list_push_class(&cfg_assist.scroller, &cfg_assist_class);
}

void cfg_push_walk_assist_screen(const struct configtree_t *ign){
	editing_walk_assist = true;
	copy_alevels_to_preview();
	cfg_list_push_class(&cfg_walk_assist.scroller, &cfg_assist_class);
}

static bool alevel_update(const struct configtree_t *it, int value){
	copy_preview_to_alevels();
}

static void alevel_preview(const struct configtree_t *it, int value){
	int level = (unsigned int*)it->numeric->ptr.ptr - preview_alevels;
	copy_alevels_to_preview();

	for(int i=0;i < ASSIST_LEVEL_NUMBER;i++) {
		if(i <= level && preview_alevels[i] >= value)
			preview_alevels[i] = value;
		if(i >= level && preview_alevels[i] <= value)
			preview_alevels[i] = value;
	}
}

static void alevel_revert(const struct configtree_t *it){
	copy_alevels_to_preview();
}

void rescale_preview(const struct configtree_t *it, int value){
	copy_alevels_to_preview();

	for(int i=0;i < ASSIST_LEVEL_NUMBER;i++) 
		preview_alevels[i] = preview_alevels[i] * value / 100;
}

bool rescale_update(const struct configtree_t *it, int value){
	rescale_preview(it, value);
	copy_preview_to_alevels();
	return true;
}

void rescale_revert(const struct configtree_t *it){
	copy_alevels_to_preview();
}

bool enumerate_assist_levels(const struct scroller_config *cfg, int index, const struct scroller_item_t **it){
	int assist_menu_items = ((const struct assist_scroller_config*)cfg)->n_menuitems;

	if(index < assist_menu_items) {// regular menu
		return configtree_scroller_item_callback(cfg, index, it);

	} else {
		int current = ui_vars.ui8_number_of_assist_levels - 1 - (index - assist_menu_items);
		if(current < 0)
			return false;

		if(it) {
			// assist levels
			const struct configtree_t *template = &((const struct configtree_t*)cfg->list)[assist_menu_items];
			struct cfgnumeric_cb_t *num = (struct cfgnumeric_cb_t*)template->numeric_cb;
			num->numeric.ptr.ptr = &preview_alevels[current];
			num->numeric.ptr.size = sizeof(preview_alevels[current]);
			num->update = alevel_update;
			num->preview = alevel_preview;
			num->revert = alevel_revert;
			sprintf((char*)template->scrollitem.text, "Level %d", current+1);
			*it = &template->scrollitem;
		}
		return true;
	}
	return false;
}

extern const struct scroller_config cfg_levels_extend, cfg_levels_truncate;
static int tmp_assist_levels;

// warning: we totally ignore walk assist levels here, sorry
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
			ui_vars.ui16_assist_level_factor[i] = ui_vars.ui16_assist_level_factor[0];
		ui_vars.ui8_number_of_assist_levels = newv;
		copy_alevels_to_preview();
		return true;

	} else if(newv > ui_vars.ui8_number_of_assist_levels)
		cfg_list_push_class(&cfg_levels_extend, &cfg_assist_sub_class);
	else
		cfg_list_push_class(&cfg_levels_truncate, &cfg_assist_sub_class);

	tmp_assist_levels = newv;
	return false;
}

void do_resize_assist_levels(const struct configtree_t *ign){
	int oldv = ui_vars.ui8_number_of_assist_levels;
	int newv = tmp_assist_levels;

	for(int i = oldv; i < newv;i++) { // if extending, extrapolate linearly
		// linear extrapolation
		ui_vars.ui16_assist_level_factor[i] = ui_vars.ui16_assist_level_factor[i-1] * 2 - ui_vars.ui16_assist_level_factor[i-2];
	}
	ui_vars.ui8_number_of_assist_levels = newv;

	copy_alevels_to_preview();
	sstack_pop(); // extend/interpolate menu
	sstack_pop(); // assist level spinner
}

void do_interpolate_assist_levels(const struct configtree_t *ign){
	int oldv = ui_vars.ui8_number_of_assist_levels;
	int newv = tmp_assist_levels;
	uint16_t tmp[ASSIST_LEVEL_NUMBER];
	tmp[0] = ui_vars.ui16_assist_level_factor[0];

	for(int i=1;i < newv-1;i++) {
		int j = i * 256 * (oldv-1) / (newv-1);
		int a = ui_vars.ui16_assist_level_factor[j>>8];
		int b = ui_vars.ui16_assist_level_factor[(j>>8)+1];
		int frac = j&255;
		tmp[i] = (a * (256-frac) + b * frac) >> 8;
	}

	tmp[newv-1] = ui_vars.ui16_assist_level_factor[oldv-1];
	memcpy(ui_vars.ui16_assist_level_factor, tmp, sizeof(tmp));
	copy_alevels_to_preview();

	ui_vars.ui8_number_of_assist_levels = newv;
	sstack_pop(); // extend/interpolate menu
	sstack_pop(); // assist level spinner
}

// torque sensor calibration  ======================================================================

#define CALIB_LEVEL_NUMBER (8)

typedef unsigned short calibdata_t[2];

static unsigned short *calibration_preview_ptr;
static unsigned short calibration_preview_value;
static calibdata_t *current_cdata;

static void calibration_draw_chart()
{
	const int y0 = 76;
	// clear some overflown parts of the scroller
	fill_rect(0, y0, 64, 20, false);
	
	calibdata_t *cdata = current_cdata;
	int xmax = cdata[7][0];
	int ymax = cdata[7][1];

	unsigned short save = 0;
	if(calibration_preview_ptr) {
		save = *calibration_preview_ptr;
		*calibration_preview_ptr = calibration_preview_value;

		if(tick & 8) {
			for(int i=0;i<CALIB_LEVEL_NUMBER;i++) {
				if(&cdata[i][0] == calibration_preview_ptr) {
					int x = cdata[i][0] * 63 / xmax;
					draw_vline(x, y0, 128);
				}
				if(&cdata[i][1] == calibration_preview_ptr) {
					int y = 127 - (128-y0) * cdata[i][1] / ymax;
					draw_hline(0, 64, y);
				}
			}
		}
	}

	int idx=0;
	for(int x=0;x<64;x++) {
		int wgt = x * xmax / 63;
		for(;idx < CALIB_LEVEL_NUMBER-1;idx++)
			if(wgt < cdata[idx+1][0])
				break;

		int xdelta = cdata[idx+1][0] - cdata[idx][0];
		int ydelta = cdata[idx+1][1] - cdata[idx][1];
		if(xdelta <= 0) // invalid data
			continue;

		int v = (wgt - cdata[idx][0]) * ydelta / xdelta + cdata[idx][1];

		int y = 127 - (128-y0) * v / ymax;
		if(y >= y0 && y < 128)
			lcd_pset(x, y, true);
	}

	if(calibration_preview_ptr) {
		*calibration_preview_ptr = save;
	}
}

extern const struct scroller_config cfg_calibration;

static void cfg_calib_idle(void *_it)
{
	struct ss_cfg_list *scr = _it;
	cfg_list_idle(_it);

	calibration_draw_chart();
}

static void cfg_calib_edit_idle(void *_it)
{
	cfg_edit_idle(_it);
	calibration_draw_chart();
}

static const struct stack_class cfg_calib_edit_class = {
	sizeof(struct ss_cfg_edit),
	.idle = cfg_calib_edit_idle,
	.button = cfg_edit_button,
};

static void cfg_calib_button(void *_it, int but, int increment)
{
	struct ss_cfg_list *scr = _it;
	if(but & M_CLICK) {
		const struct configtree_t * it = scroller_configtree_get(&scr->sst, scr->cfg);
		cfg_edit_push_class(it, scr->cfg, &cfg_calib_edit_class);

		// trigger the preview callback immediately, so that the chart display starts showing the right item
		if(it->numeric_cb->preview)
			it->numeric_cb->preview(it, ptr_get(&it->numeric->ptr));

		return;
	}

	cfg_list_button(_it, but, increment);
}

static const struct stack_class cfg_calib_class = {
	sizeof(struct ss_cfg_list),
	.idle = cfg_calib_idle,
	.button = cfg_calib_button,
};

static bool clevel_update(const struct configtree_t *it, int value)
{
	ptr_set(&it->numeric->ptr, value);
	return true;
}

static void clevel_preview(const struct configtree_t *it, int value)
{
	calibration_preview_ptr = it->numeric->ptr.ptr;
	calibration_preview_value = value;
}

static void clevel_revert(const struct configtree_t *it)
{
	calibration_preview_ptr = 0;
}

bool enumerate_calibration(const struct scroller_config *cfg, int index, const struct scroller_item_t **it)
{
	int current = index;
	// ui_vars.ui16_torque_sensor_calibration_table_right
	if(current < 0 || current >= CALIB_LEVEL_NUMBER*2)
		return false;

	if(it) {
		// assist levels
		const struct configtree_t *template = &((const struct configtree_t*)cfg->list)[index&1];
		struct cfgnumeric_cb_t *num = (struct cfgnumeric_cb_t*)template->numeric_cb;
		num->update = clevel_update;
		num->preview = clevel_preview;
		num->revert = clevel_revert;
		if(!(current&1)) {
			sprintf((char*)template->scrollitem.text, "Weight %d", current/2);
		} else {
			sprintf((char*)template->scrollitem.text, "ADC val %d", current/2);
		}
		num->numeric.ptr.ptr = &current_cdata[current/2][current&1];
		num->numeric.ptr.size = 2;
		*it = &template->scrollitem;
	}
	return true;

}

void cfg_push_calibration_screen(const struct configtree_t *it){
	if(it->scrollitem.text[0] == 'L') {
		current_cdata = ui_vars.ui16_torque_sensor_calibration_table_left;
		cfg_list_push_class(&cfg_calibration, &cfg_calib_class);
		// left
	}  else {
		current_cdata = ui_vars.ui16_torque_sensor_calibration_table_right;
		cfg_list_push_class(&cfg_calibration, &cfg_calib_class);
		// right
	}
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
