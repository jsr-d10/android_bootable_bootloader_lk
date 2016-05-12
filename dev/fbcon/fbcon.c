/*
 * Copyright (c) 2008, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <debug.h>
#include <err.h>
#include <stdlib.h>
#include <dev/fbcon.h>
#include <platform.h>
#include <string.h>
#include <mmc_sdhci.h>
#include <sdhci_msm.h>
#include <target.h>
#include <dev/font/font-12x16.h>

#define LOGO_JSR

#ifdef LOGO_JSR
#include <splash_jsr_tech.h>
#else
#include <splash.h>
#endif

#ifdef LOGO_JSR
#define FBCON_RGB888_ONLY
#endif

#ifdef FBCON_RGB888_ONLY
#define PXL_SIZE 3
#else
#define PXL_SIZE (config->pixel_size)
#endif

static struct fbcon_config * config = NULL;
static uint8_t header_line = 13; // for 25x57 font, right under logo

static void fbcon_drawglyph(uint8_t * pixels, uint32_t paint, unsigned stride, char c)
{
	unsigned cc, gw;
	unsigned i, x, y;
	uint32_t data;
	uint8_t * glyph;

	stride -= config->con.sym_width;
	stride *= PXL_SIZE;

	gw = ((config->con.font->width - 1) / 8) + 1;
	cc = (uint8_t)c;
	if (cc < config->con.font->first_char) cc = '?';
	if (cc > config->con.font->last_char)  cc = '?';
	cc -= config->con.font->first_char;
	glyph = config->con.font->bitmap + cc * gw * config->con.font->height;

	for (y = 0; y < config->con.font->height; y++) {
		data = *(uint32_t *)glyph;
		for (x = 0; x < config->con.font->width; x++) {
			if (data & 1) {
				*(uint16_t *)pixels = (uint16_t)paint;
#ifndef FBCON_RGB888_ONLY
				if (PXL_SIZE == 3)
#endif
				*(pixels + 2) = (uint8_t)(paint >> 16);
			}
			data >>= 1;
			pixels += PXL_SIZE;
		}
		if (config->con.font->width < config->con.sym_width) {
			unsigned kx = config->con.sym_width - config->con.font->width;
			pixels += kx * PXL_SIZE;
		}
		glyph += gw;
		pixels += stride;
	}
}

static void fbcon_flush(void)
{
	if (config->update_start)
		config->update_start();
	if (config->update_done)
		while (!config->update_done());
}

static void fbcon_scroll_up(void)
{
	uint32_t * dst = config->base;
	uint32_t * src;
	uint32_t size, scanline;

	scanline = config->stride * PXL_SIZE;
	src = (uint32_t *)(config->base + config->con.sym_height * scanline);
	size = (config->height - config->con.sym_height) * scanline;
	size /= 4;

	while (size--) {
		*dst++ = *src++;
	}

	size = config->con.sym_height * scanline;
	while (size--) {
		*dst++ = 0;
	}

	fbcon_flush();
}

void fbcon_clear(void)
{
	unsigned size = config->stride * config->height * PXL_SIZE;
	memset(config->base, 0, size);
}

void fix_pos(unsigned * x, unsigned * y, unsigned * w, unsigned * h)
{
	if (x) {
		if (*x > config->con.max.x)
			*x = config->con.max.x;
	}
	if (y) {
		if (*y > config->con.max.y)
			*y = config->con.max.y;
	}
	if (w) {
		unsigned xx = x ? *x : 0;
		if (xx + *w > config->con.max.x)
			*w = config->con.max.x - xx;
	}
	if (h) {
		unsigned yy = y ? *y : 0;
		if (yy + *h > config->con.max.y)
			*h = config->con.max.y - yy;
	}
}

uint8_t * get_char_pos_ptr(unsigned x, unsigned y)
{
	unsigned offset;
	uint8_t * pixels = config->base;

	fix_pos(&x, &y, NULL, NULL);
	offset = y * config->con.sym_height * config->stride;
	offset += x * config->con.sym_width;
	pixels += offset * PXL_SIZE;
	return pixels;
} 

uint8_t * get_cursor_pos_ptr()
{
	return get_char_pos_ptr(config->con.cur.x, config->con.cur.y);
} 

void fbcon_putc(char c)
{
	uint8_t * pixels = NULL;	
	uint8_t cc = (uint8_t)c;

	/* ignore anything that happens before fbcon is initialized */
	if (!config)
		return;
	if (!config->con.font)
		return;

	if (cc == '\n')
		goto newline;

	if (cc == '\r') {
		config->con.cur.x = 0;
		return;
	}

	pixels = get_cursor_pos_ptr();
	fbcon_drawglyph(pixels, config->con.fg_color, config->stride, c);

	config->con.cur.x++;
	if (config->con.cur.x < config->con.max.x)
		return;

