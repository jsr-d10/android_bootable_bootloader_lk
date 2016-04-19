#include <sys/types.h>
#include <dev/keys.h>
#include <dev/fbcon.h>
#include <debug.h>
#include <target.h>
#include <dload_util.h>
#include "board.h"
#include <app/aboot/aboot.h>
#include <app/aboot/menu.h>
#include <sdhci_msm.h>

int sdcard_is_bootable = false;
int autoboot = true;

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
	menu = create_menu ("Options menu", SILVER, 0x10);

	char charger_screen[MAX_ITEM_LEN];
	char charging_in_bootloader[MAX_ITEM_LEN];
	char isolated_sdcard_boot[MAX_ITEM_LEN];
	char default_boot_media[MAX_ITEM_LEN];

	snprintf(charger_screen, MAX_ITEM_LEN, "CHARGER SCREEN:         %s", device->charger_screen_enabled ? "Y" : "N");
	snprintf(charging_in_bootloader, MAX_ITEM_LEN, "CHARGING IN BOOTLOADER: %s", device->charging_enabled ? "Y" : "N");
	snprintf(isolated_sdcard_boot, MAX_ITEM_LEN, "ISOLATED SDCARD BOOT:   %s", device->isolated_sdcard ? "Y" : "N");

	add_menu_item(menu, 1, header_line + 2, NAVY,    "BACK TO BOOT MENU =>",   BOOT_MENU);
	add_menu_item(menu, 1, header_line + 3, RED,     charger_screen,         CHARGER_SCREEN_TOGGLE);
	add_menu_item(menu, 1, header_line + 4, YELLOW,  charging_in_bootloader, CHARGING_TOGGLE);
	add_menu_item(menu, 1, header_line + 5, FUCHSIA, isolated_sdcard_boot,   ISOLATED_SDCARD_TOGGLE);

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
		add_menu_item(menu, 1, header_line + 6, SILVER,  default_boot_media,     DEFAULT_BOOT_MEDIA_TOGGLE);
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

static uint32_t process_menu(struct menu *menu) {
	struct menu_item *selected = menu->item;
	move_cursor(selected, selected, LIME, menu->cursor);
	int timeout = 30 * 1000; // 30 seconds
	if (autoboot) {
		fbcon_acprintf(2, ALIGN_LEFT, BLUE, "  Autoboot in %2d.%d seconds\n", timeout/1000, (timeout%1000)/10 );
		fbcon_set_font_fg_color(RED);
	}
	while (timeout > 0) {
		if (autoboot) {
			if (timeout%100)
			fbcon_set_bg(BLACK, strlen("  Autoboot in "), 2, strlen("30.0"), 1);
			fbcon_set_cursor_pos(strlen("  Autoboot in "), 2);
			fbcon_printf("%2d.%1d", timeout/1000, (timeout%1000)/100 );
		} else {
			fbcon_set_bg(BLACK, 0, 2, -1, 1);
		}
		target_keystatus();
		if (keys_get_state(KEY_VOLUMEUP)) {
			wait_vib_timeout();
			vib_timed_turn_on(100);
			move_cursor(selected, selected->previous, LIME, menu->cursor);
			selected=selected->previous;
			thread_sleep(300);
			autoboot = false;
			continue;
		}
		if (keys_get_state(KEY_VOLUMEDOWN)) {
			wait_vib_timeout();
			vib_timed_turn_on(100);
			move_cursor(selected, selected->next, LIME, menu->cursor);
			selected=selected->next;
			thread_sleep(300);
			autoboot = false;
			continue;
		}
		if (keys_get_state(KEY_POWER)) {
			wait_vib_timeout();
			vib_timed_turn_on(256);
			move_cursor(selected, selected, RED, menu->cursor);
			autoboot = false;
			break;
		}
		if (autoboot) {
			thread_sleep(20);
			timeout -= 40; // This mismatch is not a typo!
		}
	}
	if (autoboot)
		fbcon_set_bg(BLACK, 0, 2, -1, 1);
	return selected->type;
}

static void handle_menu_selection(uint32_t selection, struct menu *menu) {
	struct mmc_device *dev;

	device_info *device = get_device_info();
	dprintf(SPEW, "%s: processing selection=%d\n", __func__, selection);
	dev = target_mmc_device();

	switch (selection) {
		case EMMC_BOOT:
		case EMMC_RECOVERY:
		case EMMC_FASTBOOT:
			set_last_boot_media(BOOT_MEDIA_EMMC);
			if (!(dev && dev->config.slot == EMMC_CARD))
				target_sdc_init_slot(EMMC_CARD);
			swap_sdcc = SDCC_EMMC_SD;
			break;

		case SD_BOOT:
		case SD_RECOVERY:
		case SD_FASTBOOT:
			if (!(dev && dev->config.slot == SD_CARD))
				target_sdc_init_slot(SD_CARD);
			swap_sdcc = device->isolated_sdcard ? SDCC_SD_ONLY : SDCC_SD_EMMC;
			set_last_boot_media(BOOT_MEDIA_SD);
			break;
	}

	boot_into_fastboot = false;
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
			boot_into_fastboot = true;
			break;

		case REBOOT_MENU:
			destroy_menu(menu);
			draw_menu(reboot_menu, 500);
			break;

		case BOOT_MENU:
			destroy_menu(menu);
			draw_menu(boot_menu, 500);
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
			draw_menu(options_menu, 500);
			break;

		case CHARGER_SCREEN_TOGGLE:
			if (device->charger_screen_enabled)
				cmd_oem_disable_charger_screen(NULL, NULL, 0);
			else
				cmd_oem_enable_charger_screen(NULL, NULL, 0);
			destroy_menu(menu);
			draw_menu(options_menu, 500);
			break;

		case CHARGING_TOGGLE:
			if (device->charging_enabled)
				cmd_oem_disable_charging(NULL, NULL, 0);
			else
				cmd_oem_enable_charging(NULL, NULL, 0);
			destroy_menu(menu);
			draw_menu(options_menu, 500);
			break;

		case ISOLATED_SDCARD_TOGGLE:
			if (device->isolated_sdcard)
				cmd_oem_disable_isolated_sdcard_boot(NULL, NULL, 0);
			else
				cmd_oem_enable_isolated_sdcard_boot(NULL, NULL, 0);
			destroy_menu(menu);
			draw_menu(options_menu, 500);
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
			destroy_menu(menu);
			draw_menu(options_menu, 500);
			break;

		default:
			break;
	}
}

void draw_menu(struct menu *menu_function(void), uint32_t delay) {
	struct menu *menu = menu_function();
	show_menu(menu);
	thread_sleep(delay);
	uint32_t selection = process_menu(menu);

	// Hide menu title after selection
	fbcon_hprint("", BLACK);

	handle_menu_selection(selection, menu);
}

void main_menu(void) {
	struct mmc_device *dev = target_mmc_device();
	int ret = 0;

	if (dev && dev->config.slot == SD_CARD) // If we have SD already initialized here - it is bootable
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

	draw_menu(boot_menu, 0);
}
