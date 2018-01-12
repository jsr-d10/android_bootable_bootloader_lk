#include <debug.h>
#include <stdlib.h>
#include <target.h>
#include <mmc_sdhci.h>
#include <sdhci_msm.h>
#include <sys/types.h>
#include <partition_parser.h>
#include <app/aboot/multiboot.h>

// GPT can have 128 (NUM_PARTITIONS) max, we have 26 partitions used for primary boot slot
// Each boot slot uses 4 partitions (boot, recovery, system, userdata)
// So, we may have up to (128-26)/4 == 25 more slots for mulitbooting
#define MAX_BOOT_SLOTS (26)
static int slots_count = 1;
static int slots[MAX_BOOT_SLOTS+1] = { false };

struct gpt_entry {
	unsigned char type_guid[PARTITION_TYPE_GUID_SIZE];
	unsigned char unique_partition_guid[UNIQUE_PARTITION_GUID_SIZE];
	unsigned long long first_lba;
	unsigned long long last_lba;
	unsigned long long attribute_flag;
	unsigned char name[MAX_GPT_NAME_SIZE];
};

struct GUID {
	uint32_t  Data1;
	uint16_t  Data2;
	uint16_t  Data3;
	uint8_t   Data4[8];
};

static const struct GUID EXT4_PARTITION_GUID =
{ 0xEBD0A0A2, 0xB9E5, 0x4433, {0x87, 0xc0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7} };

static const struct GUID UNUSED_PARTITION_GUID =
{ 0xDEADBEEF, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };

static const struct GUID BOOT_IMAGE_PARTITION_GUID =
{ 0x20117F86, 0xE985, 0x4357, {0xB9, 0xEE, 0x37, 0x4B, 0xC1, 0xD8, 0x48, 0x7D} };

static int multiboot_is_slots_array_populated(void) {
	return slots[0];
}

int multiboot_check_for_partition(char *name) {
	int index = partition_get_index(name);
	uint64_t ptn = partition_get_offset(index);
	if(ptn == 0) {
// 		dprintf(SPEW, "%s: partition '%s' have index %d, not valid\n", __func__, name, index);
		return false;
	}
	dprintf(SPEW, "%s: partition '%s' have index %d\n", __func__, name, index);
	return true;
}

static void mulitiboot_populate_slots_array(void) {
	int res = false;
	struct mmc_device *dev = target_mmc_device();
	int current_sdcc = dev->config.slot;
	dprintf(SPEW, "%s: entered\n", __func__);
	if (!target_sdc_init_slot(SD_CARD)) {
		dprintf(CRITICAL, "%s: cannot init SD card sdcc\n", __func__);
		goto end;
	}

	// iterate through possible slots
	slots[0] = false;
	int current_slot = 1;
	slots_count = 1;
	char name[16];
	while (current_slot <= MAX_BOOT_SLOTS) {
		snprintf(name, 16, "system%d", current_slot);
		if (multiboot_check_for_partition(name)) {
			++slots_count;
			slots[current_slot] = true;
			dprintf(SPEW, "%s: found slot #%d: '%s'\n", __func__, slots_count, name);
		} else {
			slots[current_slot] = false;
		}
		++current_slot;
	}
	slots[0] = true;

	end:
	target_sdc_init_slot(current_sdcc);
	return;
}

static uint32_t multiboot_get_suitable_buffer_size(int buf_size) {
	uint32_t block_size = mmc_get_device_blocksize();

  if (buf_size < 8*1024)
		buf_size = 8*1024;

  if (buf_size > SDHCI_ADMA_MAX_TRANS_SZ)
		buf_size = SDHCI_ADMA_MAX_TRANS_SZ;

  if (buf_size % block_size)
		buf_size = (buf_size / block_size) * block_size;
	dprintf(SPEW, "%s: buf_size = %d, CACHE_LINE = %d, block_size=%d\n", __func__, buf_size, CACHE_LINE, block_size);
	return buf_size;
}

static void multiboot_dump_buffer_hex(void *buffer, int len) {
	int pos = 0;
	char *buf = (char*)buffer;
	char out[70] = {0};
	char tmp[70] = {0};
	while (pos < len) {
		memset(out, 0, sizeof(out));
		for (int i = 0; i < 32; i++) {
			strncpy(tmp, out, sizeof(tmp));
			snprintf(out, sizeof(out), "%s%02x", tmp, buf[pos++]);
			if (pos >= len) {
				break;
			}
		}
		dprintf(SPEW, "%s: %s\n", __func__, out);
	}
}

