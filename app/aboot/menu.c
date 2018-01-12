#include <sys/types.h>
#include <dev/keys.h>
#include <dev/fbcon.h>
#include <debug.h>
#include <target.h>
#include <dload_util.h>
#include "board.h"
#include <app/aboot/aboot.h>
#include <app/aboot/menu.h>
#include <app/aboot/advanced.h>
#include <sdhci_msm.h>
#include <qtimer.h>
#include <ctype.h>

int sdcard_is_bootable = false;
int autoboot = true;

static int cached_bl_min = DEFAULT_MIN_BACKLIGHT;
static int cached_bl_max = DEFAULT_MAX_BACKLIGHT;

static struct menu *boot_menu(void) {
	struct menu *menu = NULL;
	unsigned header_line = fbcon_get_header_line();
	menu = create_menu ("Boot menu", NAVY, 0x10);
	int C = 2; //lines Counter
	if (emmc_health != EMMC_FAILURE) {
		add_menu_item(menu, 1, header_line + C++, GREEN,  "BOOT from eMMC",     EMMC_BOOT);
		add_menu_item(menu, 1, header_line + C++, BLUE,   "RECOVERY from eMMC", EMMC_RECOVERY);
		add_menu_item(menu, 1, header_line + C++, YELLOW, "FASTBOOT from eMMC", EMMC_FASTBOOT);
		}
	if (sdcard_is_bootable) {
		add_menu_item(menu, 1, header_line + C++, TEAL,   "BOOT from SD",     SD_BOOT);
		add_menu_item(menu, 1, header_line + C++, AQUA,   "RECOVERY from SD", SD_RECOVERY);
		add_menu_item(menu, 1, header_line + C++, OLIVE,  "FASTBOOT from SD", SD_FASTBOOT);
	}
	add_menu_item(menu, 1, header_line + C++, PURPLE, "REBOOT MENU =>",  REBOOT_MENU);
	add_menu_item(menu, 1, header_line + C++, SILVER, "OPTIONS MENU =>", OPTIONS_MENU);
	add_menu_item(menu, 1, header_line + C++, RED,    "ADVANCED MENU =>", ADVANCED_MENU);

	if(multiboot_is_available()) {
		char multiboot_slot[MAX_ITEM_LEN];
		const char *format_str = "MULTIBOOT SLOT:      %2d/%d";
		if (multiboot_get_slots_count() > 9) {
			format_str = "MULTIBOOT SLOT:     %2d/%2d";
		}
		snprintf(multiboot_slot, MAX_ITEM_LEN, format_str,
		         multiboot_get_active_slot(), multiboot_get_slots_count());
		add_menu_item(menu, 1, header_line + C++, MAROON, multiboot_slot, MULTIBOOT_SLOT_SELECT);
	}
	return menu;
}

static struct menu *reboot_menu(void) {
	struct menu *menu = NULL;
	unsigned header_line = fbcon_get_header_line();
	menu = create_menu ("Reboot menu", PURPLE, 0x10);
	add_menu_item(menu, 1, header_line + 2, NAVY,    "BACK TO BOOT MENU =>", BOOT_MENU);
	add_menu_item(menu, 1, header_line + 3, RED,     "REBOOT",          REBOOT);
	add_menu_item(menu, 1, header_line + 4, YELLOW,  "FASTBOOT REBOOT", FASTBOOT_REBOOT);
	add_menu_item(menu, 1, header_line + 5, FUCHSIA, "SHUTDOWN",        SHUTDOWN);
	add_menu_item(menu, 1, header_line + 6, SILVER,  "NORMAL    DLOAD (9006)", DLOAD_NORMAL);
	add_menu_item(menu, 1, header_line + 7, GRAY,    "EMERGENCY DLOAD (9008)", DLOAD_EMERGENCY);
	return menu;
}

static struct menu *options_menu(void) {
	device_info *device = get_device_info();
	unsigned header_line = fbcon_get_header_line();
	struct menu *menu = NULL;
	int C = 2; //lines Counter
	menu = create_menu ("Options menu", SILVER, 0x10);

	char charger_screen[MAX_ITEM_LEN];
	char charging_in_bootloader[MAX_ITEM_LEN];
	char isolated_sdcard_boot[MAX_ITEM_LEN];
	char default_boot_media[MAX_ITEM_LEN];
	char bootmenu_on_boot[MAX_ITEM_LEN];
	char permissive_selinux[MAX_ITEM_LEN];
	char llcon[MAX_ITEM_LEN];

