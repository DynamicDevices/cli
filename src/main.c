/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <openthread/platform/logging.h>
#include "openthread/instance.h"
#include "openthread/thread.h"

#include "mqttsn.h"
#include "app_bluetooth.h"

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
#include "low_power.h"
#endif

LOG_MODULE_REGISTER(cli_main, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

#define WELLCOME_TEXT \
	"\n\r"\
	"\n\r"\
	"OpenThread Command Line Interface is now running.\n\r" \
	"Use the 'ot' keyword to invoke OpenThread commands e.g. " \
	"'ot thread start.'\n\r" \
	"For the full commands list refer to the OpenThread CLI " \
	"documentation at:\n\r" \
	"https://github.com/openthread/openthread/blob/master/src/cli/README.md\n\r"

// Functions

// OpenThread Support Functions

static void otStateChanged(otChangedFlags aFlags, void *aContext)
{
    otInstance *instance = (otInstance *)aContext;

    LOG_INF("State Changed");

    // when thread role changed
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        // If role changed to any of active roles then send SEARCHGW message
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER)
        {
            mqttsnSearchGateway(instance);
        }
    }
}

// Main Function

int main(int aArgc, char *aArgv[])
{
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	int ret;
	const struct device *dev;
	uint32_t dtr = 0U;

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (dev == NULL) {
		LOG_ERR("Failed to find specific UART device");
		return 0;
	}

	LOG_INF("Waiting for host to be ready to communicate");

	/* Data Terminal Ready - check if host is ready to communicate */
	while (!dtr) {
		ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (ret) {
			LOG_ERR("Failed to get Data Terminal Ready line state: %d",
				ret);
			continue;
		}
		k_msleep(100);
	}

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#endif

	LOG_INF(WELLCOME_TEXT);

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
	low_power_enable();
#endif

	// New code
	otInstance *instance;
    otError error = OT_ERROR_NONE;

    instance = openthread_get_default_instance();

#if defined(CONFIG_OPENTHREAD_MANUAL_START)
    otExtendedPanId extendedPanid;
    otNetworkKey masterKey;
	uint8_t sExpanId[] = EXTPANID;
	uint8_t sMasterKey[] = MASTER_KEY;

	// Set default network settings
    // Set network name
    LOG_INF("Setting Network Name to %s", CONFIG_OPENTHREAD_NETWORK_NAME);
    error = otThreadSetNetworkName(instance, CONFIG_OPENTHREAD_NETWORK_NAME);
    // Set PANID
    LOG_INF("Setting PANID to %s", CONFIG_OPENTHREAD_PANID);
    error = otLinkSetPanId(instance, CONFIG_OPENTHREAD_PANID);
    // Set extended PANID
	sscanf(CONFIG_OPENTHREAD_XPANID, "%x:%x:%x:%x:x:%x:%x:%x", &sExpanId[0])
    memcpy(extendedPanid.m8, sExpanId, sizeof(sExpanId));
    LOG_INF("Setting extended PANID to 0x%04X", CONFIG_OPENTHREAD_XPANID);
    error = otThreadSetExtendedPanId(instance, &extendedPanid);
    // Set channel
    LOG_INF("Setting Channel to %d", CONFIG_OPENTHREAD_CHANNEL);
    error = otLinkSetChannel(instance, CONFIG_OPENTHREAD_CHANNEL);
    // Set masterkey
    LOG_INF("Setting Network Key");
	sscanf(CONFIG_OPENTHREAD_XPANID, "%x:%x:%x:%x:x:%x:%x:%x:%x:%x:%x:%x:x:%x:%x:%x", &masterKey[0])
    error = otThreadSetNetworkKey(instance, &masterKey);
#endif

    // Register notifier callback to receive thread role changed events
    error = otSetStateChangedCallback(instance, otStateChanged, instance);

    // Start thread network
#ifdef OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE
    otIp6SetSlaacEnabled(instance, true);
#endif
    error = otIp6SetEnabled(instance, true);
    error = otThreadSetEnabled(instance, true);

	// Start Bluetooth
    appbluetoothInit();

	// Start MQTT-SN client
	mqttsnInit();

    return 0;
}