static void multiboot_dump_buffer_ascii(const void *buffer, int len) {
	int pos = 0;
	char *buf = (char*)buffer;
	char buf2[65];
	while (pos < len) {
		memset(buf2, 0, sizeof(buf2));
		for (int i = 0; i < 64; i++) {
			buf2[i] = buf[pos++];
			if (buf2[i] < 32) {
				buf2[i] = '.';
			}
			if (pos >= len) {
				break;
			}
		}
		dprintf(SPEW, "%s: %s\n", __func__, buf2);
	}
}

// returns char * to newly allocated memory block or NULL for error, must be passed to free() after use
static char* multiboot_gpt_get_name(struct gpt_entry entry) {
	char *ret = calloc(1, MAX_GPT_NAME_SIZE/2 + 1);
	if (!ret) {
		dprintf(CRITICAL, "%s: malloc failed!\n", __func__);
		return ret;
	}
	int n;
	for (n = 0; n < MAX_GPT_NAME_SIZE / 2; n++) {
		ret[n] = entry.name[n * 2];
	}
	ret[n] = '\0';
	return ret;
}

static void multiboot_gpt_set_name(struct gpt_entry *entry, const char *name) {
	memset(entry->name, 0, MAX_GPT_NAME_SIZE);
	int len = strlen(name);
	for (int n = 0; n < len; n++) {
		entry->name[n * 2] = name[n];
		entry->name[n * 2 + 1] = '\0';
	}
}

static void multiboot_gpt_set_guid(struct gpt_entry *entry, const struct GUID guid) {
		memcpy(entry->type_guid, &guid, PARTITION_TYPE_GUID_SIZE);
}

// returns void * to newly allocated memory block or NULL for error, must be passed to free() after use
static void *multiboot_read_lba(uint64_t start_lba, uint32_t size_bytes) {
	dprintf(SPEW, "%s: entered(start_lba=%llu, size_bytes=%d\n", __func__, start_lba, size_bytes);
	uint32_t buf_size = multiboot_get_suitable_buffer_size(size_bytes);
	uint32_t block_size = mmc_get_device_blocksize();

	void *buf = memalign(CACHE_LINE, ROUNDUP(buf_size, CACHE_LINE));
	if (!buf)
		return buf;

	int ret = mmc_read(start_lba * block_size, buf, buf_size);
	if (ret) {
		struct mmc_device *dev = target_mmc_device();
		int slot = dev->config.slot;
		dprintf(CRITICAL, "%s: mmc_read(%llu, size=%d) from slot %d failed (ret = %d) \n",
		        __func__, start_lba * block_size, buf_size, slot, ret);
		free(buf);
		buf = NULL;
	}

	dprintf(SPEW, "%s: finished\n", __func__);
	return buf;
}

static int multiboot_write_lba(uint64_t start_lba, uint32_t size_bytes, void *data) {
	dprintf(SPEW, "%s: entered(start_lba=%llu, size_bytes=%d\n", __func__, start_lba, size_bytes);
	uint32_t buf_size = multiboot_get_suitable_buffer_size(size_bytes);
	uint32_t block_size = mmc_get_device_blocksize();

	void *buf = memalign(CACHE_LINE, ROUNDUP(buf_size, CACHE_LINE));
	if (!buf) {
		dprintf(CRITICAL, "%s: memalign() failed\n", __func__);
		return false;
	}
	memcpy(buf, data, size_bytes);

	int ret = mmc_write(start_lba * block_size, buf_size, buf);
	if (ret) {
		dprintf(CRITICAL, "%s: mmc_write() failed (ret = %d) \n", __func__, ret);
	}

	dprintf(SPEW, "%s: finished\n", __func__);
	free(buf);
	return ret;
}

static int multiboot_get_partition_index(
	struct gpt_entry *entries,
	int entry_size,
	int entry_count,
	const char *name) {
// 	dprintf(SPEW, "%s: entered for name=%s\n", __func__, name);
	for (int index = 0; index < entry_count; ++index) {
		char *pname = multiboot_gpt_get_name(entries[index]);
// 		dprintf(SPEW, "%s: pname=%s for index %d\n", __func__, pname, index);
		if (pname && (strcmp(name, pname) == 0)) {
			free(pname);
			return index;
		}
		free(pname);
	}
	return -1;
}