newline:
	config->con.cur.y++;
	config->con.cur.x = 0;
	if (config->con.cur.y >= config->con.max.y) {
		config->con.cur.y = config->con.max.y - 1;
		fbcon_scroll_up();
	} else {
		fbcon_flush();
	}
}

void fbcon_print(const char * str)
{
	while (*str) {
		fbcon_putc(*str);
		str++;
	}
}

int fbcon_vprintf(const char *fmt, va_list ap)
{
	char buf[256];
	int ret = vsnprintf(buf, sizeof(buf)-2, fmt, ap);
	if (ret > 0)
		fbcon_print(buf);
	return ret;
}

#define PRINTF(_fmt_) {     \
	va_list ap;               \
	va_start(ap, _fmt_);      \
	fbcon_vprintf(_fmt_, ap); \
	va_end(ap);               \
	}

int fbcon_printf(const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = fbcon_vprintf(fmt, ap);
	va_end(ap);
	return ret;
}

int fbcon_get_text_pos(int align, int len)
{
	switch (align) {
		case ALIGN_LEFT:
			return 0;

		case ALIGN_CENTER:
			return (config->con.max.x - len) / 2 + 1; // +1 is for \0 at end of string

		case ALIGN_RIGHT:
			return config->con.max.x - len;
	}
	dprintf(CRITICAL, "%s: wrong alignment passed: %d", __func__, align);
	return -1;
}

int fbcon_cprintf(unsigned color, const char * fmt, ...)
{
	return fbcon_acprintf(config->con.cur.y, ALIGN_LEFT, color, fmt);
}

int fbcon_cprint(const char * str, unsigned color)
{
	return fbcon_cprintf(color, str);
}

int fbcon_aprintf(int line, int align, const char * fmt, ...)
{
	return fbcon_acprintf(line, align, fbcon_get_font_fg_color(), fmt);
}

int fbcon_aprint(const char * str, int line, int align)
{
	return fbcon_aprintf(line, align, str);
}

int fbcon_acprintf(int line, int align, unsigned color, const char * fmt, ...)
{
	char buf[256];
	unsigned prev_fg_color;
	int ret, pos;
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(buf, sizeof(buf)-2, fmt, ap);
	va_end(ap);
	if (ret <= 0)
		return 0;
	pos = fbcon_get_text_pos(align, ret);
	if (pos < 0)
		return 0;
	prev_fg_color = fbcon_get_font_fg_color();
	fbcon_set_font_fg_color(color);
	fbcon_set_cursor_pos(pos, line);
	fbcon_print(buf);
	fbcon_set_font_fg_color(prev_fg_color);
	return ret;
}

int fbcon_acprint(const char * str, int line, int align, unsigned color)
{
	return fbcon_acprintf(line, align, color, str);
}

void fbcon_hprint(const char * header, unsigned color)
{
	static char previous_header[145] = { 0 }; // for tiny 5x12 font
	fbcon_acprint(previous_header, header_line, ALIGN_CENTER, BLACK);
	fbcon_acprint(header, header_line, ALIGN_CENTER, color);
	strncpy(previous_header, header, sizeof(previous_header));
}

