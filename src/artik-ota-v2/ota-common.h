#ifndef _OTA_COMMON_H
#define _OTA_COMMON_H

int is_ota_support(void);
int is_rescue(void);
int show_flag(struct boot_header *hdr);
int read_boot_header(struct boot_header *hdr);
int update_boot_state(struct boot_header *hdr, BLOCK *block, uint8_t state);
int update_image(struct boot_header *hdr, BLOCK *block,
			const char *file, const char *tag);
BLOCK *get_block_info(struct boot_header *hdr, const char *name);
PART *get_update_part_info(BLOCK *block);
#endif