// returns 0 for success, 1 for failure
static int multiboot_set_slot_for_partition(
	struct gpt_entry *entries,
	int entry_size,
	int entry_count,
	int new_slot,
	struct GUID guid,
	const char *basename) {
	char name[16];
	int index = multiboot_get_partition_index(entries, entry_size, entry_count, basename);
	if(index < 0) {
		dprintf(CRITICAL, "%s: cannot get partition index for name %s\n", __func__, name);
		return 1;
	}
	snprintf(name, sizeof(name), "%s%d", basename, multiboot_get_active_slot());
	dprintf(SPEW, "%s: index=%d, name=%s\n", __func__, index, name);
	multiboot_gpt_set_name(&entries[index], name);
	multiboot_gpt_set_guid(&entries[index], UNUSED_PARTITION_GUID);

	snprintf(name, sizeof(name), "%s%d", basename, new_slot);
	index = multiboot_get_partition_index(entries, entry_size, entry_count, name);
	if(index < 0) {
		dprintf(CRITICAL, "%s: can't get partition index for name %s\n", __func__, name);
		return 1;
	}
	dprintf(SPEW, "%s: index=%d, name=%s\n", __func__, index, name);
	multiboot_gpt_set_name(&entries[index], basename);
	multiboot_gpt_set_guid(&entries[index], guid);
	return 0;
}

static int multiboot_update_partitions(
	struct gpt_entry *entries,
	int entry_size,
	int entry_count,
	int new_slot) {
	dprintf(SPEW, "%s: entered\n", __func__);
	int res = 0;

	res += multiboot_set_slot_for_partition(entries, entry_size, entry_count, new_slot,
	                                        BOOT_IMAGE_PARTITION_GUID, "boot");
	res += multiboot_set_slot_for_partition(entries, entry_size, entry_count, new_slot,
	                                        BOOT_IMAGE_PARTITION_GUID, "recovery");
	res += multiboot_set_slot_for_partition(entries, entry_size, entry_count, new_slot,
	                                        EXT4_PARTITION_GUID, "system");
	res += multiboot_set_slot_for_partition(entries, entry_size, entry_count, new_slot,
	                                        EXT4_PARTITION_GUID, "userdata");

	dprintf(SPEW, "%s: finished, res=%d\n", __func__, res);
	return res == 0;
}

static int multiboot_update_gpt(int new_slot) {
	dprintf(SPEW, "%s: entered\n", __func__);
	int res = false;
	uint32_t block_size = mmc_get_device_blocksize();
	uint8_t *data = NULL;
	uint8_t *header = multiboot_read_lba(1, block_size);
	if (!header) {
		dprintf(CRITICAL, "%s: failed to read primary GPT header\n", __func__);
		return false;
	}
// 	multiboot_dump_buffer_hex(header, block_size);
	unsigned long long first_usable_lba;
	unsigned int gpt_entry_size;
	unsigned int header_size;
	unsigned int max_partition_count = 0;
	int ret = partition_parse_gpt_header(header, &first_usable_lba,
	                                     &gpt_entry_size, &header_size,
	                                     &max_partition_count);
	if (ret) {
		dprintf(CRITICAL, "%s: primary GPT header signature is invalid, aborting\n", __func__);
		goto exit;
	}
	dprintf(SPEW, "%s: primary GPT header signature is valid\n", __func__);

	unsigned int entries_lba = GET_LWORD_FROM_BYTE(&header[PARTITION_ENTRIES_OFFSET]);
	unsigned int partitions_count = GET_LWORD_FROM_BYTE(&header[PARTITION_COUNT_OFFSET]);
	dprintf(SPEW, "%s: primary GPT has %d partitions\n", __func__, partitions_count);
	unsigned int data_len = gpt_entry_size * partitions_count;
	data = multiboot_read_lba(entries_lba, data_len);
// 	multiboot_dump_buffer_hex(data, data_len);
	if (!data) {
		dprintf(CRITICAL, "%s: failed to read primary GPT data\n", __func__);
		goto exit;
	}
	unsigned int crc = calculate_crc32(data, data_len);
	unsigned int data_crc = GET_LWORD_FROM_BYTE(&header[PARTITION_CRC_OFFSET]);
	if (crc != data_crc) {
		dprintf(CRITICAL, "%s: primary GPT data CRC is invalid (0x%x vs 0x%x), aborting\n",
		        __func__, crc, data_crc);
		goto exit;
	}
	dprintf(SPEW, "%s: primary GPT data CRC is valid (0x%x)\n", __func__, crc);

	unsigned int header_crc = GET_LWORD_FROM_BYTE(&header[HEADER_CRC_OFFSET]);
	PUT_LONG((unsigned char *)header + HEADER_CRC_OFFSET, 0);
	crc = calculate_crc32(header, header_size);
	if (crc != header_crc) {
		dprintf(CRITICAL, "%s: primary GPT header CRC is invalid (0x%x vs 0x%x), aborting\n",
		        __func__, crc, header_crc);
		goto exit;
	}
	dprintf(SPEW, "%s: primary GPT header CRC is valid (0x%x)\n", __func__, crc);

	ret = multiboot_update_partitions((struct gpt_entry *)data, gpt_entry_size, partitions_count, new_slot);
	if(!ret) {
		dprintf(CRITICAL, "%s: failed to update GPT entries in RAM, aborting\n", __func__);
		goto exit;
	}

	data_crc = calculate_crc32(data, data_len);
	PUT_LONG((unsigned char *)header + PARTITION_CRC_OFFSET, data_crc);
	crc = calculate_crc32(header, header_size);
	PUT_LONG((unsigned char *)header + HEADER_CRC_OFFSET, crc);
	multiboot_write_lba(1, block_size, header);
	multiboot_write_lba(entries_lba, data_len, data);

	// Ugly workaround for buggy sdcards
	res = true;
	goto exit;

	uint64_t backup_header_offset = GET_LWORD_FROM_BYTE(&header[BACKUP_HEADER_OFFSET]);
	free(header);
	header = multiboot_read_lba(backup_header_offset, block_size);
	if (!header) {
		dprintf(CRITICAL, "%s: failed to read backup GPT header\n", __func__);
		goto exit;
	}
	ret = partition_parse_gpt_header(header, &first_usable_lba,
	                                 &gpt_entry_size, &header_size,
	                                 &max_partition_count);
	if (ret) {
		dprintf(CRITICAL, "%s: backup GPT header signature is invalid, aborting\n", __func__);
		goto exit;
	}
	dprintf(SPEW, "%s: backup GPT header signature is valid\n", __func__);
	header_crc = GET_LWORD_FROM_BYTE(&header[HEADER_CRC_OFFSET]);
	PUT_LONG((unsigned char *)header + HEADER_CRC_OFFSET, 0);
	crc = calculate_crc32(header, header_size);
	if (crc != header_crc) {
		dprintf(CRITICAL, "%s: backup GPT header CRC is invalid (0x%x vs 0x%x), aborting\n",
		        __func__, crc, header_crc);
		goto exit;
	}
	dprintf(SPEW, "%s: backup GPT header CRC is valid (0x%x)\n", __func__, crc);

	PUT_LONG((unsigned char *)header + PARTITION_CRC_OFFSET, data_crc);
	crc = calculate_crc32(header, header_size);
	PUT_LONG((unsigned char *)header + HEADER_CRC_OFFSET, crc);
	entries_lba = GET_LWORD_FROM_BYTE(&header[PARTITION_ENTRIES_OFFSET]);
	multiboot_write_lba(backup_header_offset, block_size, header);
	multiboot_write_lba(entries_lba, data_len, data);

	res = true;
	exit:
	free(header);
	free(data);
	return res;
}

