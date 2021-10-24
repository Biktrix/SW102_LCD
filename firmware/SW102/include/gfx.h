#pragma once
#include <stdbool.h>

struct image {
	const unsigned char *data;
	unsigned short w,h;
};
#define DEFINE_IMAGE(name) \
	const struct image img_ ## name = { \
		name ## _bits, name ## _width, name ## _height \
	};

struct font {
	const struct image *img;
	const char *chars;
	int nchars;
	const unsigned short *offsets;
};

#define DEFINE_FONT(name, chars, ...) \
	DEFINE_IMAGE(font_ ## name); \
	const struct font font_ ## name = { \
		&img_font_ ## name, \
		chars, \
		sizeof(chars)-1, \
		(const unsigned short[]){ 0, __VA_ARGS__, font_ ## name ## _width } \
	};

void img_draw_clip(const struct image *src, int x0, int y0, int cx, int cy, int w, int h);
inline static void img_draw(const struct image *src, int x0, int y0) {
	img_draw_clip(src, x0, y0, 0, 0, src->w, src->h);
}
void draw_hline(int x0, int x1, int y);

void fill_rect(int x0, int y0, int w, int h, bool v);
inline static void clear_all() { fill_rect(0,0,64,128, false); }

int font_getchar(const struct font *fnt, char c, int *cx);
int font_length(const struct font *fnt, const char *txt);

enum font_align_t {
	AlignLeft,
	AlignRight,
	AlignCenter
};
int font_text(const struct font *fnt, int x, int y, const char *txt, enum font_align_t align);