	snprintf(charger_screen, MAX_ITEM_LEN, "CHARGER SCREEN:         %s", device->charger_screen_enabled ? "Y" : "N");
	snprintf(charging_in_bootloader, MAX_ITEM_LEN, "CHARGING IN BOOTLOADER: %s", device->charging_enabled ? "Y" : "N");
	snprintf(isolated_sdcard_boot, MAX_ITEM_LEN, "ISOLATED SDCARD BOOT:   %s", device->isolated_sdcard ? "Y" : "N");
	snprintf(bootmenu_on_boot, MAX_ITEM_LEN, "BOOTMENU ON BOOT:       %s", device->bootmenu_on_boot ? "Y" : "N");
	snprintf(permissive_selinux, MAX_ITEM_LEN, "PERMISSIVE SELinux:     %s", device->permissive_selinux ? "Y" : "N");

	add_menu_item(menu, 1, header_line + C++, NAVY,    "BACK TO BOOT MENU =>",   BOOT_MENU);
	add_menu_item(menu, 1, header_line + C++, RED,     charger_screen,         CHARGER_SCREEN_TOGGLE);
	add_menu_item(menu, 1, header_line + C++, YELLOW,  charging_in_bootloader, CHARGING_TOGGLE);
	add_menu_item(menu, 1, header_line + C++, FUCHSIA, isolated_sdcard_boot,   ISOLATED_SDCARD_TOGGLE);

	if (emmc_health != EMMC_FAILURE) {
		char *boot_media = NULL;
		switch (device->default_boot_media) {
			case BOOT_MEDIA_LAST:
				boot_media = "Last";
				break;
			case BOOT_MEDIA_EMMC:
				boot_media = "eMMC";
				break;
			case BOOT_MEDIA_SD:
			default:
				boot_media = "  SD";
				break;
		}
		snprintf(default_boot_media, MAX_ITEM_LEN, "DEFAULT BOOT MEDIA:  %s", boot_media);
		add_menu_item(menu, 1, header_line + C++, SILVER,  default_boot_media,     DEFAULT_BOOT_MEDIA_TOGGLE);
	}
	add_menu_item(menu, 1, header_line + C++, OLIVE, bootmenu_on_boot, BOOTMENU_ON_BOOT_TOGGLE);
	add_menu_item(menu, 1, header_line + C++, PURPLE, permissive_selinux, PERMISSIVE_SELINUX_TOGGLE);
	add_menu_item(menu, 1, header_line + C++, GREEN, "LLCON OPTIONS =>", LLCON_OPTIONS_MENU);
	add_menu_item(menu, 1, header_line + C++, BLUE,  "BACKLIGHT CONTROL =>", BL_CONTROL_OPTIONS_MENU);
	return menu;
}

static struct menu *llcon_options_menu(void) {
	device_info *device = get_device_info();
	unsigned header_line = fbcon_get_header_line();
	struct menu *menu = NULL;
	int C = 2; //lines Counter
	size_t offset;

	menu = create_menu ("LLCON Options", GREEN, 0x10);

	char llcon_mode[MAX_ITEM_LEN]    = "LLCON MODE:              ";
	char llcon_wrap[MAX_ITEM_LEN]    = "LLCON TEXT WRAP:         ";
	char llcon_font[MAX_ITEM_LEN]    = "LLCON FONT SIZE:         ";
	char llcon_color[MAX_ITEM_LEN]   = "LLCON FONT COLOR:        ";
	char bootanimation[MAX_ITEM_LEN];

	const char * llcon_mode_name = get_llcon_mode_by_id(device->llcon_mode, "unk");
	offset = strlen(llcon_mode) - strlen(llcon_mode_name);
	strcpy(llcon_mode + offset, llcon_mode_name);
	strtoupper(llcon_mode);

	const char * llcon_wrap_name = device->llcon_wrap ? "Y" : "N";
	offset = strlen(llcon_wrap) - strlen(llcon_wrap_name);
	strcpy(llcon_wrap + offset, llcon_wrap_name);
	strtoupper(llcon_wrap);

	const char * llcon_font_name = get_llcon_font_by_id(device->llcon_font, "unk");
	offset = strlen(llcon_font) - strlen(llcon_font_name);
	strcpy(llcon_font + offset, llcon_font_name);
	strtoupper(llcon_font);

	const char * llcon_color_name = fbcon_get_color_name(device->llcon_color, "unk");
	offset = strlen(llcon_color) - strlen(llcon_color_name);
	strcpy(llcon_color + offset, llcon_color_name);
	strtoupper(llcon_color);

