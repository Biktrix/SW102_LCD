#include "gfx.h"
#include "lcd.h"

void img_draw_clip(const struct image *src, int x0, int y0, int cx, int cy, int w, int h, int flags)
{
	const unsigned char *srcptr = src->data;

	if(x0 < 0) {
		w += x0;
		cx -= x0;
		x0 = 0;
	}

	if(y0 < 0) {
		h += y0;
		cy -= y0;
		y0 = 0;
	}

	if(w > 64 - x0)
		w = 64 - x0;
	if(h > 128 - y0)
		h = 128 - y0;
	if(w <= 0 || h <= 0)
		return;

	srcptr += cx * ((src->h+7)/8) + (cy/8);
	cy&=7;

	int lshift = 8-cy+(y0&7);
	bool skipfirst = false;
	if(lshift >= 8) {
		lshift -= 8;
		skipfirst = true;
	}
	unsigned char lmask = 0xff<<(y0&7);
	int K = h-8+(y0&7);
	int loops = K/8;
	unsigned char rmask = 0;
	if(K >= 0) 
		rmask = 0xff>>(8-(K&7));
	else 
		lmask&=(0xff>>(8-((h+(y0&7))&7)));

	unsigned char *dst = framebuffer.u8 + x0*(128/8) + (y0/8);

#define BLIT_LOOP(op) \
	for(int xx=0;xx<w;xx++) { 				\
		unsigned char *dptr = dst;			\
		const unsigned char *sptr = srcptr;		\
\
		unsigned short tmp = (*sptr++ >> cy) << (y0&7);	\
		if(!skipfirst) tmp |= (*sptr++ << lshift);	\
\
		*dptr++ op ((~tmp) & lmask);			\
\
		for(int i=0;i<loops;i++) {			\
			tmp=(*sptr++ << lshift) | (tmp>>8);	\
			*dptr++ op (~tmp);			\
		}						\
\
		if(rmask) {					\
			tmp=(*sptr++ << lshift) | (tmp>>8);	\
			*dptr op ((~tmp) & rmask); 		\
		}	\
\
		dst += (128/8);	\
		srcptr += (src->h+7)/8;	\
	}

	if(flags & DrawInvert) {
		BLIT_LOOP(&=~)
	} else {
		BLIT_LOOP(|=)
	}
}

void fill_rect(int x0, int y0, int w, int h, bool v)
{
	if(x0 < 0) {
		w += x0;
		x0 = 0;
	}

	if(y0 < 0) {
		h += y0;
		y0 = 0;
	}
	if(w > 64 - x0)
		w = 64 - x0;
	if(h > 128 - y0)
		h = 128 - y0;

	if(w <= 0 || h <= 0)
		return;

	int y,x;
	uint32_t *dst = framebuffer.u32 + x0*(128/32) + y0/32;
	uint32_t lmask = (~0U) << (y0&31);
	uint32_t rmask = (~0U) >> ((32-((y0+h)&31))&31);
	
	int loops = (y0+h-1)/32 - (y0/32) + 1;
#define FILL_LOOP(op) \
	for(x=0;x<w;x++) { 		\
		uint32_t *dptr=dst;	\
		uint32_t mask = lmask; 	\
		for(y=0;y<loops;y++) {	\
			if(y == loops - 1)	\
				mask &= rmask;	\
			*dptr++ op mask; \
			mask = ~0; \
		} \
		dst += (128/32); \
	}

	if(v)
		FILL_LOOP(|=)
	else
		FILL_LOOP(&=~)
}

void draw_hline(int x0, int x1, int y)
{
	uint8_t *dptr = framebuffer.u8 + x0 * (128/8) + y/8;
	uint8_t px = 1<<(y&7);
	while(x0++ < x1) {
		*dptr |= px;
		dptr += 128/8;
	}
}

void draw_vline(int x0, int y0, int y1)
{
	fill_rect(x0, y0, 1, y1-y0, true);
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
	if(align == AlignRight)
		x -= font_length(fnt, txt);
	else if(align == AlignCenter)
		x -= font_length(fnt, txt)/2;

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

