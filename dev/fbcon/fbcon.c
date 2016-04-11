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
#include <splash.h>
#include <platform.h>
#include <string.h>

static struct fbcon_config * config = NULL;

static void fbcon_drawglyph(uint8_t * pixels, uint32_t paint, unsigned stride, char c)
{
	unsigned cc, gw;
	unsigned i, x, y;
	uint32_t data;
	uint8_t * glyph;

	stride -= config->con.sym_width;
	stride *= config->pixel_size;

	gw = (config->con.font->width / 8) + 1;
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
				if (config->pixel_size == 3) {
					*(pixels + 2) = (uint8_t)(paint >> 16);
				}
			}
			data >>= 1;
			pixels += config->pixel_size;
		}
		if (config->con.font->width < config->con.sym_width) {
			unsigned kx = config->con.sym_width - config->con.font->width;
			for (x = 0; x < kx; x++) {
				*(uint16_t *)pixels = (uint16_t)config->con.bg_color;
				if (config->pixel_size == 3) {
					*(pixels + 2) = (uint8_t)(config->con.bg_color >> 16);
				}
				pixels += config->pixel_size;
			}
		}
		glyph += gw;
		pixels += stride;
	}
	if (config->con.font->height < config->con.sym_height) {
		unsigned ky = config->con.sym_height - config->con.font->height;
		unsigned kx = config->con.sym_width;
		for (y = 0; y < ky; y++) {
			for (x = 0; x < kx; x++) {
				*(uint16_t *)pixels = (uint16_t)config->con.bg_color;
				if (config->pixel_size == 3) {
					*(pixels + 2) = (uint8_t)(config->con.bg_color >> 16);
				}
				pixels += config->pixel_size;
			}
		}
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

	scanline = config->stride * config->pixel_size;
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
	unsigned size = config->stride * config->height * config->pixel_size;
	memset(config->base, 0, size);
}

void fbcon_putc(char c)
{
	uint8_t * pixels = NULL;	
	uint32_t offset;
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

	offset = config->con.cur.y * config->con.sym_height * config->stride;
	offset += config->con.cur.x * config->con.sym_width;
	pixels = config->base + offset * config->pixel_size;
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

void fbcon_print(char * str)
{
	while (*str) {
		fbcon_putc(*str);
		str++;
	}
}

void fbcon_set_cursor_pos(unsigned x, unsigned y)
{
	if (x >= config->con.max.x) {
		config->con.cur.x = config->con.max.x - 1;
	} else {
		config->con.cur.x = x;
	}
	if (y >= config->con.max.y) {
		config->con.cur.y = config->con.max.y - 1;
	} else {
		config->con.cur.y = y;
	}
}

void fbcon_optimize_font_bitmap(struct raster_font * font)
{
	unsigned gw, i, size;
	uint8_t * bitmap;

	if (!font->processed) {
		gw = (font->width / 8) + 1;
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
	config->con.max.x = config->width / width;
	config->con.max.y = (config->height - 1) / height;
}

void fbcon_set_font_fg_color(unsigned color)
{
	config->con.fg_color = color;
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

void fbcon_setup(struct fbcon_config *_config)
{
	ASSERT(_config);
	config = _config;

	switch (config->format) {
	case FB_FORMAT_RGB565:
		config->pixel_size = 2;
		break;
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
	struct fbimage default_fbimg, *fbimg;
	bool flag = true;

	fbcon_clear();
	fbimg = fetch_image_from_partition();

	if(!fbimg) {
		flag = false;
		fbimg = &default_fbimg;
		fbimg->header.width = SPLASH_IMAGE_HEIGHT;
		fbimg->header.height = SPLASH_IMAGE_WIDTH;
#if DISPLAY_TYPE_MIPI
		fbimg->image = (unsigned char *)imageBuffer_rgb888;
#else
		fbimg->image = (unsigned char *)imageBuffer;
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

		image_base = ((((total_y/2) - (height / 2) ) *
				(config->width)) + (total_x/2 - (width / 2)));
		for (i = 0; i < height; i++) {
			memcpy (config->base + ((image_base + (i * (config->width))) * bytes_per_bpp),
				logo_base + (i * pitch * bytes_per_bpp), width * bytes_per_bpp);
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