	add_menu_item(menu, 1, header_line + C++, SILVER, "BACK TO OPTIONS MENU =>", OPTIONS_MENU);
	add_menu_item(menu, 1, header_line + C++, LIME, llcon_mode, LLCON_TOGGLE);
	if (device->llcon_mode != LLCON_DISABLED) {
		add_menu_item(menu, 1, header_line + C++, GREEN, llcon_wrap, LLCON_WRAP_TOGGLE);
		add_menu_item(menu, 1, header_line + C++, WHITE, llcon_font, LLCON_FONT_TOGGLE);
		if (device->llcon_color == BLACK) {
			cmd_oem_set_llcon_font_color(fbcon_get_color_name(WHITE, ""), NULL, 0);
		}
		add_menu_item(menu, 1, header_line + C, device->llcon_color, llcon_color, LLCON_COLOR_TOGGLE);
// 		add_menu_item(menu, 1, header_line + C++, GREEN, bootanimation, BOOTANIMATION_TOGGLE);
	}

	return menu;
}

static struct menu *bl_control_options_menu(void) {
	device_info *device = get_device_info();
	unsigned header_line = fbcon_get_header_line();
	struct menu *menu = NULL;
	int C = 2; //lines Counter
	size_t offset;

	menu = create_menu ("Backlight Options", BLUE, 0x10);

	char bl_control[MAX_ITEM_LEN] = {0};
	char bl_min[MAX_ITEM_LEN] = {0};
	char bl_max[MAX_ITEM_LEN] = {0};
	snprintf(bl_control, MAX_ITEM_LEN, "Backlight control:      %s", device->backlight_control ? "Y" : "N");
	snprintf(bl_min, MAX_ITEM_LEN,     "Minimal backlight:    %03d", cached_bl_min);
	snprintf(bl_max, MAX_ITEM_LEN,     "Maximal backlight:    %03d", cached_bl_max);

	add_menu_item(menu, 1, header_line + C++, SILVER, "BACK TO OPTIONS MENU =>", OPTIONS_MENU);
	add_menu_item(menu, 1, header_line + C++, RED,   bl_control, BL_CONTROL_TOGGLE);
	if (device->backlight_control) {
		add_menu_item(menu, 1, header_line + C++, GREEN, bl_min, BL_MIN_ADJUST);
		add_menu_item(menu, 1, header_line + C++, LIME,  bl_max, BL_MAX_ADJUST);
	}

	return menu;
}
static struct menu *advanced_menu(void) {
	struct menu *menu = NULL;
	int C = 2; //lines Counter
	menu = create_menu ("Advanced menu", RED, 0x10);

	unsigned header_line = fbcon_get_header_line();
	add_menu_item(menu, 1, header_line + C++, NAVY,    "BACK TO BOOT MENU =>", BOOT_MENU);

	if (emmc_health != EMMC_FAILURE) {
		add_menu_item(menu, 1, header_line + C++, GREEN,  "FAST READ TEST (eMMC)", EMMC_READ_SPEED_TEST);
		add_menu_item(menu, 1, header_line + C++, LIME,   "FULL READ TEST (eMMC)", FULL_EMMC_READ_SPEED_TEST);
	}
	if (sdcard_is_bootable) {
		add_menu_item(menu, 1, header_line + C++, OLIVE,  "FAST READ TEST (SD)",   SD_READ_SPEED_TEST);
		add_menu_item(menu, 1, header_line + C++, YELLOW, "FULL READ TEST (SD)",   FULL_SD_READ_SPEED_TEST);
	}
	return menu;
}

static struct menu *
create_menu(
	char *menu_name,
	uint32_t fg_color,
	uint32_t cursor)
{
	struct menu *menu = (struct menu*) malloc(sizeof(struct menu));
	if (!menu) {
		dprintf(CRITICAL, "%s: Malloc failed!", __func__);
		return(NULL);
	}
	strncpy(menu->name, menu_name, sizeof(menu->name));
	menu->fg_color = fg_color;
	menu->cursor = cursor;
	menu->item = NULL;
	return menu;
}

static void destroy_menu(struct menu *menu) {
	struct menu_item *current_item = menu->item;
	struct menu_item *next = current_item->next;
	do {
		next = current_item->next;
		fbcon_set_font_fg_color(BLACK);
		fbcon_set_cursor_pos(current_item->x_pos, current_item->y_pos);
		fbcon_putc(menu->cursor);
		fbcon_set_cursor_pos(current_item->x_pos, current_item->y_pos);
		fbcon_print(current_item->name);
		free(current_item);
		if (next == current_item) // If only one item was in that menu. Silly, but possible.
			break;
		current_item = next;
	} while (current_item != menu->item && current_item->id != 0);

	fbcon_hprint(menu->name, BLACK);
	free(menu);
}

