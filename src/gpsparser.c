/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "minmea.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "gpsparser.h"

LOG_MODULE_REGISTER(gpsparser, CONFIG_GPS_PARSER_LOG_LEVEL);

#define INDENT_SPACES "  "

// 1000 msec = 1 sec
#define SLEEP_TIME_S   1000
#define SLEEP_TIME_MS	100
#define SLEEP_TIME_MMS	 10

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

const struct gpio_dt_spec gnss_vbckup = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, gnss_vbckp_on_gpios);
const struct gpio_dt_spec gnss_vcc = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, gnss_vcc_on_gpios);
const struct gpio_dt_spec gnss_reset = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, gnss_reset_gpios);

const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

const struct uart_config uart_cfg = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};

#define RX_BUFFER_SIZE MINMEA_MAX_LENGTH
static volatile bool data_received;
static int rxdata = 0;
static char rxbuffer[RX_BUFFER_SIZE] = {'\0'};

static int fix_type = 0;
static float latitude = 0.0;
static float longitude = 0.0;
static float altitude = 0.0;
static char altitude_units = '\0';
static float height = 0.0;
static char height_units = '\0';

// Functions
static void uart_fifo_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

    char rxchar;

	/* Verify uart_irq_update() */
	if (!uart_irq_update(dev)) {
		LOG_ERR("retval should always be 1");
		return;
	}

	/* Verify uart_irq_rx_ready() */
	while(uart_irq_rx_ready(dev)) {

		/* Verify uart_fifo_read() */
		uart_fifo_read(dev, &rxchar, 1);

		if(rxchar != '\r') {
			if(rxchar == '\n') {
				rxdata = 0;
			} else {
				rxbuffer[rxdata++] = rxchar;
			}
		}
	}
}

RmcHandler _rmcHandler;

void set_callback_rmc( RmcHandler handler)
{
    _rmcHandler = handler;
}

int gpsparser_getfixtype() { return fix_type; }
float gpsparser_getlatitude() { return latitude; }
float gpsparser_getlongitude() { return longitude; }
float gpsparser_getaltitude() { return altitude; }
char gpsparser_getaltitudeunits() { return altitude_units; }
float gpsparser_getheight() { return height; }
char gpsparser_getheightunits() { return height_units; }

void gpsparser(void)
{
    char line[MINMEA_MAX_LENGTH] = {'\0'};
    int ret;

    if (!gpio_is_ready_dt(&gnss_vbckup)) { return; }
	if (!gpio_is_ready_dt(&gnss_vcc)) { return; }
	if (!gpio_is_ready_dt(&gnss_reset)) { return; }
	LOG_INF("pins are ready.");

    // Configure the pins
    ret = gpio_pin_configure_dt(&gnss_vbckup, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) { return; }
	ret = gpio_pin_configure_dt(&gnss_vcc, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) { return; }
	ret = gpio_pin_configure_dt(&gnss_reset, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) { return; }
	LOG_INF("pins are configured.");

    // GNSS start-up procedure
	LOG_INF("GNSS start-up procedure...");
	gpio_pin_set_dt(&gnss_vbckup, 0);
	gpio_pin_set_dt(&gnss_vcc, 0);
	gpio_pin_set_dt(&gnss_reset, 0);
	k_msleep(SLEEP_TIME_MMS);
	gpio_pin_set_dt(&gnss_vbckup, 1);
	gpio_pin_set_dt(&gnss_vcc, 1);
	k_msleep(SLEEP_TIME_MS);
	gpio_pin_set_dt(&gnss_reset, 1);
	LOG_INF("GNSS is set active.");

    int err = uart_configure(uart, &uart_cfg);

	if (err == -ENOSYS) {
        LOG_ERR("Can't open uart");
		return;
	}

	/* Verify uart_irq_callback_set() */
    uart_irq_callback_set(uart, uart_fifo_callback);

    /* Enable Tx/Rx interrupt before using fifo */
    /* Verify uart_irq_rx_enable() */
    uart_irq_rx_enable(uart);

	while (1) {
		k_msleep(10);
		if(rxbuffer[0] != '\0')  {
			// LOG_DBG("%s", rxbuffer);
			switch (minmea_sentence_id(rxbuffer, false)) {
				case MINMEA_SENTENCE_GGA: {
					struct minmea_sentence_gga frame;
					if (minmea_parse_gga(&frame, rxbuffer)) {
						LOG_DBG("%s", rxbuffer);

						LOG_DBG("$xxGGA: fix quality: %d", frame.fix_quality);
						fix_type = frame.fix_quality;

						LOG_DBG("$xxGGA: latitude: %f", minmea_tocoord(&frame.latitude));
						latitude = minmea_tocoord(&frame.latitude);

						LOG_DBG("$xxGGA: longitude: %f", minmea_tocoord(&frame.longitude));
						longitude = minmea_tocoord(&frame.longitude);

						LOG_DBG("$xxGGA: altitude: (%d/%d)%c", frame.altitude.value, frame.altitude.scale, frame.altitude_units);
						altitude = (float)frame.altitude.value / (float)frame.altitude.scale;
						altitude_units = frame.altitude_units;

						LOG_DBG("$xxGGA: height: (%d/%d)%c", frame.height.value, frame.height.scale, frame.height_units);
						height = (float)frame.height.value / (float)frame.height.scale;
						height_units = frame.height_units;
					}
				} break;

				case MINMEA_INVALID:
					break;

				default:
					break;
			}
			strncpy(rxbuffer, "", sizeof(rxbuffer));
		} else {
			continue;
		}
	}
}

K_THREAD_DEFINE(gpsparser_id, 2048, gpsparser, NULL, NULL, NULL,
		7, 0, 0);