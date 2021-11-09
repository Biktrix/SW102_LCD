#pragma once
#include <stdbool.h>
struct font;
extern const struct font font_full;

/////////////////////////////////////////////////////////////////////////////////////////////////
// sstack: a stack of screens
// only the topmost screen receives idle/button callbacks
// state is retained for the hidden screens
struct stack_class;
struct stack_screen {
	const struct stack_class *klass;
	unsigned char userdata[0];
};
struct stack_class {
	int size; // size of userdata structure ('it' in callbacks)
	void (*enter)(void *it, bool pop);
	void (*idle)(void *it);
	void (*button)(void *it, int btn, int extra);
	void (*leave)(void *it, bool pop);
};

void sstack_reset();

// each call to sstack_alloc needs to be completed by sstack_push()
// the pointer returned by sstack_alloc is the userdata struct
// holding the state for the screen being pushed, it needs to be
// filled in before calling sstack_push
void *sstack_alloc(const struct stack_class *klass);
void sstack_push();

// removes the topmost screen
void sstack_pop();

// forwarded to idle/button callbacks on the topmost screen
void sstack_idle();
void sstack_button(int btn, int extra);
extern struct stack_screen *sstack_current;

/////////////////////////////////////////////////////////////////////////////////////////////////
// scroller: a vertically scrolling list of items
// each item consists of a text field and possibly some userdata ignored by the scroller
struct scroller_config;
struct scroller_item_t {
	const char *text;
};

typedef bool (scroller_item_callback)(const struct scroller_config *cfg, int index, const struct scroller_item_t **it);

// the config struct describes the dimensions of the scroller and a callback for accessing the scroller items
// if not specified, the default callback is configtree_scroller_item_callback
struct scroller_config {
	int pitch, winy, winh, y0, y1;
	const void *list;
	scroller_item_callback *cb;
};

struct scroller_state {
	int xscroll, yscroll, cidx;
};

void scroller_reset(struct scroller_state *st);
void scroller_draw_list(struct scroller_state *st, const struct scroller_config *cfg);
void scroller_draw_item(struct scroller_state *st, const struct scroller_config *cfg);
int scroller_button(struct scroller_state *st, const struct scroller_config *cfg, int but, int increment);

// accessors for using a scroller holding configtree items (see below)
bool configtree_scroller_item_callback(const struct scroller_config *cfg, int index, const struct scroller_item_t **it);
const struct configtree_t *scroller_configtree_get(struct scroller_state *st, const struct scroller_config *cfg);

/////////////////////////////////////////////////////////////////////////////////////////////////
// config tree: the main structure representing the configuration menu tree

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

// most of the configuration is stored in variables accessed by a pointer
// this struct holds a pointer and size of the variable, which is assumed to be either
// u8 u16 or u32 depending on the size
struct cfgptr_t {
	void *ptr; int size;
};
#define PTRSIZE(a) 	{ &(a), sizeof(a) }

int ptr_get(const struct cfgptr_t *it);
void ptr_set(const struct cfgptr_t *it, int v);

// numerical data - this is displayed with a numerical scroller ranging from min to max
struct cfgnumeric_t {
	struct cfgptr_t ptr;
	int decimals;
	const char *unit;
	int min, max, step;
};

void numeric2string(const struct cfgnumeric_t *num, int v, char *out, bool include_unit);

// optionally some callbacks can be specified using the struct below (F_CALLBACK needs to be marked in flags)
// if not needed, the smaller cfgnumeric_t can be used
struct cfgnumeric_cb_t;
typedef bool (wr_callback_t)(const struct configtree_t *it, int value);
typedef void (preview_callback_t)(const struct configtree_t *it, int value);
typedef void (revert_callback_t)(const struct configtree_t *it);

struct cfgnumeric_cb_t {
	struct cfgnumeric_t numeric;
	wr_callback_t *update;
	preview_callback_t *preview;
	revert_callback_t *revert;
};

// a list of options, each entry is described by some text
struct cfgoptions_t {
	struct cfgptr_t ptr;
	const char **options;
};

// the actual configtree struct
// each item in the config menu has a label. The flags specify the details of the item:
// - F_SUBMENU: a submenu, described with a scroller_config structure
// 	scroller_config::list is an array of configtree[] items representing the submenu
// - F_BUTTON: a simple button triggering an action. The action could be to show a submenu, but
// 	in usual cases you would just use F_SUBMENU
// - F_NUMERIC: numerical variable (.numeric), possibly with callbacks (F_CALLBACK, .numeric_cb),
// 	or read-only (F_RO). Callbacks + readonly make no sense
// - F_OPTIONS: option selection (.options)
struct configtree_t {
	struct scroller_item_t scrollitem;
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

struct assist_scroller_config {
	struct scroller_config scroller;
	int n_menuitems;
};
