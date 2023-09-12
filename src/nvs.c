
/*
 * Copyright (c) 2023 Craig Peacock
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>
#include <zephyr/console/console.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include "nvs.h"

// Set NVS_CLEAR to wipe NVS partition on boot.
//#define NVS_CLEAR

LOG_MODULE_REGISTER(lorawan_nvs, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

const int nVars = 4;
const char *nvs_name[] = {"DevNonce", "DevEUI", "JoinEUI", "AppKey"};
int nvs_len[] = {2, 8, 8, 16};

void nvs_initialise(struct nvs_fs *fs)
{
	struct flash_pages_info info;
	int ret;

	fs->flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs->flash_device)) {
		LOG_ERR("Flash device %s is not ready", fs->flash_device->name);
		return;
	}
	fs->offset = NVS_PARTITION_OFFSET;
	ret = flash_get_page_info_by_offs(fs->flash_device, fs->offset, &info);
	if (ret) {
		LOG_ERR("Unable to get page info");
		return;
	}
	fs->sector_size = info.size;
	fs->sector_count = 3U;

	ret = nvs_mount(fs);
	if (ret) {
		LOG_ERR("Flash Init failed");
		return;
	}

#ifdef NVS_CLEAR
	ret = nvs_clear(fs);
	if (ret) {
		LOG_ERR("Flash Clear failed");
		return;
	} else {
		LOG_ERR("Cleared NVS from flash");
	}
#endif
}

void nvs_read_init_parameter(struct nvs_fs *fs, uint16_t id, void *data)
{
	int ret;

	char *array = (void *)data;
	int *devnonce = (void *)data;

	LOG_INF("NVS ID %d %s: ", id, nvs_name[id]);
	ret = nvs_read(fs, id, data, nvs_len[id]);
	if (ret > 0) { 
		// Item found, print output:
		switch (id) {
			case NVS_DEVNONCE_ID:
				LOG_INF("%d", (uint16_t)*devnonce);
				break;
			case NVS_LORAWAN_DEV_EUI_ID:
			case NVS_LORAWAN_JOIN_EUI_ID:
			case NVS_LORAWAN_APP_KEY_ID:
				for (int i = 0; i < nvs_len[id]; i++)
					LOG_INF("- %02X ",array[i]);
				break;
			default:
				break;
		}
	} else {
		// Item not found
		LOG_WRN("Not found.");
	}
}