/* artik_updater - Firmware Updater Utility for Artik
 *
 * Copyright (C) 2016 Samsung Electronics
 * Author: Jaewon Kim <jaewon02.kim@samsung.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <fcntl.h>

#define EMMC_DEV		"/dev/mmcblk0p"
#define SDCARD_DEV		"/dev/mmcblk1p"

#define FLAG_PART		"1"
#define BOOT0_PART		"2"
#define BOOT1_PART		"3"
#define MODULES0_PART		"5"
#define MODULES1_PART		"6"

#define CMDLINE			"/proc/cmdline"
#define PART_SIZE		(32 * 1024 * 1024)
#define PATH_LEN		32
#define VERSION			"v0.3"
#define HEADER_MAGIC		"ARTIK_OTA"

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

enum boot_part check_booting_part(void)
{
	FILE *fp;
	char cmdline[256] = {0, };
	enum boot_part bootpart;

	fp = fopen(CMDLINE, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", CMDLINE);
		return -ENODEV;
	}

	while (fscanf(fp, "%s", cmdline) != EOF) {
		if (strncmp(cmdline, "bootfrom=2", 10) == 0) {
			bootpart = PART0;
			break;
		} else if (strncmp(cmdline, "bootfrom=3", 10) == 0) {
			bootpart = PART1;
			break;
		} else {
			bootpart = -EINVAL;
		}
	}
	fclose(fp);

	return bootpart;
}

enum boot_dev check_booting_dev(void)
{
	FILE *fp;
	char cmdline[256] = {0, };
	enum boot_dev bootdev;

	fp = fopen(CMDLINE, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", CMDLINE);
		return -ENODEV;
	}

	while (fscanf(fp, "%s", cmdline) != EOF) {
		if (strncmp(cmdline, "root=/dev/mmcblk0", 17) == 0) {
			bootdev = EMMC;
			break;
		} else if (strncmp(cmdline, "root=/dev/mmcblk1", 17) == 0) {
			bootdev = SDCARD;
			break;
		} else {
			bootdev = -EINVAL;
		}
	}
	fclose(fp);

	return bootdev;
}

int read_boot_info(struct boot_info *boot)
{
	int fd, ret;
	enum boot_dev bootdev;
	char flag_path[PATH_LEN];

	/* Read Partition Information */
	bootdev = check_booting_dev();
	if (bootdev == EMMC)
		snprintf(flag_path, PATH_LEN, "%s%s", EMMC_DEV, FLAG_PART);
	else if (bootdev == SDCARD)
		snprintf(flag_path, PATH_LEN, "%s%s", SDCARD_DEV, FLAG_PART);
	else
		return -ENODEV;

	fd = open(flag_path, O_RDWR | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "Error: Can not open FLAG Partition\n");
		return fd;
	}

	ret = read(fd, boot, sizeof(struct boot_info));
	if (ret < sizeof(struct boot_info)) {
		fprintf(stderr, "Error: Can not read FLAG Partition\n");
		close(fd);
		return ret;
	}

	/* Check Header Magic */
	if (strncmp(boot->header_magic, HEADER_MAGIC,
				strlen(HEADER_MAGIC)) != 0) {
		fprintf(stderr, "Error: This is not FLAG Partition\n");
		close(fd);
		return -1;
	}

	return fd;
}

int write_boot_info(int fd, struct boot_info *info)
{
	int ret;

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "Error: Cannot change position\n");
		return ret;
	}

	ret = write(fd, info, sizeof(struct boot_info));
	if (ret < sizeof(struct boot_info)) {
		fprintf(stderr, "Error: Cannot write FLAG partition\n");
		return ret;
	}

	return 0;
}

int write_image(const char *file_path, char *part)
{
	FILE *in, *out;
	char *buf;
	size_t len;

	if ((in = fopen(file_path, "rb")) == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", file_path);
		return -1;
	}

	if ((out = fopen(part, "wb")) == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", part);
		fclose(in);
		return -1;
	}

	/* Copies in 1M unit */
	if ((buf = (char *)malloc(1024 * 1024)) == NULL) {
		fclose(in);
		fclose(out);
		return -1;
	}

	while ((len = fread(buf, sizeof(char), 1024 * 1024, in)) != 0) {
		if (fwrite(buf, sizeof(char), len, out) == 0) {
			fclose(in);
			fclose(out);
			free(buf);
			return -1;
		}
	}

	fclose(in);
	fclose(out);
	free(buf);

	return 0;
}