static struct menu_item *add_menu_item(
	struct menu *menu,
	uint32_t x_pos,
	uint32_t y_pos,
	uint32_t fg_color,
	char *name,
	enum item_types type)
{
	struct menu_item *first_item=menu->item;
	struct menu_item *new_item = (struct menu_item*) malloc(sizeof(struct menu_item));
	if (!new_item) {
		dprintf(CRITICAL, "%s: Malloc failed!\n", __func__);
		return NULL;
	}

	new_item->x_pos = x_pos;
	new_item->y_pos = y_pos;
	new_item->fg_color = fg_color;
	new_item->type = type;
	snprintf(new_item->name, sizeof(new_item->name), "  %s", name); // add 2 spaces before name - one for cursor and one for separator
	if (first_item != NULL) {
		new_item->id = first_item->previous->id + 1;
		first_item->previous->next = new_item;
		new_item->next = first_item;
		new_item->previous = first_item->previous;
		first_item->previous = new_item;
		return first_item;
	} else {
		new_item->id = 0;
		new_item->previous = new_item;
		new_item->next = new_item;
		menu->item = new_item;
		return new_item;
	}

}

static void show_menu(struct menu *menu) {
	struct menu_item *item = menu->item;
	int iter=0;

	fbcon_hprint(menu->name, menu->fg_color);

	while (1) {
		dprintf(SPEW, "%s: iter %d, name %s, id %d, cur=%p, next=%p\n",
			__func__, iter++, item->name, item->id, item, item->next);
		fbcon_set_cursor_pos(item->x_pos, item->y_pos);
		fbcon_set_font_fg_color(item->fg_color);
		fbcon_print(item->name);
		if ((item->next->id < item->id) || (item->next == item))
			break;
		item=item->next;
	}
}

static void move_cursor(struct menu_item *old, struct menu_item *new, uint32_t color, uint32_t cursor) {
	fbcon_set_cursor_pos(old->x_pos, old->y_pos);
	fbcon_set_font_fg_color(BLACK);
	fbcon_putc(cursor);
	fbcon_set_cursor_pos(new->x_pos, new->y_pos);
	fbcon_set_font_fg_color(color);
	fbcon_putc(cursor);
}

struct menu_item *get_item_by_type(struct menu *menu, int type)
{
	bool item_found = false;
	struct menu_item *selected = menu->item;
	while (true) {
	if (selected->type == type) {
		item_found = true;
		break;
	} else {
		selected = selected->next;
	}

	if (selected == menu->item) // If we passed full loop through menu items, but item with desired type was not found
		break;
	}

	if (!item_found)
		selected = menu->item;

	return selected;
}

static uint32_t process_menu(struct menu *menu, int default_selection) {
	struct menu_item *selected = get_item_by_type(menu, default_selection);
	bool power_released = false;

	move_cursor(selected, selected, LIME, menu->cursor);
	device_info *device = get_device_info();

	int timeout = 0;
	if (keys_get_state(KEY_FUNCTION))
		timeout = 30 * 1000; // 30 seconds
	else // Bootmenu on boot
		timeout = 5 * 1000; // 5 seconds

	if (autoboot) {
		fbcon_acprintf(2, ALIGN_LEFT, BLUE, "  Autoboot in %2d.%d seconds\n", timeout/1000, (timeout%1000)/10 );
		fbcon_set_font_fg_color(RED);
	}
	while (timeout > 0) {
		uint64_t t0, t1;
		t0 = qtimer_get_phy_timer_cnt();
		if (autoboot) {
			if (timeout%100 == 0) {
				fbcon_set_bg(BLACK, strlen("  Autoboot in "), 2, strlen("30.0"), 1);
				fbcon_set_cursor_pos(strlen("  Autoboot in "), 2);
				fbcon_printf("%2d.%1d", timeout/1000, (timeout%1000)/100 );
			}
		}
		target_keystatus();
		if (!keys_get_state(KEY_POWER)) {
			power_released = true;
		}
		if (keys_get_state(KEY_VOLUMEUP)) {
			wait_vib_timeout();
			vib_timed_turn_on(100);
			move_cursor(selected, selected->previous, LIME, menu->cursor);
			selected=selected->previous;
			thread_sleep(300);
			if (autoboot) {
				fbcon_set_bg(BLACK, 0, 2, -1, 1);
				autoboot = false;
			}
			continue;
		}
		if (keys_get_state(KEY_VOLUMEDOWN)) {
			wait_vib_timeout();
			vib_timed_turn_on(100);
			move_cursor(selected, selected->next, LIME, menu->cursor);
			selected=selected->next;
			thread_sleep(300);
			if (autoboot) {
				fbcon_set_bg(BLACK, 0, 2, -1, 1);
				autoboot = false;
			}
			continue;
		}
		if (keys_get_state(KEY_POWER) && power_released) {
			wait_vib_timeout();
			vib_timed_turn_on(400);
			move_cursor(selected, selected, RED, menu->cursor);
			if (autoboot) {
				fbcon_set_bg(BLACK, 0, 2, -1, 1);
				autoboot = false;
			}
			break;
		}

		uint64_t ticks;
		t1 = qtimer_get_phy_timer_cnt();

		if (t0 > t1)
			ticks = (QTMR_PHY_CNT_MAX_VALUE - t0) + t1;
		else
			ticks = t1 - t0;

		uint64_t time_ms = ticks * 1000 / qtimer_tick_rate();
		if (time_ms < KEY_SCAN_FREQ)
			thread_sleep(KEY_SCAN_FREQ - time_ms);

		if (autoboot) {
			timeout -= KEY_SCAN_FREQ;
		}
	}
	if (autoboot)
		fbcon_set_bg(BLACK, 0, 2, -1, 1);
	return selected->type;
}

