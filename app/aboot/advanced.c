#include <sys/types.h>
#include <debug.h>
#include <mmc.h>
#include <partition_parser.h>
#include <target.h>
#include <stdlib.h>
#include <app/aboot/devinfo.h>
#include <sdhci_msm.h>
#include <dev/fbcon.h>
#include <qtimer.h>
#include "board.h"

int get_storage_speed(uint32_t data_len, uint32_t buf_size, int64_t skip_blk)
{
	int ret = 0;
	uint64_t ptn, lba;
	uint64_t ptn_size;
	uint32_t block_size;	
	uint32_t blk_count;
	uint32_t total_count;
	struct mmc_device * dev = target_mmc_device();
	void * buf;
	uint64_t t0, t1, speed;

	block_size = mmc_get_device_blocksize();

	if (buf_size < 8*1024)
		buf_size = 8*1024;

	if (buf_size > SDHCI_ADMA_MAX_TRANS_SZ)
		buf_size = SDHCI_ADMA_MAX_TRANS_SZ;

	if (buf_size % block_size)
		buf_size = (buf_size / block_size) * block_size;

	buf = memalign(CACHE_LINE, ROUNDUP(buf_size, CACHE_LINE));
	if (!buf)
		return -2;

	if (data_len == 0)
		data_len = dev->card.capacity; // Scan full card

	if (data_len % buf_size)
		data_len = (data_len / buf_size) * buf_size;

	ptn = 0;
	if (dev->card.capacity > data_len) {
		if (skip_blk < 0) {
			ptn = dev->card.capacity - data_len;
		}
		ptn += skip_blk * block_size;
	}
	ptn /= block_size;
	blk_count = buf_size / block_size;
	total_count = data_len / buf_size;

	if (sdhci_wait_for_cmd(&dev->host, 10))
		return -5;

	lba = ptn;
	t0 = qtimer_get_phy_timer_cnt();	
	while (total_count && !ret) {
		ret = mmc_sdhci_read(dev, buf, ptn, blk_count);
		ptn += blk_count;
		total_count--;
	}
	t1 = qtimer_get_phy_timer_cnt();

	if (ret) {
		dprintf(CRITICAL, "Failed Reading block @ 0x%llx (ret = %d) \n", ptn, ret);
		ret = -6;
		goto exit;
	}

	if (t0 > t1) {
		t0 = (QTMR_PHY_CNT_MAX_VALUE - t0) + t1;
	} else {
		t0 = t1 - t0;
	}

	speed = ((uint64_t)data_len * qtimer_tick_rate()) / t0;
	if (speed > INT_MAX) {
		ret = INT_MAX;
	} else {
		ret = (int)speed;
	}

#if DEBUGLEVEL >= SPEW
	{
		uint32_t x = (uint32_t)speed / 1024;
		uint64_t ms = t0 * 1000;
		uint64_t us = (ms * 1000) / qtimer_tick_rate();
		ms /= qtimer_tick_rate();
		us -= ms * 1000;
		x = (x * 10) / 1024;
		_dprintf("%s: speed = %u.%u MB/s , time = %u.%03u ms, LBA = 0x%llx, data_len = %u, buf_size = %u \n", __func__,
			x / 10, x % 10, (uint32_t)ms, (uint32_t)us, lba, data_len, buf_size);
	}
#endif

exit:
	free(buf);
	return ret;
}

unsigned get_color_by_speed(int storage, int speed)
{
	switch (storage) {
		case EMMC_CARD:
			if (speed < 40 * MB)
				return RED;
			if (speed < 50 * MB)
				return YELLOW;
			break;
		case SD_CARD:
			if (speed < 10 * MB)
				return RED;
			if (speed < 20 * MB)
				return YELLOW;
			break;
	}
	return GREEN;
}

void print_speed(int line, int storage, int speed)
{
	int x, x1, x2;

	fbcon_set_bg(BLACK, 0, line, -1, 1);
	if (speed > 0) {
		x = speed / 1024;
		x = (x * 10) / 1024;
		x1 = x / 10;
		x2 = x % 10;
	} else {
		x1 = speed;
		x2 = 0;
	}
	fbcon_acprintf(line, ALIGN_CENTER, get_color_by_speed(storage, speed),
		"%s: Read: %d.%d MiB/s", get_slot_name(storage), x1, x2);
}

void test_storage_read_speed(int storage, bool full_scan)
{
	int speed = 0;
	int speed2 = 0;

	dprintf(SPEW, "%s: entered\n", __func__);
	struct mmc_device *dev = target_mmc_device();
	int slot = dev->config.slot;

	if (!target_sdc_init_slot(storage)) {
		dprintf(CRITICAL, "%s: Unable to init storage int slot %d\n", __func__, storage);
		fbcon_set_bg(BLACK, 0, 1, -1, 1);
		fbcon_acprintf(1, ALIGN_CENTER, RED, "%s: Init failed!", get_slot_name(storage));
		return;
	}

	fbcon_set_bg(BLACK, 0, 1, -1, 1);
	fbcon_set_bg(BLACK, 0, 2, -1, 1);
	fbcon_acprintf(2, ALIGN_CENTER, TEAL, "%s: Testing read speed", get_slot_name(storage));

	if (full_scan) {
		fbcon_hprint("Please wait up to 30 min", GREEN);
		speed = get_storage_speed(0, 4 * MB, 0);
	} else {
		speed = get_storage_speed(4 * MB, 4 * MB, -128); // 4MiB data and 4MiB buffer is best settings for d10f
		speed2 = get_storage_speed(4 * MB, 4 * MB, -65736);
	}

	print_speed(2, storage, speed);
	if (speed2)
		print_speed(1, storage, speed2);

	dprintf(SPEW, "%s: done, speed=%d, speed2=%d\n", __func__, speed, speed2);

	target_sdc_init_slot(slot);

}