int update_boot_info(int fd, struct boot_info *boot, const char *tag)
{
	int ret;
	struct part_info *part;

	boot->state = BOOT_UPDATED;
	if (boot->part_num == PART0)
		part = &boot->part0;
	else if (boot->part_num == PART1)
		part = &boot->part1;

	part->state = BOOT_UPDATED;
	part->retry = 1;

	if (tag == NULL)
		memset(part->tag, 0, 32);
	else
		snprintf(part->tag, strlen(tag) + 1, "%s", tag);

	ret = write_boot_info(fd, boot);
	if (ret < 0)
		return ret;

	return 0;
}

int check_booting_rescue(void)
{
	FILE *fp;
	char cmdline[256];
	int rescue = 0;

	fp = fopen(CMDLINE, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", CMDLINE);
		return -ENODEV;
	}

	while (fscanf(fp, "%s", cmdline) != EOF) {
		if (strncmp(cmdline, "rescue=0", 8) == 0)
			rescue = 0;
		else if (strncmp(cmdline, "rescue=1", 8) == 0)
			rescue = 1;
		else
			rescue = -EINVAL;
	}
	fclose(fp);

	return rescue;
}

int change_boot_state(enum boot_state state)
{
	struct boot_info boot;
	struct part_info *part;
	int fd, ret, rescue;

	fd = read_boot_info(&boot);
	if (fd < 0)
		return fd;

	if (state == BOOT_SUCCESS)
		boot.state = BOOT_SUCCESS;

	rescue = check_booting_rescue();
	if (rescue < 0)
		return rescue;

	if (state == BOOT_FAILED && rescue) {
		if (check_booting_part() == PART0) {
			boot.part_num = PART1;
			part = &boot.part1;
		} else {
			boot.part_num = PART0;
			part = &boot.part0;
		}
	} else {
		(check_booting_part() == PART0) ?
			(part = &boot.part0) : (part = &boot.part1);
	}

	part->state = state;

	ret = write_boot_info(fd, &boot);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	close(fd);

	return 0;
}

char *state_to_str(enum boot_state state, char *str)
{
	switch (state) {
	case BOOT_FAILED:
		snprintf(str, 16, "%s", "Failed");
		break;
	case BOOT_SUCCESS:
		snprintf(str, 16, "%s", "Success");
		break;
	case BOOT_UPDATED:
		snprintf(str, 16, "%s", "Updated");
		break;
	default:
		printf("Unsupported state = %d\n", state);
		memset(str, 0, 16);
		return NULL;
	}

	return str;
}

void show_boot_info(struct boot_info *boot)
{
	char msg[16];

	/* Show Information */
	printf("------------------------------------\n");
	printf("FLAG Partition Information\n");
	printf("------------------------------------\n");
	printf("Boot State = [%s(%s)]\n",
			state_to_str(boot->state, msg),
			check_booting_rescue() == 0 ? "Normal" : "Recovery"
			);
	printf("Partition 0\n");
	printf("\t- State = [%s]\n", state_to_str(boot->part0.state, msg));
	printf("\t- Tag = [%s]\n", boot->part0.tag);
	printf("Partition 1\n");
	printf("\t- State = [%s]\n", state_to_str(boot->part1.state, msg));
	printf("\t- Tag = [%s]\n", boot->part1.tag);
	printf("------------------------------------\n");
}

