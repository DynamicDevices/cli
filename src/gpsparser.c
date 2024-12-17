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
#include <zephyr/sys/ring_buffer.h>

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

const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart2));

struct uart_config uart_cfg = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};

#define RX_TIMEOUT_MS 10
#define RX_BUFFERS 2
#define RX_BUFFER_SIZE 16
static char rxbuffer[2*RX_BUFFER_SIZE] = {'\0'};
K_SEM_DEFINE(sem_rx_data, 0, 1);
#define MAX_RING_BUF_BYTES 128
RING_BUF_DECLARE(ring_buf_rx_data, MAX_RING_BUF_BYTES);

static int fix_type = 0;
static float latitude = 0.0;
static float longitude = 0.0;
static float altitude = 0.0;

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	static int buffer_index = 0;
    static bool is_overflowing = false;

	switch (evt->type) {
	case UART_TX_DONE:
		// do something
		break;

	case UART_TX_ABORTED:
		// do something
		break;

	case UART_RX_RDY:
//		LOG_DBG("Buff %d Rx %d bytes at offset %d", buffer_index, evt->data.rx.len, evt->data.rx.offset);
		int ret = ring_buf_put(&ring_buf_rx_data, &evt->data.rx.buf[evt->data.rx.offset], evt->data.rx.len);
		if(ret != evt->data.rx.len) {
            if(!is_overflowing) {
				LOG_WRN("Rx data buffer overflow");
				is_overflowing = true;
			}
		} else {
			is_overflowing = false;
		}

		// TODO: For now tell the parser we have any new data. We could optimise this by only waking the parser when we get an EOL
		k_sem_give(&sem_rx_data);
		break;

	case UART_RX_BUF_REQUEST:
		buffer_index = (1+buffer_index) % RX_BUFFERS;
		uart_rx_buf_rsp(dev, (uint8_t *)&rxbuffer[buffer_index*RX_BUFFER_SIZE], RX_BUFFER_SIZE);
		break;

	case UART_RX_BUF_RELEASED:
		break;

	case UART_RX_DISABLED:
		break;

	case UART_RX_STOPPED:
		break;

	default:
		break;
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

void gpsparser(void)
{
    int ret;
	char gst_buf[32];
	uint8_t gst_checksum;
	bool gst_checksum_success;
	bool initialised_gnss = false;

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

    if(!device_is_ready(uart)) {
       LOG_ERR("UART not ready");
		return;
	}

    int err = uart_configure(uart, &uart_cfg);

	if (err == -ENOSYS) {
        LOG_ERR("Can't configure uart");
		return;
	}

    err = uart_callback_set(uart, uart_cb, NULL);
	if(err ) {
		LOG_ERR("Couldn't set UART callback");
		return;
	}

	uart_rx_enable(uart, (uint8_t *)rxbuffer, sizeof(rxbuffer), RX_TIMEOUT_MS);
	if (ret == 0) {
		LOG_DBG("\nuart_rx_enable success\n");
	}  else
		LOG_WRN("\nuart_rx_enable fail: %d\n", ret);

	gst_checksum = minmea_checksum("$PAIR062,8,1*");
	sprintf(gst_buf, "$PAIR062,8,1*%02x\r\n", gst_checksum);
	gst_checksum_success = minmea_check(gst_buf, true);

	if (gst_checksum_success)
		LOG_DBG("$xxGST sentence checksum check success");
	else
		LOG_WRN("$xxGST sentence checksum check fail");

	#define MAX_GPS_LINE_LEN 64

	while (1) {
		if(k_sem_take(&sem_rx_data, K_MSEC(50)) == 0) {

			static char gpsline[MAX_GPS_LINE_LEN+1] = { 0 };
			static int gpslinelen = 0;
			int bytes;
			bool eol = false;

			// We have some data. Pull it out of the ring buffer
			do {
				bytes = ring_buf_get(&ring_buf_rx_data, &gpsline[gpslinelen], 1);
				if(bytes > 0) {
					eol = gpsline[gpslinelen] == '\n';
					// Drop EOLs
					if(gpsline[gpslinelen] != '\r' && gpsline[gpslinelen] != '\n')
					gpslinelen++;
				}
				// LOG_DBG("Bytes: %d, Data: %02X, gpslinelen %d, eol %d", bytes, gpsline[gpslinelen-1], gpslinelen, eol);
			} while( gpslinelen < MAX_GPS_LINE_LEN && bytes > 0 && !eol);

			// Have we got a complete line ?
			if(!eol)
				continue;

			gpsline[gpslinelen] = '\0';
			LOG_DBG("GPS Line: %s (%d), gpslinelen %d", gpsline, strlen(gpsline), gpslinelen);

			// Reset index
			gpslinelen = 0;

			// Check if the GNSS is up so we can do our setup
			if(!initialised_gnss && !strncmp(gpsline, "$G", 2))
			{
				{
					ret = uart_tx(uart, gst_buf, strlen(gst_buf), SLEEP_TIME_MS);
					if (ret == 0)
						LOG_DBG("\nPAIR062 send success\n");
					else
						LOG_WRN("\nPAIR062 send fail: %d\n", ret);
					initialised_gnss = true;
				}
			}

			LOG_DBG("> %s", (char *)gpsline);
			switch (minmea_sentence_id(gpsline, false)) {
				case MINMEA_SENTENCE_GGA: {
					struct minmea_sentence_gga frame;

					LOG_DBG("Got GGA");

					if (minmea_parse_gga(&frame, (char *)gpsline)) {
						LOG_DBG("$xxGGA: %s", (char *)gpsline);

						LOG_DBG("$xxGGA: fix quality: %d", frame.fix_quality);
						fix_type = frame.fix_quality;

						LOG_DBG("$xxGGA: latitude: %f", minmea_tocoord(&frame.latitude));
						latitude = minmea_tocoord(&frame.latitude);

						LOG_DBG("$xxGGA: longitude: %f", minmea_tocoord(&frame.longitude));
						longitude = minmea_tocoord(&frame.longitude);

						LOG_DBG("$xxGGA: altitude: %f", minmea_tofloat(&frame.altitude));
						altitude =  minmea_tofloat(&frame.altitude);
					}
				} break;

				case MINMEA_SENTENCE_GST: {
					struct minmea_sentence_gst frame;
					if (minmea_parse_gst(&frame, (char *)gpsline)) {
						LOG_DBG("$xxGST: %s", (char *)gpsline);

						LOG_DBG("$xxGST:  latitude error deviation: %f", minmea_tofloat(&frame.latitude_error_deviation));

						LOG_DBG("$xxGST:  longitude error deviation: %f", minmea_tofloat(&frame.longitude_error_deviation));

						LOG_DBG("$xxGST:  altitude error deviation: %f", minmea_tofloat(&frame.altitude_error_deviation));
					}
				} break;

				case MINMEA_INVALID:
					break;

				default:
					break;
			}
		} else {
			continue;
		}
	}
}

K_THREAD_DEFINE(gpsparser_id, 4096, gpsparser, NULL, NULL, NULL,
		7, 0, 0);