static void adjust_backlight_limits(struct menu_item *current_item, int selection)
{
	if (selection == BL_MIN_ADJUST)
		fbcon_hprint("Minimal backlight", GREEN);
	else if (selection == BL_MAX_ADJUST)
		fbcon_hprint("Maximal backlight", LIME);

	while (true) {
		wait_vib_timeout();
		target_keystatus();

		if (keys_get_state(KEY_POWER)) {
			break;
		}

		if (keys_get_state(KEY_VOLUMEUP)) {
			if (selection == BL_MIN_ADJUST)
				cached_bl_min += 1;
			else if (selection == BL_MAX_ADJUST)
				cached_bl_max += 1;
		}

		if (keys_get_state(KEY_VOLUMEDOWN)) {
			if (selection == BL_MIN_ADJUST)
				cached_bl_min -= 1;
			else if (selection == BL_MAX_ADJUST)
				cached_bl_max -= 1;
		}

		cached_bl_min = cached_bl_min < 1 ? 1 : cached_bl_min;
		cached_bl_max = cached_bl_max < 1 ? 1 : cached_bl_max;
		cached_bl_min = cached_bl_min > 255 ? 255 : cached_bl_min;
		cached_bl_max = cached_bl_max > 255 ? 255 : cached_bl_max;

		if (! (keys_get_state(KEY_VOLUMEDOWN) || keys_get_state(KEY_VOLUMEUP)) )
			continue;

		fbcon_set_cursor_pos(current_item->x_pos, current_item->y_pos);
		fbcon_set_font_fg_color(BLACK);
		fbcon_print(current_item->name);
		fbcon_set_cursor_pos(current_item->x_pos, current_item->y_pos);
		fbcon_set_font_fg_color(current_item->fg_color);

		//+2 is for 2 spaces before actual item name
		if (selection == BL_MIN_ADJUST)
			snprintf(current_item->name+2, MAX_ITEM_LEN, "Minimal backlight:    %03d", cached_bl_min);
		else if (selection == BL_MAX_ADJUST)
			snprintf(current_item->name+2, MAX_ITEM_LEN, "Maximal backlight:    %03d", cached_bl_max);

		fbcon_print(current_item->name);
		thread_sleep(100);
	}
	if (selection == BL_MIN_ADJUST)
		cmd_oem_set_min_backlight((const char *)cached_bl_min, NULL, 0);
	else if (selection == BL_MAX_ADJUST)
		cmd_oem_set_max_backlight((const char *)cached_bl_max, NULL, 0);
}

