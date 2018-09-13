#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/types.h>
#include <linux/errno.h>
#include "artik-updater.h"
#include "ota-common.h"
#include "crc32.h"

#define FLAG_SIZE	(128 * 1024)

int show_image(const char *image_name)
{
	struct boot_header *hdr;
	FILE *image;
	char *buf;

	if ((buf = (char *)malloc(FLAG_SIZE)) == NULL)
		return -ENOMEM;

	if ((image = fopen(image_name, "rb")) == NULL) {
		fprintf(stderr, "Cannot read image file %s\n", image_name);
		free(buf);
		return -1;
	}

	if (fread(buf, sizeof(char), FLAG_SIZE, image) == 0) {
		fclose(image);
		free(buf);
		return -1;
	}

	hdr = (struct boot_header *)buf;
	if (show_flag(hdr) < 0) {
		fprintf(stderr, "Cannot read flag info\n");
		fclose(image);
		free(buf);
		return -1;
	}

	return 0;
}

int create_image(const char *image_name)
{
	struct boot_header hdr;
	BLOCK *block;
	PART *part;
	FILE *image;
	char *buf;

	if ((buf = (char *)malloc(FLAG_SIZE)) == NULL)
		return -ENOMEM;

	memset(buf, 0, FLAG_SIZE);
	memset(&hdr, 0, sizeof(struct boot_header));

	if ((image = fopen(image_name, "wb")) == NULL) {
		fprintf(stderr, "Cannot create file %s\n", image_name);
		free(buf);
		return -1;
	}

	/* Fill the data */
	strncpy(hdr.header_magic, HEADER_MAGIC, 32);
	hdr.major = VERSION_MAJOR;
	hdr.minor = VERSION_MINOR;

	/* BLOCK Boot */
	block = &hdr.blocks.block_boot;
	block->active= PART_A;
	block->b_state= SUCCESS;
	strncpy(block->block_name, "boot", NAME_SIZE);
	part = &block->part_a;
	part->p_state = SUCCESS;
	part->part_n = 2;
	part->retry = 0;
	strncpy(part->tag, "Default", TAG_SIZE);
	part = &block->part_b;
	part->part_n = 3;

	/* BLOCK Module */
	block = &hdr.blocks.block_module;
	block->active= PART_A;
	block->b_state= SUCCESS;
	strncpy(block->block_name, "module", NAME_SIZE);
	part = &block->part_a;
	part->p_state = SUCCESS;
	part->part_n = 4;
	part->retry = 0;
	strncpy(part->tag, "Default", TAG_SIZE);
	part = &block->part_b;
	part->part_n = 5;

	/* BLOCK Reserved */
	block = &hdr.blocks.block_res2;
	block->b_state= UNUSED;
	block = &hdr.blocks.block_res3;
	block->b_state= UNUSED;
	block = &hdr.blocks.block_res4;
	block->b_state= UNUSED;
	/* Add Checksum */
	hdr.checksum = crc32(0, &hdr.blocks, sizeof(struct blocks));

	memcpy(buf, &hdr, sizeof(struct boot_header));
	if (fwrite(buf, sizeof(char), FLAG_SIZE, image) == 0) {
		fprintf(stderr, "Cannot write file %s\n", image_name);
		fclose(image);
		free(buf);
	}

	return 0;
}

void print_usage()
{
	printf("Usage: flag_checker [OPTION]... [FILE]...\n");
	printf(" -c	: Create flag image\n");
	printf(" -i	: Show flag information\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	const char *file = NULL;
	int opt, show = 0, create = 0;
	struct option opts[] = {
		{"create", required_argument, 0, 'c'},
		{"info", required_argument, 0, 'i'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	while (1) {
		opt = getopt_long(argc, argv, "c:i:", opts, NULL);
		if (opt < 0)
			break;
		switch (opt) {
		case 'c':
			create = 1;
			file = optarg;
			break;
		case 'i':
			show = 1;
			file = optarg;
			break;
		case 'h':
		default:
			print_usage();
			return 0;
		}
	}
	if ((file == NULL) || (show == create)) {
		print_usage();
		return 0;
	}

	if (create)
		create_image(file);
	if (show)
		show_image(file);

	return 0;
}
