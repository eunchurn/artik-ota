/* artik_updater - Firmware Updater Utility for Artik
 *
 * Copyright (C) 2016 Samsung Electronics
 * Author: Jaewon Kim <jaewon02.kim@samsung.com>
 */

#ifndef _ARTIK_UPDATER_H
#define	_ARTIK_UPDATER_H

#define EMMC_DEV		"/dev/mmcblk0p"
#define SDCARD_DEV		"/dev/mmcblk1p"

#define FLAG_PART		"1"
#define CMDLINE			"/proc/cmdline"
#define COMPATIBLE		"/proc/device-tree/compatible"
#define PATH_LEN		128
#define CMD_LEN			1024
#define HEADER_MAGIC		"ARTIK_OTA"
#define MB			(1024 * 1024)

enum boot_dev {
	EMMC	= 0,
	SDCARD,
};

enum boot_part {
	PART0	= 0,
	PART1,
};

enum boot_state {
	BOOT_FAILED	= 0,
	BOOT_SUCCESS,
	BOOT_UPDATED,
};

struct part_info {
	enum boot_state state;
	char retry;
	char tag[32];
};

struct boot_info {
	char header_magic[32];
	enum boot_part part_num;
	enum boot_state state;
	struct part_info part0;
	struct part_info part1;
};

struct target_info {
	char *name;
	char *boot0_p;
	char *boot1_p;
	char *modules0_p;
	char *modules1_p;
	int boot_s;
	int modules_s;
};

#endif /* _ARTIK_UPDATER_H */