static void multiboot_slot_set_menu(struct menu_item *current_item, int selection)
{
	fbcon_hprint("Multiboot slot", GREEN);
	int multiboot_slots = multiboot_get_slots_count();
	int cached_multiboot_slot = multiboot_get_active_slot();
	while (true) {
		wait_vib_timeout();
		target_keystatus();

		if (keys_get_state(KEY_POWER)) {
			break;
		}

		if (keys_get_state(KEY_VOLUMEUP)) {
			++cached_multiboot_slot;
		}

		if (keys_get_state(KEY_VOLUMEDOWN)) {
			--cached_multiboot_slot;
		}

		cached_multiboot_slot = cached_multiboot_slot < 1 ? multiboot_slots : cached_multiboot_slot;
		cached_multiboot_slot = cached_multiboot_slot > multiboot_slots ? 1 : cached_multiboot_slot;

		if (! (keys_get_state(KEY_VOLUMEDOWN) || keys_get_state(KEY_VOLUMEUP)) )
			continue;

		fbcon_set_cursor_pos(current_item->x_pos, current_item->y_pos);
		fbcon_set_font_fg_color(BLACK);
		fbcon_print(current_item->name);
		fbcon_set_cursor_pos(current_item->x_pos, current_item->y_pos);
		fbcon_set_font_fg_color(current_item->fg_color);

		const char *format_str = "MULTIBOOT SLOT:      %2d/%d";
		if (multiboot_get_slots_count() > 9) {
			format_str = "MULTIBOOT SLOT:     %2d/%2d";
		}
		//+2 is for 2 spaces before actual item name
		snprintf(current_item->name+2, MAX_ITEM_LEN, format_str, cached_multiboot_slot, multiboot_get_slots_count());

		fbcon_print(current_item->name);
		thread_sleep(300);
	}
	multiboot_set_active_slot(cached_multiboot_slot);
}

