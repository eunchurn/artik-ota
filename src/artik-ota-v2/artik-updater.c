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
#include "artik-updater.h"
#include "ota-common.h"

void print_usage(void)
{
	printf("Usage:\n");
	printf("  artik_updater <option>\n");
	printf("  artik_updater -b <block_name> -f <file_name> -t <tag>\n");
	printf("  artik_updater -b <block_name> -s <success/fail>\n");
	printf("\nOption Description:\n");
	printf("  -b <block>\t\tblock name\n");
	printf("  -f <file>\t\tfile name\n");
	printf("  -t <tag>\t\tpartition tag\n");
	printf("  -s <success/fail>\tstate\n");
	printf("  -i\t\t\tshow partition information\n");
	printf("  -v \t\t\ttool version\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	const char *file = NULL;
	const char *tag = NULL;
	const char *block_name = NULL;
	const char *check = NULL;
	BLOCK *block;
	struct boot_header hdr;
	int ret, opt;
	struct option opts[] = {
		{"info", no_argument, 0, 'i'},
		{"state", no_argument, 0, 's'},
		{"block", required_argument, 0, 'b'},
		{"file", required_argument, 0, 'f'},
		{"tag", required_argument, 0, 't'},
		{"version", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	if (!is_ota_support())
		return -EINVAL;

	if (read_boot_header(&hdr) < 0)
		return -ENODEV;

	/* Option parameter handling */
	while (1) {
		opt = getopt_long(argc, argv, "ivhs:f:b:t:", opts, NULL);
		if (opt < 0)
			break;

		switch (opt) {
		case 's': /* state */
			check = optarg;
			break;
		case 'f': /* file */
			file = optarg;
			ret = access(file, 0);
			if (ret < 0) {
				fprintf(stderr, "Cannot find : %s\n", file);
				return -EINVAL;
			}
			break;
		case 'b': /* block */
			block_name = optarg;
			break;
		case 't': /* tag */
			tag = optarg;
			break;
		case 'i': /* information */
			show_flag(&hdr);
			return 0;
		case 'v': /* version */
			printf("Artik Firmware Update Utility v%d.%d\n",
					VERSION_MAJOR, VERSION_MINOR);
			return 0;
		case 'h':
		default:
			print_usage();
			return 0;
		}
	};

	signal(SIGINT, SIG_IGN);

	/* Find block */
	if (block_name == NULL) {
		print_usage();
		return -1;
	}
	block = get_block_info(&hdr, block_name);
	if (block == NULL) {
		fprintf(stderr, "Wrong block name\n");
		return -1;
	}

	/* Update state */
	if (check != NULL) {
		if (strncmp(check, "success", 8) == 0) {
			ret = update_boot_state(&hdr, block, 1);
		} else if(strncmp(check, "fail", 8) == 0) {
			ret = update_boot_state(&hdr, block, 0);
		} else {
			fprintf(stderr, "unsupported command %s\n", check);
			ret = -1;
		}

		return ret;
	}

	/* Update part */
	if (file == NULL) {
		print_usage();
		return -1;
	}

	if (update_image(&hdr, block, file, tag) < 0) {
		fprintf(stderr, "Update failed\n");
		return -1;
	}

	return 0;
}
