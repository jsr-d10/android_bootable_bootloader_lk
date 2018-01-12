#ifndef __MENU_H
#define __MENU_H

// Maximum menu item title length
#define MAX_ITEM_LEN 26
#define KEY_SCAN_FREQ 100

enum item_types {
	DEFAULT_ITEM = 0,
	EMMC_BOOT,
	EMMC_RECOVERY,
	EMMC_FASTBOOT,
	SD_BOOT,
	SD_RECOVERY,
	SD_FASTBOOT,
	FASTBOOT_REBOOT,
	DLOAD_NORMAL,
	DLOAD_EMERGENCY,
	REBOOT,
	SHUTDOWN,
	BOOT_MENU,
	REBOOT_MENU,
	OPTIONS_MENU,
	ADVANCED_MENU,
	MULTIBOOT_SLOT_SELECT,
	CHARGER_SCREEN_TOGGLE,
	CHARGING_TOGGLE,
	ISOLATED_SDCARD_TOGGLE,
	DEFAULT_BOOT_MEDIA_TOGGLE,
	BOOTMENU_ON_BOOT_TOGGLE,
	PERMISSIVE_SELINUX_TOGGLE,
	LLCON_TOGGLE,
	LLCON_OPTIONS_MENU,
	LLCON_WRAP_TOGGLE,
	LLCON_FONT_TOGGLE,
	LLCON_COLOR_TOGGLE,
	BL_CONTROL_OPTIONS_MENU,
	BL_CONTROL_TOGGLE,
	BL_MIN_ADJUST,
	BL_MAX_ADJUST,
	EMMC_READ_SPEED_TEST,
	SD_READ_SPEED_TEST,
	FULL_EMMC_READ_SPEED_TEST,
	FULL_SD_READ_SPEED_TEST
};

struct menu_item {
	uint32_t x_pos;
	uint32_t y_pos;
	uint32_t fg_color;
	uint32_t id;
	int type;
	char name[32+1];
	struct menu_item *previous;
	struct menu_item *next;
};

struct menu {
	char name[32+1];
	uint32_t fg_color;
	uint32_t cursor;
	struct menu_item *item;
};

static struct menu *
create_menu(
	char *menu_name,
	uint32_t fg_color,
	uint32_t cursor);

static struct menu_item *add_menu_item(
	struct menu *menu,
	uint32_t x_pos,
	uint32_t y_pos,
	uint32_t fg_color,
	char *name,
	enum item_types type);

static void draw_menu(struct menu *menu_function(void), int default_selection);
void main_menu(int boot_media, int reboot_mode, int hard_reboot_mode);
struct menu_item *get_item_by_type(struct menu *menu, int type);

#endif // __MENU_H
