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
#include <zephyr/sys/reboot.h>

#include "openthread/platform/logging.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/link.h"

#include "app.h"
#include "nvs.h"
#include "gpsparser.h"

#include "lorawan_client.h"

// #define DELAY K_SECONDS(30)
//#warning DEBUG - FAST DELAY INCLUDING 8s for LoRa OTA send
#define DELAY_MSG_S K_SECONDS((60 - 8))
#define DELAY_JOIN_S K_SECONDS(10)
#define DELAY_RESEND_S K_SECONDS(10)

LOG_MODULE_REGISTER(lorawan_client, CONFIG_LORAWAN_CLIENT_LOG_LEVEL);


void LoRaMacTestSetDutyCycleOn(bool enable);

static void dl_callback(uint8_t port, bool data_pending, int16_t rssi, int8_t snr, uint8_t len, const uint8_t *data)
{
	LOG_INF("Port %d, Pending %d, RSSI %ddB, SNR %ddBm", port, data_pending, rssi, snr);
	if (data)
	{
		LOG_HEXDUMP_INF(data, len, "Payload: ");
	}
}

extern struct otInstance *openthread_get_default_instance(void);

static void lorawan_datarate_changed(enum lorawan_datarate dr)
{
	uint8_t unused, max_size;

	lorawan_get_payload_sizes(&unused, &max_size);
	LOG_INF("New Datarate: DR_%d, Max Payload %d", dr, max_size);
}

uint8_t get_battery_level(void)
{
    // Placeholder implementation
    // Replace with actual code to read battery level
    uint8_t battery_level = 100; // Assuming battery is fully charged
    return battery_level;
}

void rmc_handler(int fix_type, float latitude, float longitude, float altitude)
{
	// Impelement callback - currently just polling
}

int lorawan_client_thread(void)
{
	const struct device *lora_dev;
	static struct nvs_fs fs;

	struct lorawan_join_config join_cfg;
	uint16_t dev_nonce = 0;
	uint8_t join_fail_count = 0;

#ifdef LORAWAN_USE_NVS
	uint8_t dev_eui[8];
	uint8_t join_eui[8];
	uint8_t app_key[16];
	uint8_t nwk_key[16];

#else
	uint8_t dev_eui[8];
	uint8_t join_eui[] = LORAWAN_JOIN_EUI;
	uint8_t app_key[] = LORAWAN_APP_KEY;
	uint8_t nwk_key[] = LORAWAN_NWK_KEY;

	// Get EUI64
	otInstance *instance;
	instance = openthread_get_default_instance();
	otLinkGetFactoryAssignedIeeeEui64(instance, (otExtAddress *)&dev_eui);

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
	nvs_read_init_parameter(&fs, NVS_LORAWAN_NWK_KEY_ID, nwk_key);
#endif

	lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
	if (!device_is_ready(lora_dev))
	{
		LOG_WRN("%s: device not ready.", lora_dev->name);
		return -1;
	}

	// Set duty cycle off
	LoRaMacTestSetDutyCycleOn( false );

	LOG_INF("Starting LoRaWAN stack.");
	ret = lorawan_start();
	if (ret < 0)
	{
		LOG_WRN("lorawan_start failed: %d", ret);
		return -1;
	}

	// Enable callbacks
	struct lorawan_downlink_cb downlink_cb = {
		.port = LW_RECV_PORT_ANY,
		.cb = dl_callback};

	lorawan_register_downlink_callback(&downlink_cb);
	lorawan_register_dr_changed_callback(lorawan_datarate_changed);
//	lorawan_register_battery_level_callback(get_battery_level);
	lorawan_enable_adr(false);

	join_cfg.mode = LORAWAN_ACT_OTAA;
	join_cfg.dev_eui = dev_eui;
	join_cfg.otaa.join_eui = join_eui;
	join_cfg.otaa.app_key = app_key;
	join_cfg.otaa.nwk_key = nwk_key;
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
			dev_eui[7]);

	// Set duty cycle again as we get duty cycle restricted join failures here?
	LoRaMacTestSetDutyCycleOn( false );

	do
	{
		LOG_INF("Joining network using OTAA, dev nonce %d, attempt %d: ", join_cfg.otaa.dev_nonce, i++);
		ret = lorawan_join(&join_cfg);
		if (ret < 0) {
			if ((ret =-ETIMEDOUT)) {
				LOG_WRN("Timed-out waiting for response.");
			} else {
				LOG_WRN("Join failed (error %d) (count %d)", ret, join_fail_count);
			}

			// Even when we have set the duty cycle off we can fail with join duty cycle restriction.
			// If we fail due to restricted duty cycle we'll see a timeout error. (!)
			if(++join_fail_count > 5) {
				LOG_ERR("Join failed too many times. Rebooting.");
				k_sleep(K_SECONDS(3));
				sys_reboot(SYS_REBOOT_WARM);
			}

			LOG_DBG("Join Sleep.");
			k_sleep(DELAY_JOIN_S);
			LOG_DBG("Slept.");
			continue;

		} else {
			LOG_INF("Join successful.");
		}

		// Increment DevNonce as per LoRaWAN 1.0.4 Spec.
		dev_nonce++;
		join_cfg.otaa.dev_nonce = dev_nonce;
		// Save value away in Non-Volatile Storage.
		bytes_written = nvs_write(&fs, NVS_DEVNONCE_ID, &dev_nonce, sizeof(dev_nonce));
		if (bytes_written < 0)
		{
			LOG_WRN("NVS: Failed to write id %d (%d)", NVS_DEVNONCE_ID, bytes_written);
		}
		else
		{
			LOG_DBG("NVS: Wrote %d bytes to id %d", bytes_written, NVS_DEVNONCE_ID);
		}

		if (ret < 0)
		{
			// If failed, wait before re-trying.
			k_sleep(K_MSEC(5000));
		}

	} while (ret != 0);

