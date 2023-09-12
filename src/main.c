/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

// Includes

#include <stdio.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/lora.h>

#include <openthread/platform/logging.h>
#include "openthread/instance.h"
#include "openthread/thread.h"

#ifdef CONFIG_NRFX_TEMP
#include "nrfx_temp.h"
#endif

#include "utils.h"
#include "mqttsn.h"
#include "app_bluetooth.h"
#include "gpio.h"

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
#include "low_power.h"
#endif

// Definitions

#define MAX_DATA_LEN 255

LOG_MODULE_REGISTER(cli_main, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

#define WELCOME_TEXT \
	"\n\r"\
	"\n\r"\
    "Starting INST CLI\n\r"\
	"\n\r"\
	"\n\r"\

// Functions

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

#if defined(CONFIG_WAIT_FOR_CLI_CONNECTION)
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
#endif

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#endif

	LOG_INF(WELCOME_TEXT);

	// Start Bluetooth
// CHECK LOCKUP
//    appbluetoothInit();

	// Start MQTT-SN client
	mqttsnInit();

    return 0;
}

void mpsl_assert_handle(const char * const file, const uint32_t line)
{
	LOG_WRN("Error");
}