void multiboot_set_active_slot(int new_slot) {
	struct mmc_device *dev = target_mmc_device();
	int current_sdcc = dev->config.slot;

	dprintf(SPEW, "%s: entered\n", __func__);
	if (!target_sdc_init_slot(SD_CARD)) {
		dprintf(CRITICAL, "%s: cannot init SD card sdcc\n", __func__);
		goto end;
	}

	dprintf(SPEW, "%s: entered (new_slot=%d)\n", __func__, new_slot);
	if (new_slot != multiboot_get_active_slot()) {
		multiboot_update_gpt(new_slot);
		partition_read_table();
		mulitiboot_populate_slots_array();
	}

	end:
	target_sdc_init_slot(current_sdcc);
	dprintf(SPEW, "%s: finished\n", __func__);
}

// returns -1 in case if all 26 slots are busy
int multiboot_get_active_slot(void) {
	if (!multiboot_is_slots_array_populated()) {
		mulitiboot_populate_slots_array();
	}
	int current_slot = 1;
	while(current_slot <= MAX_BOOT_SLOTS) {
		if (!slots[current_slot]) {
			dprintf(SPEW, "%s: active slot is %d\n", __func__, current_slot);
			return current_slot;
		}
		dprintf(SPEW, "%s: slot %d is not active\n", __func__, current_slot);
		current_slot++;
	}
	return -1;
}

int multiboot_get_slots_count(void) {
	if (!multiboot_is_slots_array_populated()) {
		mulitiboot_populate_slots_array();
	}
	int res = slots_count;
	dprintf(SPEW, "%s: exiting with res=%d\n", __func__, res);
	return res;
}

int multiboot_is_available(void) {
	if (!multiboot_is_slots_array_populated()) {
		mulitiboot_populate_slots_array();
	}
	int res = slots_count > 1 ? true : false;
	dprintf(SPEW, "%s: exiting with res=%d\n", __func__, res);
	return res;
}