static void handle_menu_selection(uint32_t selection, struct menu *menu) {
	struct mmc_device *dev;
	int x;
	struct menu_item *current_item = get_item_by_type(menu, selection);

	device_info *device = get_device_info();
	dprintf(SPEW, "%s: processing selection=%d\n", __func__, selection);
	dev = target_mmc_device();

	switch (selection) {
		case EMMC_BOOT:
		case EMMC_RECOVERY:
		case EMMC_FASTBOOT:
			set_last_boot_media(BOOT_MEDIA_EMMC);
			if (dev->config.slot != EMMC_CARD)
				target_sdc_init_slot(EMMC_CARD);
			swap_sdcc = SDCC_EMMC_SD;
			break;

		case SD_BOOT:
		case SD_RECOVERY:
		case SD_FASTBOOT:
			if (dev->config.slot != SD_CARD)
				target_sdc_init_slot(SD_CARD);
			swap_sdcc = device->isolated_sdcard ? SDCC_SD_ONLY : SDCC_SD_EMMC;
			set_last_boot_media(BOOT_MEDIA_SD);
			break;
	}

	boot_into_fastboot_set(false);
	boot_into_recovery = 0;

	switch (selection) {
		case EMMC_BOOT:
		case SD_BOOT:
			// nothing
			break;

		case EMMC_RECOVERY:
		case SD_RECOVERY:
			boot_into_recovery = 1;
			break;

		case EMMC_FASTBOOT:
		case SD_FASTBOOT:
			boot_into_fastboot_set(true);
			break;

		case REBOOT_MENU:
			destroy_menu(menu);
			draw_menu(reboot_menu, DEFAULT_ITEM);
			break;

		case BOOT_MENU:
			destroy_menu(menu);
			draw_menu(boot_menu, DEFAULT_ITEM);
			break;

		case DLOAD_NORMAL:
			platform_halt();
			break;

		case DLOAD_EMERGENCY:
			set_download_mode(EMERGENCY_DLOAD);
			reboot_device(DLOAD);
			break;

		case FASTBOOT_REBOOT:
			reboot_device(FASTBOOT_MODE);
			break;

		case REBOOT:
			reboot_device(0);
			break;

		case SHUTDOWN:
			shutdown_device();
			break;

		case OPTIONS_MENU:
			destroy_menu(menu);
			draw_menu(options_menu, DEFAULT_ITEM);
			break;

		case ADVANCED_MENU:
			destroy_menu(menu);
			draw_menu(advanced_menu, DEFAULT_ITEM);
			break;

		case CHARGER_SCREEN_TOGGLE:
			if (device->charger_screen_enabled)
				cmd_oem_disable_charger_screen(NULL, NULL, 0);
			else
				cmd_oem_enable_charger_screen(NULL, NULL, 0);
			break;

		case CHARGING_TOGGLE:
			if (device->charging_enabled)
				cmd_oem_disable_charging(NULL, NULL, 0);
			else
				cmd_oem_enable_charging(NULL, NULL, 0);
			break;

		case ISOLATED_SDCARD_TOGGLE:
			if (device->isolated_sdcard)
				cmd_oem_disable_isolated_sdcard_boot(NULL, NULL, 0);
			else
				cmd_oem_enable_isolated_sdcard_boot(NULL, NULL, 0);
			break;

		case DEFAULT_BOOT_MEDIA_TOGGLE:
			switch (device->default_boot_media) {
				case BOOT_MEDIA_LAST:
					cmd_oem_set_default_boot_media_emmc(NULL, NULL, 0);
					break;
				case BOOT_MEDIA_EMMC:
					cmd_oem_set_default_boot_media_sd(NULL, NULL, 0);
					break;
				case BOOT_MEDIA_SD:
					cmd_oem_set_default_boot_media_last(NULL, NULL, 0);
					break;
				default:
					cmd_oem_set_default_boot_media_sd(NULL, NULL, 0);
					break;
			}
			break;

		case BOOTMENU_ON_BOOT_TOGGLE:
			if (device->bootmenu_on_boot)
				cmd_oem_disable_bootmenu_on_boot(NULL, NULL, 0);
			else
				cmd_oem_enable_bootmenu_on_boot(NULL, NULL, 0);
			break;

		case PERMISSIVE_SELINUX_TOGGLE:
			if (device->permissive_selinux)
				cmd_oem_disable_permissive_selinux(NULL, NULL, 0);
			else
				cmd_oem_enable_permissive_selinux(NULL, NULL, 0);
			break;

		case LLCON_OPTIONS_MENU:
			destroy_menu(menu);
			draw_menu(llcon_options_menu, DEFAULT_ITEM);
			break;

		case LLCON_TOGGLE:
			x = device->llcon_mode;
			while (1) {
				x = (x > 10) ? 0 : x + 1;
				if (get_llcon_mode_by_id(x, NULL))
					break;
			}				
			cmd_oem_set_llcon_mode((const char *)x, NULL, 0);
			break;

		case LLCON_WRAP_TOGGLE:
			if (device->llcon_wrap)
				cmd_oem_disable_llcon_wrap(NULL, NULL, 0);
			else
				cmd_oem_enable_llcon_wrap(NULL, NULL, 0);
			break;

		case LLCON_FONT_TOGGLE:
			x = device->llcon_font;
			while (1) {
				x = (x > 32) ? 0 : x + 1;
				if (get_llcon_font_by_id(x, NULL))
					break;
			}
			cmd_oem_set_llcon_font_size(get_llcon_font_by_id(x, ""), NULL, 0);
			break;

		case LLCON_COLOR_TOGGLE:
			switch (device->llcon_color) {
				case GRAY:   x = SILVER; break;
				case SILVER: x = WHITE; break;
				case WHITE:  x = MAROON; break;
				case MAROON: x = RED; break;
				case RED:    x = GREEN; break;
				case GREEN:  x = LIME; break;
				case LIME:   x = NAVY; break;
				case NAVY:   x = BLUE; break;
				case BLUE:   x = OLIVE; break;
				case OLIVE:  x = YELLOW; break;
				case YELLOW: x = PURPLE; break;
				case PURPLE: x = TEAL; break;
				case TEAL:   x = AQUA; break;
				case AQUA:   x = FUCHSIA; break;
				case FUCHSIA: x = GRAY; break;
				default: x = WHITE; break;
			}
			cmd_oem_set_llcon_font_color(fbcon_get_color_name(x, ""), NULL, 0);
			break;

		case BL_CONTROL_OPTIONS_MENU:
			destroy_menu(menu);
			draw_menu(bl_control_options_menu, DEFAULT_ITEM);
			break;

		case BL_CONTROL_TOGGLE:
			if (device->backlight_control)
				cmd_oem_disable_backlight_control(NULL, NULL, 0);
			else {
				cmd_oem_enable_backlight_control(NULL, NULL, 0);
				if (cached_bl_min == 0) {
					cached_bl_min = DEFAULT_MIN_BACKLIGHT;
					cmd_oem_set_min_backlight((const char *)cached_bl_min, NULL, 0);
				}
				if (cached_bl_max == 0) {
					cached_bl_max = DEFAULT_MAX_BACKLIGHT;
					cmd_oem_set_max_backlight((const char *)cached_bl_max, NULL, 0);
				}
			}
			break;

		case BL_MIN_ADJUST:
		case BL_MAX_ADJUST:
			adjust_backlight_limits(current_item, selection);
			break;

		case EMMC_READ_SPEED_TEST:
			test_storage_read_speed(EMMC_CARD, false);
			break;
			
		case SD_READ_SPEED_TEST:
			test_storage_read_speed(SD_CARD, false);
			break;

		case FULL_EMMC_READ_SPEED_TEST:
			test_storage_read_speed(EMMC_CARD, true);
			break;

		case FULL_SD_READ_SPEED_TEST:
			test_storage_read_speed(SD_CARD, true);
			break;

		case MULTIBOOT_SLOT_SELECT:
			multiboot_slot_set_menu(current_item, selection);
			break;

		default:
			break;
	}

	switch (selection) {
		case CHARGER_SCREEN_TOGGLE:
		case CHARGING_TOGGLE:
		case ISOLATED_SDCARD_TOGGLE:
		case DEFAULT_BOOT_MEDIA_TOGGLE:
		case BOOTMENU_ON_BOOT_TOGGLE:
		case PERMISSIVE_SELINUX_TOGGLE:
			destroy_menu(menu);
			draw_menu(options_menu, selection);
			break;

		case EMMC_READ_SPEED_TEST:
		case SD_READ_SPEED_TEST:
		case FULL_EMMC_READ_SPEED_TEST:
		case FULL_SD_READ_SPEED_TEST:
			destroy_menu(menu);
			draw_menu(advanced_menu, selection);
			break;

		case LLCON_TOGGLE:
		case LLCON_WRAP_TOGGLE:
		case LLCON_FONT_TOGGLE:
		case LLCON_COLOR_TOGGLE:
			destroy_menu(menu);
			draw_menu(llcon_options_menu, selection);
			break;

		case BL_CONTROL_TOGGLE:
		case BL_MIN_ADJUST:
		case BL_MAX_ADJUST:
			destroy_menu(menu);
			draw_menu(bl_control_options_menu, selection);
			break;

		case MULTIBOOT_SLOT_SELECT:
			destroy_menu(menu);
			draw_menu(boot_menu, selection);
			break;

		default:
			break;
	}
}

