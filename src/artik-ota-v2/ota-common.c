#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "artik-updater.h"
#include "crc32.h"

int is_ota_support(void)
{
	FILE *fp;
	char cmdline[CMD_LEN];

	if ((fp = fopen(CMDLINE, "r")) == NULL) {
		fprintf(stderr, "Cannot open CMDLINE\n");
		return 0;
	}

	while (fscanf(fp, "%255s", cmdline) != EOF) {
		if (strncmp(cmdline, "ota", 8) == 0) {
			fclose(fp);
			return 1;
		}
	}

	fflush(fp);
	fclose(fp);

	return 0;
}

int is_rescue(void)
{
	FILE *fp;
	char cmdline[CMD_LEN];

	if ((fp = fopen(CMDLINE, "r")) == NULL) {
		fprintf(stderr, "Cannot open CMDLINE\n");
		return 0;
	}

	while (fscanf(fp, "%255s", cmdline) != EOF) {
		if (strncmp(cmdline, "rescue=1", 8) == 0) {
			fclose(fp);
			return 1;
		}
	}

	fflush(fp);
	fclose(fp);

	return 0;
}

static char *state_to_char(enum part_state state)
{
	switch (state) {
	case UNUSED:
		return "UNUSED";
	case FAIL:
		return "FAIL";
	case SUCCESS:
		return "SUCCESS";
	case UPDATE:
		return "UPDATE";
	default:
		return "NULL";
	};

	return NULL;
}

static void show_block(BLOCK *block)
{
	PART *part;


	if (block->b_state == UNUSED)
		return;

	printf("Block - %s\n", block->block_name);

	printf(" Active: %s\n", block->active == PART_A ? "PART_A" : "PART_B");
	printf(" State: %s\n", state_to_char(block->b_state));
	part = &block->part_a;
	printf("  * PART_A\n");
	printf("\tState:\t\t%s\n", state_to_char(part->p_state));
	printf("\tPartNumber:\t%d\n", part->part_n);
	printf("\tTag:\t\t%s\n", part->tag);
	printf("\tChecksum:\t%x\n", part->checksum);
	part = &block->part_b;
	printf("  * PART_B\n");
	printf("\tState:\t\t%s\n", state_to_char(part->p_state));
	printf("\tPartNumber:\t%d\n", part->part_n);
	printf("\tTag:\t\t%s\n", part->tag);
	printf("\tChecksum:\t%x\n", part->checksum);
	printf("-----------------------------------\n");
}

int show_flag(struct boot_header *hdr)
{
	if (strncmp(hdr->header_magic, HEADER_MAGIC, 32) != 0) {
		fprintf(stderr, "This is not FLAG information\n");
		return -1;
	}

	if (crc32(0, &hdr->blocks, sizeof(struct blocks)) != hdr->checksum) {
		fprintf(stderr, "Tainted FLAG information\n");
		return -1;
	}

	printf("===================================\n");
	printf("ARTIK UPDATER\n");
	printf("Version: v%d.%d\n", hdr->major, hdr->minor);
	printf("Checksum: %x\n", hdr->checksum);
	printf("===================================\n");
	show_block(&hdr->blocks.block_boot);
	show_block(&hdr->blocks.block_module);
	show_block(&hdr->blocks.block_res1);
	show_block(&hdr->blocks.block_res2);
	show_block(&hdr->blocks.block_res3);
	show_block(&hdr->blocks.block_res4);
	printf("===================================\n");

	return 0;
}

