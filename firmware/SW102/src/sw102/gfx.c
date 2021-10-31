#include "gfx.h"
#include "lcd.h"

void img_draw_clip(const struct image *src, int x0, int y0, int cx, int cy, int w, int h, int flags)
{
	int y,x;
	const unsigned char *srcptr = src->data;

	srcptr += cy * ((src->w+7)/8);

	for(y=0;y < h;y++) {
		unsigned char bits;
		for(x=0; x < w;x++) 
			if(!((srcptr[(x+cx)/8] >> ((x+cx)&7)) & 1))
				lcd_pset(x0+x, y+y0, !(flags & DrawInvert));
		srcptr += (src->w+7)/8;
	}
}

void fill_rect(int x0, int y0, int w, int h, bool v)
{
	int y,x;
	for(y=0;y<h;y++)
		for(x=0;x<w;x++)
			lcd_pset(x0+x,y0+y, v);
}

void draw_hline(int x0, int x1, int y)
{
	while(x0 < x1)
		lcd_pset(x0++, y, true);
}

int font_getchar(const struct font *fnt, char c, int *cx)
{
	int i0=0;
	int i1=fnt->nchars;
	while(i0<i1) {
		int m = (i0+i1)/2;
		if(fnt->chars[m] < c) {
			i0 = m+1;
		} else if(fnt->chars[m] > c) {
			i1 = m;
		} else {
			if(cx)
				*cx = fnt->offsets[m];
			return fnt->offsets[m+1] - fnt->offsets[m];
		}
	}
	return 0;
}

int font_length(const struct font *fnt, const char *txt)
{
	int l = 0;
	while(*txt)
		l += font_getchar(fnt, *txt++, 0);
	return l;
}

int font_text(const struct font *fnt, int x, int y, const char *txt, int flags)
{
	int align = flags & AlignMask;
	if(align == AlignRight) {
		x = x - font_length(fnt, txt);
	} else if(align == AlignCenter) {
		x = x - font_length(fnt, txt)/2;
	}

	while(*txt) {
		int cx, l;
		l = font_getchar(fnt, *txt++, &cx);
		if(l > 0) {
			img_draw_clip(fnt->img, x, y, cx, 0, l, fnt->img->h, flags);
			x += l;
		}
	}

	return x;
}