void draw_menu(struct menu *menu_function(void), int default_selection) {
	struct menu *menu = menu_function();
	show_menu(menu);
	uint32_t selection = process_menu(menu, default_selection);

	// Hide menu title after selection
	fbcon_hprint("", BLACK);

	handle_menu_selection(selection, menu);
}

void main_menu(int boot_media, int reboot_mode, int hard_reboot_mode) {
	struct mmc_device *dev = target_mmc_device();
	int ret = 0;

	device_info *device = get_device_info();
	cached_bl_min = device->min_backlight;
	cached_bl_max = device->max_backlight;

	if (dev->config.slot == SD_CARD) // If we have SD already initialized here - it is bootable
		sdcard_is_bootable = true;
	else if (target_sdc_init_slot(SD_CARD)) // Else let's try to initialize SD now
		sdcard_is_bootable = true;
	else // SD wasn't initialized or doesn't have boot partitions
		sdcard_is_bootable = false;

	if (emmc_health != EMMC_FAILURE)
		target_sdc_init_slot(EMMC_CARD); // Try to initialize eMMC now and fetch it health state to emmc_health var

	fbcon_set_splash_pos(-1, 220);
	display_image_on_screen();
	fbcon_set_header_line(7);
	fbcon_print_version();
	fbcon_set_storage_status(); // We must update storage status to make it visible after display_image_on_screen()
	fbcon_print_init_time();

	int default_selection = DEFAULT_ITEM;
	dprintf(SPEW, "%s: reboot_mode=%x; hard_reboot_mode=%x\n", __func__, reboot_mode, hard_reboot_mode);
	dprintf(SPEW, "%s: reboot_mode FASTBOOT=%x; reboot_mode RECOVERY=%x\n", __func__, FASTBOOT_MODE, RECOVERY_MODE);
	dprintf(SPEW, "%s: hard_reboot_mode FASTBOOT=%x; hard_reboot_mode RECOVERY=%x\n\n", __func__, FASTBOOT_HARD_RESET_MODE, RECOVERY_HARD_RESET_MODE);
	if (reboot_mode == RECOVERY_MODE || hard_reboot_mode == RECOVERY_HARD_RESET_MODE) {
			switch (boot_media) {
			case BOOT_MEDIA_EMMC:
				default_selection = EMMC_RECOVERY;
				break;
			case BOOT_MEDIA_SD:
				default_selection = SD_RECOVERY;
				break;
			default:
				break;
		}
	} else if(reboot_mode == FASTBOOT_MODE || hard_reboot_mode == FASTBOOT_HARD_RESET_MODE) {
			switch (boot_media) {
			case BOOT_MEDIA_EMMC:
				default_selection = EMMC_FASTBOOT;
				break;
			case BOOT_MEDIA_SD:
				default_selection = SD_FASTBOOT;
				break;
			default:
				break;
		}
	} else {
			switch (boot_media) {
			case BOOT_MEDIA_EMMC:
				default_selection = EMMC_BOOT;
				break;
			case BOOT_MEDIA_SD:
				default_selection = SD_BOOT;
				break;
			default:
				break;
		}
	}
	draw_menu(boot_menu, default_selection);
}
