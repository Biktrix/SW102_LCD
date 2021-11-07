#pragma once
#include <stdbool.h>
struct font;
extern const struct font font_full;

struct stack_class;
struct stack_screen {
	const struct stack_class *klass;
	unsigned char userdata[0];
};
// stack
struct stack_class {
	int size;
	void (*enter)(void *it, bool pop);
	void (*idle)(void *it);
	void (*button)(void *it, int btn, int extra);
	void (*leave)(void *it, bool pop);
};

void sstack_reset();
void *sstack_alloc(const struct stack_class *klass);
void sstack_push();
void sstack_pop();
void sstack_idle();
void sstack_button(int btn, int extra);
extern struct stack_screen *sstack_current;

// scroller
struct configtree_t;
struct scroller_config;
typedef const char *(scroller_item_callback)(const struct scroller_config *cfg, int index, bool testonly);
struct scroller_config {
	int pitch, winy, winh, y0, y1;
	const struct configtree_t *list;
	scroller_item_callback *cb;
};

struct scroller_state {
	int xscroll, yscroll, cidx;
};

void scroller_reset(struct scroller_state *st);
const struct configtree_t *scroller_get(struct scroller_state *st, const struct scroller_config *cfg);
void scroller_draw_list(struct scroller_state *st, const struct scroller_config *cfg);
void scroller_draw_item(struct scroller_state *st, const struct scroller_config *cfg);
int scroller_button(struct scroller_state *st, const struct scroller_config *cfg, int but, int increment);

// config tree

enum {
	F_SUBMENU=1,
	F_BUTTON,
	F_NUMERIC,
	F_OPTIONS,
	F_TYPEMASK=0xff,
	F_RO=0x100, 
	F_CALLBACK=0x200
};


struct configtree_t;
typedef void (cfgaction_t)(const struct configtree_t *ign);

struct cfgptr_t {
	void *ptr; int size;
};
int ptr_get(const struct cfgptr_t *it);
void ptr_set(const struct cfgptr_t *it, int v);

struct cfgnumeric_t {
	struct cfgptr_t ptr;
	int decimals;
	const char *unit;
	int min, max, step;
};

void numeric2string(const struct cfgnumeric_t *num, int v, char *out, bool include_unit);

typedef void (wr_callback_t)(int value);

struct cfgnumeric_cb_t {
	struct cfgnumeric_t numeric;
	wr_callback_t *callback;
};

struct cfgoptions_t {
	struct cfgptr_t ptr;
	const char **options;
};

#define PTRSIZE(a) 	{ &(a), sizeof(a) }

struct configtree_t {
	const char *text;
	int flags;
	union {
		const struct scroller_config *submenu;
		cfgaction_t *action;
		const struct cfgptr_t *ptr;
		const struct cfgnumeric_t *numeric;
		const struct cfgnumeric_cb_t *numeric_cb;
		const struct cfgoptions_t *options;
	};
};