void fbcon_set_bg(unsigned bg, unsigned x, unsigned y, unsigned w, unsigned h)
{
	unsigned ky, kx, iy, ix, iw, ih;
	unsigned stride;
	uint8_t * pixels;

	if (x == 0 && y == 0 && w == 0 && h == 0) {
		w = config->con.max.x;
		h = config->con.max.y;
	}
	fix_pos(&x, &y, &w, &h);
	pixels = get_char_pos_ptr(x, y);
	iw = w * config->con.sym_width;
	ih = h * config->con.sym_height;
	stride = (config->stride - iw) * PXL_SIZE;

	for (iy = 0; iy < ih; iy++) {
		for (ix = 0; ix < iw; ix++) {
			*(uint16_t *)pixels = (uint16_t)bg;
#ifndef FBCON_RGB888_ONLY
			if (PXL_SIZE == 3)
#endif
			*(pixels + 2) = (uint8_t)(bg >> 16);
			pixels += PXL_SIZE;
		}
		pixels += stride;
	}
}

void fbcon_set_cursor_pos(unsigned x, unsigned y)
{
	fix_pos(&x, &y, NULL, NULL);
	config->con.cur.x = x;
	config->con.cur.y = y;
}

void fbcon_optimize_font_bitmap(struct raster_font * font)
{
	unsigned gw, i, size;
	uint8_t * bitmap;

	if (!font->processed) {
		gw = ((font->width - 1) / 8) + 1;
		size = font->last_char - font->first_char + 1;
		size = gw * font->height * size;
		bitmap = font->bitmap;
		for (i = 0; i < size; i++) {
			uint8_t x = bitmap[i];

			// Classical implementation (can be extended to 16 and 32 bits)
			// x = ((x & 0xF0) >> 4) | ((x & 0x0F) << 4);
			// x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
			// x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
			// bitmap[i] = x;

			// reverse bits on 32-bit systems (this is faster)
			bitmap[i] = ((x * 0x0802LU & 0x22110LU) | (x * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
		}
		font->processed = 1;
	}
}

void fbcon_set_font_size(unsigned width, unsigned height)
{
	config->con.sym_width = width;
	config->con.sym_height = height;
	config->con.max.x = (config->width - 1) / width;
	config->con.max.y = (config->height - 1) / height;
}

unsigned fbcon_get_font_fg_color(void)
{
	return config->con.fg_color;
}

void fbcon_set_font_fg_color(unsigned color)
{
	config->con.fg_color = color;
}

unsigned fbcon_get_font_bg_color(void)
{
	return config->con.bg_color;
}

void fbcon_set_font_bg_color(unsigned color)
{
	config->con.bg_color = color;
}

void fbcon_set_font_color(unsigned fg, unsigned bg)
{
	fbcon_set_font_fg_color(fg);
	fbcon_set_font_bg_color(bg);
}

unsigned fbcon_get_header_line(void)
{
	return header_line;
}

void fbcon_set_header_line(unsigned line)
{
	header_line = line;
}

void fbcon_set_font_type(struct raster_font * font)
{
	if (font) {
		config->con.font = font;
		fbcon_optimize_font_bitmap(font);
		fbcon_set_font_size(font->width, font->height);
		fbcon_set_font_color(COLOR_WHITE, COLOR_BLACK);
		config->con.cur.x = 0;
		config->con.cur.y = 0;
	}
}

struct raster_font *fbcon_get_font_type(void)
{
	return config->con.font;
}

void fbcon_set_splash_pos(int x, int y)
{
	config->splash_pos.x = x;
	config->splash_pos.y = y;
}

void fbcon_setup(struct fbcon_config *_config)
{
	ASSERT(_config);
	config = _config;

	fbcon_set_splash_pos(-1, -1);  // SCREEN CENTER

	switch (config->format) {
#ifndef FBCON_RGB888_ONLY
	case FB_FORMAT_RGB565:
		config->pixel_size = 2;
		break;
#endif
	case FB_FORMAT_RGB888:
		config->pixel_size = 3;
		break;
	default:
		dprintf(CRITICAL, "unknown framebuffer pixel format\n");
		ASSERT(0);
		break;
	}
}

struct fbcon_config * fbcon_display(void)
{
	return config;
}

// -------------------------------------------------------------------------------

extern struct fbimage * fetch_image_from_partition();

void fbcon_putImage(struct fbimage *fbimg, bool flag);

void display_image_on_screen()
{
	struct fbimage default_fbimg;
	struct fbimage * fbimg = NULL;
	bool flag = true;

	fbcon_clear();
#ifndef LOGO_JSR
	fbimg = fetch_image_from_partition();
#endif

	if(!fbimg) {
		flag = false;
		fbimg = &default_fbimg;
		fbimg->header.width = SPLASH_IMAGE_WIDTH;
		fbimg->header.height = SPLASH_IMAGE_HEIGHT;
#ifdef LOGO_JSR
		fbimg->image = (unsigned char *)imageBuffer_jsr_tech;
#else
#if DISPLAY_TYPE_MIPI
		fbimg->image = (unsigned char *)imageBuffer_rgb888;
#else
		fbimg->image = (unsigned char *)imageBuffer;
#endif
#endif
	}

	fbcon_putImage(fbimg, flag);
}

void fbcon_putImage(struct fbimage *fbimg, bool flag)
{
	unsigned i = 0;
	unsigned total_x;
	unsigned total_y;
	unsigned bytes_per_bpp;
	unsigned image_base;
	unsigned width, pitch, height;
	unsigned char *logo_base;
	struct logo_img_header *header;

	if (!config) {
		dprintf(CRITICAL,"NULL configuration, image cannot be displayed\n");
		return;
	}

	header = &fbimg->header;
	width = pitch = header->width;
	height = header->height;
	logo_base = (unsigned char *)fbimg->image;

	total_x = config->width;
	total_y = config->height;
	bytes_per_bpp = ((config->bpp) / 8);

#if DISPLAY_TYPE_MIPI
	if (bytes_per_bpp == 3)
	{
		if(flag) {
			if (header->width == config->width && header->height == config->height)
				return;
			else {
				logo_base = (unsigned char *)config->base + LOGO_IMG_OFFSET;
				if (header->width > config->width) {
					width = config->width;
					pitch = header->width;
					logo_base += (header->width - config->width) / 2 * bytes_per_bpp;
				} else {
					width = pitch = header->width;
				}

				if (header->height > config->height) {
					height = config->height;
					logo_base += (header->height - config->height) / 2 * pitch * bytes_per_bpp;
				} else {
					height = header->height;
				}
			}
		}

		if (!flag && config->splash_pos.y >= 0) {
			image_base = (unsigned)config->splash_pos.y * config->width;
		} else {
			image_base = ((total_y / 2) - (height / 2)) * config->width;
		}
		if (!flag && config->splash_pos.x >= 0) {
			image_base += (unsigned)config->splash_pos.x;
		} else {
			image_base += (total_x / 2) - (width / 2);
		}

		for (i = 0; i < height; i++) {
			uint8_t * dst = (uint8_t *)config->base + ((image_base + (i * config->width)) * bytes_per_bpp);
			uint8_t * src = logo_base + (i * pitch * bytes_per_bpp);
			memcpy(dst, src, width * bytes_per_bpp);
		}
	}

	fbcon_flush();

#if DISPLAY_MIPI_PANEL_NOVATEK_BLUE
	if(is_cmd_mode_enabled())
		mipi_dsi_cmd_mode_trigger();
#endif

#else /* DISPLAY_TYPE_MIPI */
	if (bytes_per_bpp == 2) {
		for (i = 0; i < header->width; i++) {
			memcpy (config->base + ((image_base + (i * (config->width))) * bytes_per_bpp),
				fbimg->image + (i * header->height * bytes_per_bpp),
				header->height * bytes_per_bpp);
		}
	}
	fbcon_flush();
#endif /* DISPLAY_TYPE_MIPI */
}

void fbcon_set_storage_status(void)
{
	char card_state[16] = { 0 };
	struct mmc_device *dev;

	/* ignore anything that happens before fbcon is initialized */
	if (!config)
		return;

	dev = target_mmc_device();
	if (!dev)
		return;

	switch (dev->config.slot) {
		case EMMC_CARD:
			snprintf(card_state, sizeof(card_state), "[%d] eMMC", dev->card.retries);
			dprintf(CRITICAL, "%s: Slot id: %d\n", __func__, dev->config.slot);
			break;
		case SD_CARD:
			snprintf(card_state, sizeof(card_state), "[%d] SD", dev->card.retries);
			dprintf(CRITICAL, "%s: Slot id: %d\n", __func__, dev->config.slot);
			break;
		default:
			dprintf(CRITICAL, "%s: Unknown slot id: %d\n", __func__, dev->config.slot);
			break;
	}
	fbcon_set_bg(BLACK, config->con.max.x - 9, 0, 9, 1);

	int color = GREEN;
	switch (dev->card.retries) {
		case 0:
		case 1:
		case 2:
			dprintf(CRITICAL, "%s: Health good, slot=%d, retries=%d\n", __func__, dev->config.slot, dev->card.retries);
			break;
		case MMC_MAX_COMMAND_RETRY:
			dprintf(CRITICAL, "%s: Health failure, slot=%d, retries=%d\n", __func__, dev->config.slot, dev->card.retries);
			color = RED;
			break;
		default:
			dprintf(CRITICAL, "%s: Health bad, slot=%d, retries=%d\n", __func__, dev->config.slot, dev->card.retries);
			color=YELLOW;
			break;
	}
	fbcon_acprint(card_state, 0, ALIGN_RIGHT, color);
}

void fbcon_print_version(void)
{
	struct raster_font *font = fbcon_get_font_type();
	unsigned prev_fg_color = fbcon_get_font_fg_color();
	fbcon_set_font_type(&font_12x16);
	fbcon_set_font_fg_color(WHITE);
	fbcon_acprintf(config->con.max.y-4, ALIGN_CENTER,  WHITE, "IBL " VERSION " Built at " __DATE__ " " __TIME__ "\n\n");
	fbcon_print("Developed by\n");
	fbcon_print("  S-trace <S-trace@list.ru>\n");
	fbcon_print("  acDev <remittor@gmail.com>");
	fbcon_set_font_fg_color(prev_fg_color);
	fbcon_set_font_type(font);
}

void fbcon_print_init_time(void)
{
	uint64_t it = (device_init_time * 1000) / qtimer_tick_rate();
	unsigned color = GREEN;

	if (it > 2500)
		color = RED;
	else if (it > 2200)
		color = YELLOW;

	fbcon_acprintf(0, ALIGN_RIGHT, color, "INIT: %u          ",
		       (it > 99999) ? 99999 : (uint32_t)it);
	}

char *fbcon_get_color_name(unsigned color)
{
	switch (color) {
		case BLACK:  return "BLACK";
		case GRAY:   return "GRAY";
		case SILVER: return "SILVER";
		case WHITE:  return "WHITE";
		case MAROON: return "MAROON";
		case RED:    return "RED";
		case GREEN:  return "GREEN";
		case LIME:   return "LIME";
		case NAVY:   return "NAVY";
		case BLUE:   return "BLUE";
		case OLIVE:  return "OLIVE";
		case YELLOW: return "YELLOW";
		case PURPLE: return "PURPLE";
		case TEAL:   return "TEAL";
		case AQUA:   return "AQUA";
		case FUCHSIA: return "FUCHSIA";
	}
	return "UNKNOWN";
}

unsigned fbcon_get_color_by_name(const char *name)
{
	unsigned color = WHITE;
	while (name[0] == ' ' || name[0] == '\t') name++; //trim out unnecessary spaces

	     if (!strcmp(name, "WHITE"))   color=WHITE;
	else if (!strcmp(name, "BLACK"))   color=BLACK;
	else if (!strcmp(name, "GRAY"))    color=GRAY;
	else if (!strcmp(name, "SILVER"))  color=SILVER;
	else if (!strcmp(name, "MAROON"))  color=MAROON;
	else if (!strcmp(name, "RED"))     color=RED;
	else if (!strcmp(name, "GREEN"))   color=GREEN;
	else if (!strcmp(name, "LIME"))    color=LIME;
	else if (!strcmp(name, "NAVY"))    color=NAVY;
	else if (!strcmp(name, "BLUE"))    color=BLUE;
	else if (!strcmp(name, "OLIVE"))   color=OLIVE;
	else if (!strcmp(name, "YELLOW"))  color=YELLOW;
	else if (!strcmp(name, "PURPLE"))  color=PURPLE;
	else if (!strcmp(name, "FUCHSIA")) color=FUCHSIA;
	else if (!strcmp(name, "TEAL"))    color=TEAL;
	else if (!strcmp(name, "AQUA"))    color=AQUA;
	else if (!strcmp(name, "BLACK"))   color=WHITE;
	return color;
}
