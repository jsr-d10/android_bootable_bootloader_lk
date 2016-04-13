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

#ifndef __DEV_FBCON_H
#define __DEV_FBCON_H

#include <stdint.h>
#define LOGO_IMG_OFFSET (12*1024*1024)
#define LOGO_IMG_MAGIC "SPLASH!!"
#define LOGO_IMG_MAGIC_SIZE sizeof(LOGO_IMG_MAGIC) - 1


struct logo_img_header {
    unsigned char magic[LOGO_IMG_MAGIC_SIZE]; // "SPLASH!!"
    uint32_t width; // logo's width, little endian
    uint32_t height; // logo's height, little endian
    uint32_t offset;
    unsigned char reserved[512-20];
};

struct fbimage {
	struct logo_img_header  header;
	void *image;
};

struct raster_font {
  unsigned width;
  unsigned height;
  const char * name;
  unsigned char first_char;
  unsigned char last_char;
  unsigned char processed;
  unsigned char * bitmap;
};

struct fb_pos {
  int x;
  int y;
};

struct fb_console {
  struct raster_font * font;
  unsigned sym_width;
  unsigned sym_height;
  unsigned fg_color;
  unsigned bg_color;
  struct fb_pos cur;
  struct fb_pos max;
};

#define COLOR_BLACK      0x000000
#define COLOR_WHITE      0xffffff

#define FB_FORMAT_RGB565 0
#define FB_FORMAT_RGB666 1
#define FB_FORMAT_RGB666_LOOSE 2
#define FB_FORMAT_RGB888 3

struct fbcon_config {
	void		* base;
	unsigned	width;
	unsigned	height;
	unsigned	stride;
	unsigned	bpp;
	unsigned	format;
	unsigned	pixel_size;
	struct fb_console con;

	void		(*update_start)(void);
	int		(*update_done)(void);
};

void fbcon_setup(struct fbcon_config * cfg);
void fbcon_putc(char c);
void fbcon_clear(void);
struct fbcon_config * fbcon_display(void);

void fbcon_set_cursor_pos(unsigned x, unsigned y);
void fbcon_set_font_size(unsigned width, unsigned height);
unsigned fbcon_get_font_fg_color(void);
unsigned fbcon_get_font_bg_color(void);
void fbcon_set_font_fg_color(unsigned color);
void fbcon_set_font_bg_color(unsigned color);
void fbcon_set_font_color(unsigned fg, unsigned bg);
void fbcon_set_font_type(struct raster_font * font);
void fbcon_print(char * str);

enum colors {
	BLACK   = 0x000000,
	GRAY    = 0x808080,
	SILVER  = 0xC0C0C0,
	WHITE   = 0xFFFFFF,
	MAROON  = 0x800000,
	RED     = 0xFF0000,
	GREEN   = 0x008000,
	LIME    = 0x00FF00,
	NAVY    = 0x000080,
	BLUE    = 0x0000FF,
	OLIVE   = 0x808000,
	YELLOW  = 0xFFFF00,
	PURPLE  = 0x800080,
	FUCHSIA = 0xFF00FF,
	TEAL    = 0x008080,
	AQUA    = 0x00FFFF,
};

#define DISPLAY_MAX_X_POS 28
#define DISPLAY_MAX_Y_POS 22
#define DISPLAY_HEADER_Y_POS 13


#endif /* __DEV_FBCON_H */
