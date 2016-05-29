/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Fundation, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __ABOOT_H
#define __ABOOT_H

#ifndef __MMC_H
#include <mmc.h>
#endif // __MMC_H

#ifndef __RECOVERY_H
#include <app/aboot/recovery.h>
#endif // __RECOVERY_H

#ifndef __FASTBOOT_H
#include <app/aboot/fastboot.h>
#endif // __FASTBOOT_H


#ifndef __DEVINFO_H
#include <app/aboot/devinfo.h>
#endif // __DEVINFO_H

#ifndef __PARTITION_PARSER_H
#include <partition_parser.h>
#endif // __PARTITION_PARSER_H


extern bool target_use_signed_kernel(void);
extern void platform_uninit(void);
extern void target_uninit(void);
extern int get_target_boot_params(const char *cmdline, const char *part,
				  char *buf, int buflen);

void write_device_info_mmc(device_info *dev);
void write_device_info_flash(device_info *dev);

#define EXPAND(NAME) #NAME
#define TARGET(NAME) EXPAND(NAME)

#ifdef MEMBASE
#define EMMC_BOOT_IMG_HEADER_ADDR (0xFF000+(MEMBASE))
#else
#define EMMC_BOOT_IMG_HEADER_ADDR 0xFF000
#endif

#ifndef MEMSIZE
#define MEMSIZE 1024*1024
#endif

#define MAX_TAGS_SIZE   1024

#define RECOVERY_HARD_RESET_MODE   0x01
#define FASTBOOT_HARD_RESET_MODE   0x02
#define RTC_HARD_RESET_MODE        0x03

#define RECOVERY_MODE   0x77665502
#define FASTBOOT_MODE   0x77665500
#define ALARM_BOOT      0x77665503

/* make 4096 as default size to ensure EFS,EXT4's erasing */
#define DEFAULT_ERASE_SIZE  4096
#define MAX_PANEL_BUF_SIZE 128

#define UBI_MAGIC      "UBI#"
#define UBI_MAGIC_SIZE 0x04

#define ADD_OF(a, b) (UINT_MAX - b > a) ? (a + b) : UINT_MAX

#if UFS_SUPPORT
static const char *emmc_cmdline = " androidboot.bootdevice=msm_sdcc.1";
static const char *ufs_cmdline = " androidboot.bootdevice=msm_ufs.1";
#else
static const char *emmc_cmdline = " androidboot.emmc=true";
static const char *swap_sdcc_cmdline[] = { "", " androidboot.swap_sdcc=1", " androidboot.swap_sdcc=2" };
#endif
static const char *usb_sn_cmdline = " androidboot.serialno=";
static const char *androidboot_mode = " androidboot.mode=";
static const char *alarmboot_cmdline = " androidboot.alarmboot=true";
static const char *loglevel         = " quiet";
static const char *battchg_pause = " androidboot.mode=charger";
static const char *auth_kernel = " androidboot.authorized_kernel=true";
static const char *secondary_gpt_enable = " gpt";

static const char *baseband_apq     = " androidboot.baseband=apq";
static const char *baseband_msm     = " androidboot.baseband=msm";
static const char *baseband_csfb    = " androidboot.baseband=csfb";
static const char *baseband_svlte2a = " androidboot.baseband=svlte2a";
static const char *baseband_mdm     = " androidboot.baseband=mdm";
static const char *baseband_mdm2    = " androidboot.baseband=mdm2";
static const char *baseband_sglte   = " androidboot.baseband=sglte";
static const char *baseband_dsda    = " androidboot.baseband=dsda";
static const char *baseband_dsda2   = " androidboot.baseband=dsda2";
static const char *baseband_sglte2  = " androidboot.baseband=sglte2";
static const char *warmboot_cmdline = " qpnp-power-on.warm_boot=1";

static const char *permissive_selinux = " androidboot.selinux=permissive";
static const char *llcon_cmdline_fmt = " androidboot.llcon=%d,100,%d,0,24,1280,720,720,%d,0x%06x";

struct atag_ptbl_entry
{
	char name[16];
	unsigned offset;
	unsigned size;
	unsigned flags;
};

/*
 * Partition info, required to be published
 * for fastboot
 */
struct getvar_partition_info {
	const char part_name[MAX_GPT_NAME_SIZE]; /* Partition name */
	char getvar_size[MAX_GET_VAR_NAME_SIZE]; /* fastboot get var name for size */
	char getvar_type[MAX_GET_VAR_NAME_SIZE]; /* fastboot get var name for type */
	char size_response[MAX_RSP_SIZE];        /* fastboot response for size */
	char type_response[MAX_RSP_SIZE];        /* fastboot response for type */
};

