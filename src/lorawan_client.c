/*
 * Class A LoRaWAN sample application
 *
 * Copyright (c) 2023 Craig Peacock
 * Copyright (c) 2020 Manivannan Sadhasivam <mani@kernel.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/lorawan/lorawan.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include "openthread/platform/logging.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/link.h"

#include "app.h"
#include "nvs.h"
#include "gpsparser.h"

#include "lorawan_client.h"

#define DELAY K_SECONDS(30)

LOG_MODULE_REGISTER(lorawan_client, CONFIG_LORAWAN_CLIENT_LOG_LEVEL);

static void dl_callback(uint8_t port, bool data_pending, int16_t rssi, int8_t snr, uint8_t len, const uint8_t *data)
{
	LOG_INF("Port %d, Pending %d, RSSI %ddB, SNR %ddBm", port, data_pending, rssi, snr);
	if (data) {
		LOG_HEXDUMP_INF(data, len, "Payload: ");
	}
}

extern struct otInstance *openthread_get_default_instance(void);

static void lorwan_datarate_changed(enum lorawan_datarate dr)
{
	uint8_t unused, max_size;

	lorawan_get_payload_sizes(&unused, &max_size);
	LOG_INF("New Datarate: DR_%d, Max Payload %d", dr, max_size);
}

bool _gnssValid = false;
float _gnssLatitude = 0.0;
float _gnssLongitude = 0.0;
float _gnssElevation = 0.0;
float _gnssSpeed = 0.0;

// TODO: Need a mutex here

void rmc_handler(bool valid, float latitude, float longitude, float speed)
{
	_gnssValid = valid;
	_gnssLatitude = valid ? latitude : 0.0f;
	_gnssLongitude = valid ? longitude: 0.0f;
	_gnssSpeed = valid ? speed : 0.0f;

	LOG_INF("Valid: %d, Latitude: %f, Longitude: %f, Speed: %f",
		valid,
		latitude,
		longitude,
		speed);
}

void gga_handler(float elevation)
{
	_gnssElevation = elevation;

	LOG_INF("Elevation: %f", 
		elevation);
}

int lorawan_client_thread(void)
{
	const struct device *lora_dev;
	static struct nvs_fs fs;
	
	struct lorawan_join_config join_cfg;
	uint16_t dev_nonce = 0;

#ifdef LORAWAN_USE_NVS 
	uint8_t dev_eui[8];
	uint8_t join_eui[8];
	uint8_t app_key[16];
	
#else
	uint8_t dev_eui[8];

    // Get EUI64
    otInstance *instance;
	instance = openthread_get_default_instance();
    otLinkGetFactoryAssignedIeeeEui64(instance, (otExtAddress *)&dev_eui);

	uint8_t join_eui[] = LORAWAN_JOIN_EUI;
	uint8_t app_key[] = LORAWAN_APP_KEY;
#endif

	
	int ret;
	ssize_t bytes_written;

	k_msleep(5000);

	LOG_INF("Zephyr LoRaWAN Client. Board: %s", CONFIG_BOARD);

	nvs_initialise(&fs);
	nvs_read_init_parameter(&fs, NVS_DEVNONCE_ID, &dev_nonce);
#ifdef LORAWAN_USE_NVS 
	nvs_read_init_parameter(&fs, NVS_LORAWAN_DEV_EUI_ID, dev_eui);
	nvs_read_init_parameter(&fs, NVS_LORAWAN_JOIN_EUI_ID, join_eui);
	nvs_read_init_parameter(&fs, NVS_LORAWAN_APP_KEY_ID, app_key);
#endif

	lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
	if (!device_is_ready(lora_dev)) {
		LOG_WRN("%s: device not ready.", lora_dev->name);
		return -1;
	}

	LOG_INF("Starting LoRaWAN stack.");
	ret = lorawan_start();
	if (ret < 0) {
		LOG_WRN("lorawan_start failed: %d", ret);
		return -1;
	}

	// Enable callbacks
	struct lorawan_downlink_cb downlink_cb = {
		.port = LW_RECV_PORT_ANY,
		.cb = dl_callback
	};

	lorawan_register_downlink_callback(&downlink_cb);
	lorawan_register_dr_changed_callback(lorwan_datarate_changed);

	join_cfg.mode = LORAWAN_ACT_OTAA;
	join_cfg.dev_eui = dev_eui;
	join_cfg.otaa.join_eui = join_eui;
	join_cfg.otaa.app_key = app_key;
	join_cfg.otaa.nwk_key = app_key;
	join_cfg.otaa.dev_nonce = dev_nonce;

	int i = 1;

	LOG_INF("DevEUI: %02x%02x%02x%02x%02x%02x%02x%02x",
            dev_eui[0],
            dev_eui[1],
            dev_eui[2],
            dev_eui[3],
            dev_eui[4],
            dev_eui[5],
            dev_eui[6],
            dev_eui[7]
            );

	do {
		LOG_INF("Joining network using OTAA, dev nonce %d, attempt %d: ", join_cfg.otaa.dev_nonce, i++);
		ret = lorawan_join(&join_cfg);
		if (ret < 0) {
			if ((ret =-ETIMEDOUT)) {
				LOG_WRN("Timed-out waiting for response.");
			} else {
				LOG_WRN("Join failed (%d)", ret);
			}
		} else {
			LOG_INF("Join successful.");
		}

		// Increment DevNonce as per LoRaWAN 1.0.4 Spec.
		dev_nonce++;
		join_cfg.otaa.dev_nonce = dev_nonce;
		// Save value away in Non-Volatile Storage.
		bytes_written = nvs_write(&fs, NVS_DEVNONCE_ID, &dev_nonce, sizeof(dev_nonce));
		if (bytes_written < 0) {
			LOG_WRN("NVS: Failed to write id %d (%d)", NVS_DEVNONCE_ID, bytes_written);
		} else {
			//printf("NVS: Wrote %d bytes to id %d\n",bytes_written, NVS_DEVNONCE_ID);
		}

		if (ret < 0) {
			// If failed, wait before re-trying.
			k_sleep(K_MSEC(5000));
		}

	} while (ret != 0);

#ifdef LORAWAN_CLASS_C
	printk("Setting device to Class C");
	ret = lorawan_set_class(LORAWAN_CLASS_C);
	if (ret != 0) {
		LOG_WRN("Failed to set LoRaWAN class: %d", ret);
	}
#endif

	int count = 0;
    enum TriageStatus triage_status = P0;
	int battery_percentage = 100;

	// Set GNSS callback
	set_callback_rmc(rmc_handler);

	while (1) {

#define LORAWAN_PORT 2
#define PAYLOAD_SIZE 16
		uint8_t payload[PAYLOAD_SIZE];

		// Build test payload format here - keep it similar to OpenThread payload
        // Byte 0 - \"Version\":\"%s\", 
		payload[0] = VERSION;

		// Byte 1 - \"Count\":%d, 
		payload[1] = count++;

		// Byte 2 - \"Status\":\"%s\", 
		payload[2] = triage_status;

		// Byte 3 - \"Battery\":%d, 
		payload[3] = battery_percentage;

		// Byte 4 - Bits - bit0 GPSlock
		payload[4] = _gnssValid ? 0x01: 0x00;

		// Byte 5 .. 8 \"Latitude\":%d, 
		*((float *)&payload[5]) = _gnssLatitude;

		// Byte 9 .. 12 \"Longitude\":%d, 
		*((float *)&payload[9]) = _gnssLongitude;

		// Byte 13 .. \"Elevation\":%d,
		payload[13] = (int)_gnssElevation;

		// Byte 14 \"Speed\":%d, 
		payload[14] = (int)_gnssSpeed;

		// Byte 15 - \"Temperature\":%d.%02u }";
		payload[15] = whole_celsius;

		ret = lorawan_send(LORAWAN_PORT, payload, PAYLOAD_SIZE, LORAWAN_MSG_UNCONFIRMED);
		if (ret == -EAGAIN) {
			LOG_ERR("lorawan_send failed: %d. Continuing...", ret);
			k_sleep(DELAY);
			continue;
		} else if (ret < 0) {
			LOG_WRN("lorawan_send failed: %d", ret);
//			return -1;
		}
		else {
			LOG_INF("Data sent!");
		}
		k_sleep(DELAY);
	}

	return 0;
}

K_THREAD_DEFINE(lorawan_client_id, 2048, lorawan_client_thread, NULL, NULL, NULL,
		7, 0, 0);