#ifdef LORAWAN_CLASS_C
	printk("Setting device to Class C");
	ret = lorawan_set_class(LORAWAN_CLASS_C);
	if (ret != 0)
	{
		LOG_WRN("Failed to set LoRaWAN class: %d", ret);
	}
#endif

	int debug_count = 0;
	enum TriageStatus triage_status = P0;
	int battery_percentage = 100;

	// Set GNSS callback
	set_callback_rmc(rmc_handler);

	while (1)
	{

#define LORAWAN_PORT 2

		struct minmea_sentence_gga last_gga;
		struct minmea_sentence_gst last_gst;

		get_last_gnss_gga(&last_gga);
		get_last_gnss_gst(&last_gst);

#if VERSION == 1 
		uint8_t payload[19];
#else
		uint8_t payload[5 + sizeof( struct minmea_sentence_gga) + sizeof( struct minmea_sentence_gst)];
#endif

		// Build test payload format here - keep it similar to OpenThread payload
		// Byte 0 - version [1]
		payload[0] = VERSION;

		// Byte 1 - triageStatus [1]
		payload[1] = triage_status;

		// Byte 2 - batteryPercentage [1]
		payload[2] = battery_percentage;

		// Byte 3 - temperature [1]
//		payload[3] = whole_celsius;
		payload[3] = 0;

		// Byte 4 - fix type
		payload[4] = last_gga.fix_quality;

		// Byte 5 .. 8 - latitude [4]
		*((float *)&payload[5]) = minmea_tocoord(&last_gga.latitude);

		// Byte 9 .. 12 - longitude [4]
		*((float *)&payload[9]) = minmea_tocoord(&last_gga.longitude);

		// Byte 13 .. 16 - altitude [4]
		*((float *)&payload[13]) = minmea_tofloat(&last_gga.altitude);

		// Byte 17 - accuracyMetres [1]
		payload[17] = (uint8_t)minmea_tofloat(&last_gst.rms_deviation);

		// Byte 18 - debugCount [1]
		payload[18] = debug_count++;

#if VERSION == 2
		// Byte (5+sizeof(struct minmea_sentence_gga)) to Z last GST
		memcpy(&payload[5 + sizeof(struct minmea_sentence_gga)], &last_gst, sizeof(struct minmea_sentence_gst));
#endif

		// TODO: Need to have a look at this. It seems to take 7-8s to send a message
		LOG_INF("lorawan_send %d bytes", sizeof(payload));
		ret = lorawan_send(LORAWAN_PORT, payload, sizeof(payload), LORAWAN_MSG_UNCONFIRMED);
		if (ret == -EAGAIN)
		{
			LOG_WRN("lorawan_send failed: %d. Continuing...", ret);
			LOG_DBG("Retry Sleep.");
			k_sleep(DELAY_RESEND_S);
			LOG_DBG("Slept.");
			continue;
		}
		else if (ret < 0)
		{
			LOG_WRN("lorawan_send failed: %d", ret);
			//			return -1;
		}
		else
		{
//			LOG_INF("Data sent! (debug count %d) (tx payload bytes %d)", debug_count, sizeof(payload));
		}

		LOG_DBG("Loop Sleep.");
		k_sleep(DELAY_MSG_S);
		LOG_DBG("Slept.");

#warning Updating triage status for debugging
		if (++triage_status >= P3)
			triage_status = P0;
	}

	return 0;
}

K_THREAD_DEFINE(lorawan_client_id, 8192, lorawan_client_thread, NULL, NULL, NULL, 7, 0, 0);