static int hwpart_ctrl_ro(int enable_ro)
{
	FILE *fp;
	char ctrl_msg;

	if ((fp = fopen(BOOTCTRL, "wb")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", BOOTCTRL);
		return -1;
	}

	((enable_ro & 0x1) == 0) ? (ctrl_msg = '0') : (ctrl_msg = '1');
	if (fwrite(&ctrl_msg, sizeof(char), 1, fp) == 0) {
		fprintf(stderr, "Cannot ctrl %s to %d\n", BOOTCTRL, enable_ro);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	return 0;
}

int read_boot_header(struct boot_header *hdr)
{
	FILE *fp;

	if ((fp = fopen(BOOTPART, "rb")) == NULL) {
		fprintf(stderr, "Cannot open flag part\n");
		return -1;
	}

	fseek(fp, FLAG_OFFSET, SEEK_SET);
	if (fread(hdr, sizeof(struct boot_header), 1, fp) == 0) {
		fprintf(stderr, "Cannot read flag information\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

int write_boot_header(struct boot_header *hdr)
{
	FILE *fp;
	size_t len;
	if (hwpart_ctrl_ro(0) < 0) {
		fprintf(stderr, "Cannot change boot part to rw\n");
		return -1;
	}

	if ((fp = fopen(BOOTPART, "wb")) == NULL) {
		fprintf(stderr, "Cannot open flag information\n");
		hwpart_ctrl_ro(1);
		return -1;
	}

	hdr->checksum = crc32(0, &hdr->blocks, sizeof(struct blocks));
	fseek(fp, FLAG_OFFSET, SEEK_SET);
	if ((len = fwrite(hdr, sizeof(struct boot_header), 1, fp)) == 0) {
		fprintf(stderr, "Cannot write flag information\n");
		hwpart_ctrl_ro(1);
		fclose(fp);
		return -1;
	}

	fflush(fp);
	fclose(fp);

	if (hwpart_ctrl_ro(0) < 0) {
		fprintf(stderr, "Cannot change boot part to rw\n");
		return -1;
	}

	return 0;
}

int update_boot_state(struct boot_header *hdr, BLOCK *block, uint8_t state)
{
	PART *part = NULL;

	(block->active == PART_A) ?
		(part = &block->part_a) : (part = &block->part_b);

	if (state == 1) {
		part->p_state = SUCCESS;
		block->b_state = SUCCESS;
	} else if (state == 0){
		if (block->b_state != FAIL) {
			(block->active == PART_A) ?
				(block->active = PART_B ) :
				(block->active = PART_A);
			part->p_state = FAIL;
			block->b_state = FAIL;
		}
	}

	if (write_boot_header(hdr) < 0) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	return 0;
}

int get_checksum(char *file)
{
	FILE *fp;
	char *buf;
	size_t size, size_split;
	uint32_t crc = 0;

	if ((fp = fopen(file, "rb")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", file);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	(size > 1 * MB) ? (size_split = 1 * MB) : (size_split = size);

	buf = (char *)malloc(sizeof(char) * size);

	while (size > 0) {
		if (size < size_split)
			size_split = size;

		if (fread(buf, sizeof(char), size_split, fp) == 0) {
			fprintf(stderr, "Cannot read file %s\n", file);
			crc = 0;
			break;
		}
		crc = crc32(crc, buf, size_split);
		size -= size_split;
	}

	free(buf);
	fclose(fp);

	return crc;
}

BLOCK *get_block_info(struct boot_header *hdr, char *name)
{
	BLOCK *block = NULL;

	block = &hdr->blocks.block_boot;
	if (strncmp(block->block_name, name, 32) == 0)
		return block;

	block = &hdr->blocks.block_module;
	if (strncmp(block->block_name, name, 32) == 0)
		return block;

	block = &hdr->blocks.block_res1;
	if (strncmp(block->block_name, name, 32) == 0)
		return block;

	block = &hdr->blocks.block_res2;
	if (strncmp(block->block_name, name, 32) == 0)
		return block;

	block = &hdr->blocks.block_res3;
	if (strncmp(block->block_name, name, 32) == 0)
		return block;

	block = &hdr->blocks.block_res4;
	if (strncmp(block->block_name, name, 32) == 0)
		return block;

	return block;
}

PART *get_update_part_info(BLOCK *block)
{
	PART *part = NULL;

	switch (block->b_state) {
	case UPDATE:
		if (block->active == PART_A) {
			part = &block->part_a;
			block->active = PART_A;
		} else {
			part = &block->part_b;
			block->active = PART_B;
		}
		break;
	case FAIL:
	case SUCCESS:
		if (block->active == PART_A) {
			part = &block->part_b;
			block->active = PART_B;
		} else {
			part = &block->part_a;
			block->active = PART_A;
		}
		break;
	case UNUSED:
	default:
		break;
	};

	return part;
}

int update_image(struct boot_header *hdr, BLOCK *block, char *file, char *tag)
{
	FILE *fp_in, *fp_out;
	PART *part;
	uint32_t checksum_in, checksum_out;
	size_t len;
	char part_path[PATH_LEN];
	char buf[MB];

	/* Update part info */
	part = get_update_part_info(block);
	if (part == NULL) {
		fprintf(stderr, "Cannot find part\n");
		return -1;
	}
	part->p_state = UPDATE;
	part->retry = 1;
	memset(part->tag, 0, TAG_SIZE);
	if (tag == NULL)
		snprintf(part->tag, 5, "%s", "NONE");
	else
		snprintf(part->tag, strlen(tag) + 1, "%s", tag);
	checksum_in = get_checksum(file);
	snprintf(part_path, PATH_LEN, "%s%d", MMCPART, part->part_n);
	block->b_state = UPDATE;

	/* Write image */
	printf("OTA: Write image(%s) to %s\n", file, part_path);
	if ((fp_in = fopen(file, "rb")) == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", file);
		return -1;
	}

	if ((fp_out = fopen(part_path, "wb")) == NULL) {
		fprintf(stderr, "Error: Cannot open %s\n", part_path);
		fclose(fp_in);
		return -1;
	}

	while ((len = fread(&buf, sizeof(char), MB, fp_in)) != 0) {
		if (fwrite(&buf, sizeof(char), len, fp_out) != len) {
			fclose(fp_in);
			fclose(fp_out);
			return -1;
		}
	}

	fclose(fp_in);
	fclose(fp_out);

	/* Check checksum */
	checksum_out = get_checksum(part_path);
	if (checksum_in != checksum_out) {
		fprintf(stderr, "Checksum mismatched\n");
		return -1;
	}
	part->checksum = checksum_out;

	/* Update FLAG info */
	if (write_boot_header(hdr) < 0) {
		fprintf(stderr, "Write error\n");
		return -1;
	}
	printf("OTA: Updated\n");

	return 0;
}