extern int emmc_recovery_init(void);

#if NO_KEYPAD_DRIVER
extern int fastboot_trigger(void);
#endif

typedef void entry_func_ptr(unsigned, unsigned, unsigned*);

enum swap_sdcc_mode {
	SDCC_EMMC_SD = 0,
	SDCC_SD_EMMC,
	SDCC_SD_ONLY
};

enum boot_media {
	BOOT_MEDIA_LAST = 0,
	BOOT_MEDIA_EMMC,
	BOOT_MEDIA_SD
};

inline const char * get_boot_media_by_id(unsigned id, const char * unk)
{
	switch (id) {
		case BOOT_MEDIA_LAST: return "Last";
		case BOOT_MEDIA_EMMC: return "eMMC";
		case BOOT_MEDIA_SD:   return "SD";
	}
	return unk;
}

enum llcon_mode {
	LLCON_DISABLED = 0,
	LLCON_SYNC     = 1,
	LLCON_ASYNC    = 2,
};

inline const char * get_llcon_mode_by_id(unsigned id, const char * unk)
{
	switch (id) {
		case LLCON_DISABLED: return "disabled";
		case LLCON_SYNC:     return "sync";
		case LLCON_ASYNC:    return "async";
	}
	return unk;
}

enum llcon_font {
	LLCON_FONT_6x11 = 6,
	LLCON_FONT_8x16 = 8,
	LLCON_FONT_10x18 = 10,
	LLCON_FONT_12x22 = 12,
};

inline const char * get_llcon_font_by_id(unsigned id, const char * unk)
{
	switch (id) {
		case LLCON_FONT_6x11:  return "6x11";
		case LLCON_FONT_8x16:  return "8x16";
		case LLCON_FONT_10x18: return "10x18";
		case LLCON_FONT_12x22: return "12x22";
	}
	return unk;
}

bool boot_into_fastboot_get(void);
void boot_into_fastboot_set(bool flag);

extern uint32_t swap_sdcc;

device_info *get_device_info(void);

void cmd_oem_disable_charging(const char *arg, void *data, unsigned size);
void cmd_oem_enable_charging(const char *arg, void *data, unsigned size);
void cmd_oem_disable_charger_screen(const char *arg, void *data, unsigned size);
void cmd_oem_enable_charger_screen(const char *arg, void *data, unsigned size);
void cmd_oem_disable_isolated_sdcard_boot(const char *arg, void *data, unsigned size);
void cmd_oem_enable_isolated_sdcard_boot(const char *arg, void *data, unsigned size);
void cmd_oem_set_default_boot_media_emmc(const char *arg, void *data, unsigned size);
void cmd_oem_set_default_boot_media_sd(const char *arg, void *data, unsigned size);
void cmd_oem_set_default_boot_media_last(const char *arg, void *data, unsigned size);
void cmd_oem_disable_bootmenu_on_boot(const char *arg, void *data, unsigned size);
void cmd_oem_enable_bootmenu_on_boot(const char *arg, void *data, unsigned size);
void cmd_oem_disable_selinux(const char *arg, void *data, unsigned size);
void cmd_oem_enable_selinux(const char *arg, void *data, unsigned size);

void cmd_oem_disable_llcon(const char *arg, void *data, unsigned size);
void cmd_oem_enable_sync_llcon(const char *arg, void *data, unsigned size);
void cmd_oem_enable_async_llcon(const char *arg, void *data, unsigned size);
void cmd_oem_set_llcon_mode(const char *arg, void *data, unsigned size);

void cmd_oem_enable_llcon_wrap(const char *arg, void *data, unsigned size);
void cmd_oem_disable_llcon_wrap(const char *arg, void *data, unsigned size);

void cmd_oem_set_llcon_font_size(const char *arg, void *data, unsigned size);
void cmd_oem_set_llcon_font_color(const char *arg, void *data, unsigned size);

void set_last_boot_media(int media);

void write_device_info(device_info *dev);

void cmd_oem_enable_backlight_control(const char *arg, void *data, unsigned size);
void cmd_oem_disable_backlight_control(const char *arg, void *data, unsigned size);
void cmd_oem_set_min_backlight(const char *arg, void *data, unsigned size);
void cmd_oem_set_max_backlight(const char *arg, void *data, unsigned size);

#define DEFAULT_MIN_BACKLIGHT 3
#define DEFAULT_MAX_BACKLIGHT 255
int is_backlight_control_enabled(void);
int get_min_backlight(void);
int get_max_backlight(void);

#endif // __ABOOT_H
