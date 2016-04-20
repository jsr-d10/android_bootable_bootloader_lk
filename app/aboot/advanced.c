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

int get_storage_speed(uint32_t data_len, uint32_t buf_size)
{
	int ret = 0;
	int index;
	uint64_t ptn;
	uint64_t ptn_size;
	uint32_t block_size;	
	uint32_t blk_count;
	uint32_t total_count;
	struct mmc_device * dev;
	void * buf;
	uint64_t t0, t1, speed;

	dev = target_mmc_device();
	if (!dev)
		return -1;

	block_size = mmc_get_device_blocksize();

	if (!buf_size)
		buf_size = 16*1024;

	if (buf_size > SDHCI_ADMA_MAX_TRANS_SZ)
		buf_size = SDHCI_ADMA_MAX_TRANS_SZ;

	if (buf_size % block_size)
		buf_size = (buf_size / block_size) * block_size;

	buf = memalign(CACHE_LINE, ROUNDUP(buf_size, CACHE_LINE));
	if (!buf)
		return -2;

	if (data_len % buf_size)
		data_len = (data_len / buf_size) * buf_size;

	ptn = dev->card.capacity - data_len - (block_size * 128);
	ptn /= block_size;
	blk_count = buf_size / block_size;
	total_count = data_len / buf_size;

	if (sdhci_wait_for_cmd(&dev->host, 10))
		return -5;

	t0 = qtimer_get_phy_timer_cnt();	
	while (total_count && !ret) {
		ret = mmc_sdhci_read(dev, buf, ptn, blk_count);
		ptn += blk_count;
		total_count--;
	}
	t1 = qtimer_get_phy_timer_cnt();

	if (ret) {
		dprintf(CRITICAL, "Failed Reading block @ %llux (ret = %d) \n", ptn, ret);
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
		uint32_t KiB = (uint32_t)speed / 1024;
		uint32_t MiB = KiB / 1024;
		uint64_t ms = t0 * 1000;
		uint64_t us = (ms * 1000) / qtimer_tick_rate();
		ms /= qtimer_tick_rate();
		us -= ms * 1000;
		KiB -= MiB * 1024;
		_dprintf("%s: speed = %u.%03u MB/s , time = %u.%03u ms, data_len = %u, buf_size = %u \n", __func__,
			MiB, KiB, (uint32_t)ms, (uint32_t)us, data_len, buf_size);
	}
#endif

exit:
	free(buf);
	return ret;
}

void test_storage_read_speed(int storage)
{
	struct mmc_device *mmc_dev = target_mmc_device();
	int slot = SD_CARD;
	int speed = 0;
	int KiB = 0;
	int MiB = 0;

	dprintf(SPEW, "%s: entered\n", __func__);

	if (mmc_dev)
		slot = mmc_dev->config.slot;

	if (!target_sdc_init_slot(storage)) {
		dprintf(CRITICAL, "%s: Unable to init storage int slot %d\n", __func__, storage);
		fbcon_set_bg(BLACK, 0, 1, -1, 1);
		fbcon_acprintf(1, ALIGN_CENTER, RED, "%s: Init failed!", storage == EMMC_CARD ? "eMMC" : "SD");
		return;
	}

	fbcon_set_bg(BLACK, 0, 2, -1, 1);
	fbcon_acprintf(2, ALIGN_CENTER, TEAL, "%s: Testing read speed", storage == EMMC_CARD ? "eMMC" : "SD");

	speed = get_storage_speed(4 * MB, 4 * MB); // 4MiB data and 4MiB buffer is best settings for d10f
	if (speed >= 0) {
		KiB = speed / 1024;
		MiB = KiB / 1024;
		KiB -= MiB * 1024;
	} else {
		MiB = speed;
	}

	unsigned color = GREEN;
	switch (storage) {
		case EMMC_CARD:
			if (MiB < 78)
				color=YELLOW;
			if (MiB < 72)
				color=RED;
			break;
		case SD_CARD:
			if (MiB < 20)
				color=YELLOW;
			if (MiB < 10)
				color=RED;
			break;
		default:
			break;
	}
	
	fbcon_set_bg(BLACK, 0, 2, -1, 1);
	fbcon_acprintf(2, ALIGN_CENTER, color, "%s: Read: %d.%03d MiB/s", storage == EMMC_CARD ? "eMMC" : "SD", MiB, KiB);

	dprintf(SPEW, "%s: done, speed=%d\n", __func__, speed);

	target_sdc_init_slot(slot);

}