void print_usage(void)
{
	printf("Usage:\n");
	printf("  artik_updater <option>\n");
	printf("  artik_updater -b <file> -m <file> -t <tag>\n");
	printf("\nOption Description:\n");
	printf("  -i\t\t\tshow partition information\n");
	printf("  -s\t\t\tbooting state change to success\n");
	printf("  -f\t\t\tbooting state change to fail\n");
	printf("  -b <file>\t\tboot image\n");
	printf("  -m <file>\t\tmodule image\n");
	printf("  -t <tag>\t\tpartition tag\n");
	printf("  -v \t\t\ttool version\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	const char *boot_file = NULL;
	const char *modules_file = NULL;
	char bootdev_path[PATH_LEN] = {0, };
	char boot_path[PATH_LEN] = {0, };
	char modules_path[PATH_LEN] = {0, };
	enum boot_part bootpart;
	enum boot_dev bootdev;
	const char *tag = NULL;
	int fd, ret, opt;
	struct boot_info current_boot;
	struct option opts[] = {
		{"info", no_argument, 0, 'i'},
		{"set", no_argument, 0, 's'},
		{"boot", required_argument, 0, 'b'},
		{"module", required_argument, 0, 'm'},
		{"tag", required_argument, 0, 't'},
		{"tool-version", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	/* Option parameter handling */
	while (1) {
		opt = getopt_long(argc, argv, "ivhsfb:m:t:", opts, NULL);
		if (opt < 0)
			break;

		switch (opt) {
		case 'i':
			fd = read_boot_info(&current_boot);
			if (fd < 0)
				return ret;
			show_boot_info(&current_boot);
			fsync(fd);
			close(fd);
			return 0;
		case 's':
			ret = change_boot_state(BOOT_SUCCESS);
			if (ret < 0)
				return ret;
			return 0;
		case 'f':
			ret = change_boot_state(BOOT_FAILED);
			if (ret < 0)
				return ret;
			return 0;
		case 'b':
			boot_file = optarg;
			ret = access(boot_file, 0);
			if (ret < 0) {
				fprintf(stderr, "Cannot file : %s\n", boot_file);
				return -EINVAL;
			}
			break;
		case 'm':
			modules_file = optarg;
			ret = access(modules_file, 0);
			if (ret < 0) {
				fprintf(stderr, "Cannot file : %s\n", modules_file);
				return -EINVAL;
			}
			break;
		case 't':
			tag = optarg;
			break;
		case 'v':
			printf("Artik Firmware Update Utility %s\n", VERSION);
			return 0;
		case 'h':
			print_usage();
			return 0;
		default:
			print_usage();
			return 0;
		}
	}

	if (boot_file == NULL || modules_file == NULL) {
		print_usage();
		return -EINVAL;
	}

	signal(SIGINT, SIG_IGN);

	/* Read FLAG partition */
	fd = read_boot_info(&current_boot);
	if (fd < 0)
		return fd;

	/* Write Image to partition */
	bootdev = check_booting_dev();
	if (bootdev == EMMC)
		snprintf(bootdev_path, PATH_LEN, "%s", EMMC_DEV);
	else if (bootdev == SDCARD)
		snprintf(bootdev_path, PATH_LEN, "%s", SDCARD_DEV);
	else {
		printf("Cannot find bootdev\n");
		close(fd);
		return -1;
	}

	bootpart = check_booting_part();
	if (bootpart == PART0) {
		snprintf(boot_path, PATH_LEN, "%s%s",
				bootdev_path, BOOT1_PART);
		snprintf(modules_path, PATH_LEN, "%s%s",
				bootdev_path, MODULES1_PART);
		current_boot.part_num = PART1;
	} else if (bootpart == PART1) {
		snprintf(boot_path, PATH_LEN, "%s%s",
				bootdev_path, BOOT0_PART);
		snprintf(modules_path, PATH_LEN, "%s%s",
				bootdev_path, MODULES0_PART);
		current_boot.part_num = PART0;
	} else {
		printf("Cannot find bootpart\n");
		close(fd);
		return -1;
	}

	printf("Write Boot Image : Wait\n");
	ret = write_image(boot_file, boot_path);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	printf("Write Modules Image : Wait\n");
	ret = write_image(modules_file, modules_path);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	/* Update Boot Flags */
	ret = update_boot_info(fd, &current_boot, tag);
	if (ret < 0) {
		close (fd);
		return ret;
	}

	fsync(fd);
	close(fd);

	return 0;
}
