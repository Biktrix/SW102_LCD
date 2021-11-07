#include "screen_cfg_utils.h"
#include "gfx.h"
#include "ui.h"
#include "lcd.h"
#include "state.h"
#include "buttons.h"
#include "eeprom.h"
#include <stdlib.h>
#include <stdio.h>
#include "ui.h"
#include <string.h>

static unsigned char stack_storage[256];
struct stack_screen *sstack_current;

#define STACK_CALLBACK(it, cb, ...) if(it->klass->cb) it->klass->cb(it->userdata, ##__VA_ARGS__)

void sstack_reset()
{
	sstack_current = NULL;
}

void *sstack_alloc(const struct stack_class *klass)
{
	unsigned char *stack_end = stack_storage;
	if(sstack_current)
		stack_end = (sstack_current->userdata) + sstack_current->klass->size;

	if(stack_end + sizeof(struct stack_screen) + klass->size > stack_storage + sizeof(stack_storage)) {
#ifndef NRF51 
		fprintf(stderr, "Out of sstack space!\n");
#endif
		for(;;);
	}

	struct stack_screen *ret = (struct stack_screen*)stack_end;
	ret->klass = klass;
	return ret->userdata;
}

void sstack_push()
{
	unsigned char *stack_end = stack_storage;
	if(sstack_current) {
		stack_end = (sstack_current->userdata) + sstack_current->klass->size;
		STACK_CALLBACK(sstack_current, leave, false);
	}

	sstack_current = (struct stack_screen*)stack_end;
	STACK_CALLBACK(sstack_current, enter, false);
}

void sstack_pop()
{
	if(!sstack_current)
		return;

	STACK_CALLBACK(sstack_current, leave, true);

	// find new top
	struct stack_screen *old = sstack_current;
	sstack_current = NULL;

	for(struct stack_screen *tmp = (struct stack_screen*)stack_storage;
		tmp != old;
		tmp = (struct stack_screen*)(tmp->userdata + tmp->klass->size)) {
		sstack_current = tmp;
	}

	if(sstack_current) 
		STACK_CALLBACK(sstack_current, enter, true);
}

void sstack_idle()
{
	if(sstack_current)
		STACK_CALLBACK(sstack_current, idle);
}

void sstack_button(int btn, int extra)
{
	if(sstack_current)
		STACK_CALLBACK(sstack_current, button, btn, extra);
}

const
#include "font_full.xbm"
DEFINE_FONT(full, 
	" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
	5,7,11,20,26,37,46,48,52,56,62,70,72,76,78,83,90,96,103,110,117,124,131,138,145,152,154,156,165,174,183,189,201,210,217,224,232,239,245,253,261,263,267,274,280,289,297,305,312,320,328,335,343,351,360,372,380,388,396,399,404,407,414,421,424,431,438,444,451,458,463,470,477,479,482,488,490,500,507,514,521,528,533,539,544,551,558,568,575,582,588,594,596,602
);

int ptr_get(const struct cfgptr_t *it)
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

void ptr_set(const struct cfgptr_t *it, int v)
{
	if(it->size == 1) {
		*((unsigned char*)it->ptr) = v;
	} else if(it->size == 2) {
		*((unsigned short*)it->ptr) = v;
	} else if(it->size == 4) {
		*((unsigned int*)it->ptr) = v;
	}
}

void numeric2string(const struct cfgnumeric_t *num, int v, char *out, bool include_unit)
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

void scroller_reset(struct scroller_state *st)
{
	st->cidx = st->xscroll = st->yscroll = 0;
}

#define INVERTED_DRAW 1

static const char * default_scroller_item_callback(const struct scroller_config *cfg, int index, bool testonly)
{
	return cfg->list[index].text;
}

void scroller_draw_list(struct scroller_state *st, const struct scroller_config *cfg)
{
	scroller_item_callback *cb = cfg->cb ? cfg->cb : default_scroller_item_callback;

	if(st->yscroll > 0)
		st->yscroll-=2;
	if(st->yscroll < 0)
		st->yscroll+=2;

	for(int i=-1;i + st->cidx >= 0;i--){
		int y = cfg->winy + cfg->pitch * i + st->yscroll;
		if(y + font_full.img->h <= cfg->y0)
			break;
		font_text(&font_full, 32, y, cb(cfg, st->cidx + i, false), AlignCenter);
	}

	for(int i=1;;i++){
		const char *text = cb(cfg, st->cidx+i, false);
		if(!text)
			break;
		int y = cfg->winy + cfg->winh + cfg->pitch * (i-1) + st->yscroll;
		if(y >= cfg->y1)
			break;

		font_text(&font_full, 32, y, text, AlignCenter);
	}
}

void scroller_draw_item(struct scroller_state *st, const struct scroller_config *cfg)
{
	scroller_item_callback *cb = cfg->cb ? cfg->cb : default_scroller_item_callback;

	// frame for the current selection
	fill_rect(0,cfg->winy-2,64,cfg->winh, true);

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
	
	font_text(&font_full, -st->xscroll, cfg->winy, cb(cfg, st->cidx, false), AlignLeft | DrawInvert);
}

int scroller_button(struct scroller_state *st, const struct scroller_config *cfg, int but, int increment)
{
	scroller_item_callback *cb = cfg->cb ? cfg->cb : default_scroller_item_callback;

	if(but & UP_PRESS) {
		if(st->cidx > 0) {
			st->cidx -= increment;
			if(st->cidx < 0)
				st->cidx = 0;

			st->yscroll = -cfg->pitch;
			st->xscroll = 0;
			but &= ~UP_PRESS;
		}
	}

	if(but & DOWN_PRESS) {
		if(cb(cfg, st->cidx+1, true)) {
			++st->cidx;
			st->yscroll = cfg->pitch;
			st->xscroll = 0;
			but &= ~DOWN_PRESS;

			for(int i=1;i<increment;i++) 
				if(cb(cfg, st->cidx+1, true))
					++st->cidx;
		}
	}

	return but;
}
		
const struct configtree_t *scroller_get(struct scroller_state *st, const struct scroller_config *cfg)
{
	return &cfg->list[st->cidx];
}
