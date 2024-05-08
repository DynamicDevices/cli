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
static char rxbuffer[RX_BUFFER_SIZE];

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

        if(!data_received) {
            if(rxdata < RX_BUFFER_SIZE) {
                if ((rxchar == '\n') || (rxchar == '\r')) {
                    rxbuffer[rxdata] = '\0';
                    rxdata =0;
                    data_received = true;
                } else {
                    rxbuffer[rxdata++] = rxchar;
                }
            }
        }
	}
}

RmcHandler _rmcHandler;

void set_callback_rmc( RmcHandler handler)
{
    _rmcHandler = handler;
}

GgaHandler _ggaHandler;

void set_callback_gga( GgaHandler handler)
{
    _ggaHandler = handler;
}

void gpsparser(void)
{
    char line[MINMEA_MAX_LENGTH];
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

    while(1) {

        k_msleep(250);

        data_received = false;
        while (data_received == false) {
            /* Allow other thread/workqueue to work. */
            continue;
        }
 
        strncpy(line, rxbuffer, sizeof(line));

        LOG_INF("Rx: >%s<", line);
            switch (minmea_sentence_id(line, false)) {
                case MINMEA_SENTENCE_RMC: {
                    struct minmea_sentence_rmc frame;
                    if (minmea_parse_rmc(&frame, line)) {
#if 0
                        LOG_DBG(INDENT_SPACES "$xxRMC: raw coordinates and speed: (%d/%d,%d/%d) %d/%d",
                                frame.latitude.value, frame.latitude.scale,
                                frame.longitude.value, frame.longitude.scale,
                                frame.speed.value, frame.speed.scale);
                        LOG_DBG(INDENT_SPACES "$xxRMC fixed-point coordinates and speed scaled to three decimal places: (%d,%d) %d",
                                minmea_rescale(&frame.latitude, 1000),
                                minmea_rescale(&frame.longitude, 1000),
                                minmea_rescale(&frame.speed, 1000));
                        LOG_DBG(INDENT_SPACES "$xxRMC floating point degree coordinates and speed: (%f,%f) %f",
                                minmea_tocoord(&frame.latitude),
                                minmea_tocoord(&frame.longitude),
                                minmea_tofloat(&frame.speed));
#endif
                        if(_rmcHandler) {
                            _rmcHandler(frame.valid, 
                                        minmea_tocoord(&frame.latitude), 
                                        minmea_tocoord(&frame.longitude), 
                                        minmea_tofloat(&frame.speed));
                        }
                    }
                    else {
                        LOG_WRN(INDENT_SPACES "$xxRMC sentence is not parsed");
                    }
                } break;

                case MINMEA_SENTENCE_GGA: {
                    struct minmea_sentence_gga frame;
                    if (minmea_parse_gga(&frame, line)) {
                        LOG_DBG(INDENT_SPACES "$xxGGA: fix quality: %d", frame.fix_quality);

                        if(_ggaHandler) {
                            _ggaHandler(minmea_tofloat(&frame.altitude));
                        }
                    }
                    else {
                        LOG_WRN(INDENT_SPACES "$xxGGA sentence is not parsed");
                    }
                } break;

                case MINMEA_SENTENCE_GST: {
                    struct minmea_sentence_gst frame;
                    if (minmea_parse_gst(&frame, line)) {
                        LOG_DBG(INDENT_SPACES "$xxGST: raw latitude,longitude and altitude error deviation: (%d/%d,%d/%d,%d/%d)",
                                frame.latitude_error_deviation.value, frame.latitude_error_deviation.scale,
                                frame.longitude_error_deviation.value, frame.longitude_error_deviation.scale,
                                frame.altitude_error_deviation.value, frame.altitude_error_deviation.scale);
                        LOG_DBG(INDENT_SPACES "$xxGST fixed point latitude,longitude and altitude error deviation"
                            " scaled to one decimal place: (%d,%d,%d)",
                                minmea_rescale(&frame.latitude_error_deviation, 10),
                                minmea_rescale(&frame.longitude_error_deviation, 10),
                                minmea_rescale(&frame.altitude_error_deviation, 10));
                        LOG_DBG(INDENT_SPACES "$xxGST floating point degree latitude, longitude and altitude error deviation: (%f,%f,%f)",
                                minmea_tofloat(&frame.latitude_error_deviation),
                                minmea_tofloat(&frame.longitude_error_deviation),
                                minmea_tofloat(&frame.altitude_error_deviation));
                    }
                    else {
                        LOG_WRN(INDENT_SPACES "$xxGST sentence is not parsed");
                    }
                } break;

                case MINMEA_SENTENCE_GSV: {
                    struct minmea_sentence_gsv frame;
                    if (minmea_parse_gsv(&frame, line)) {
                        LOG_DBG(INDENT_SPACES "$xxGSV: message %d of %d", frame.msg_nr, frame.total_msgs);
                        LOG_DBG(INDENT_SPACES "$xxGSV: sattelites in view: %d", frame.total_sats);
                        for (int i = 0; i < 4; i++)
                            LOG_DBG(INDENT_SPACES "$xxGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm",
                                frame.sats[i].nr,
                                frame.sats[i].elevation,
                                frame.sats[i].azimuth,
                                frame.sats[i].snr);
                    }
                    else {
                        LOG_WRN(INDENT_SPACES "$xxGSV sentence is not parsed");
                    }
                } break;

                case MINMEA_SENTENCE_VTG: {
                struct minmea_sentence_vtg frame;
                if (minmea_parse_vtg(&frame, line)) {
                        LOG_DBG(INDENT_SPACES "$xxVTG: true track degrees = %f",
                            minmea_tofloat(&frame.true_track_degrees));
                        LOG_DBG(INDENT_SPACES "        magnetic track degrees = %f",
                            minmea_tofloat(&frame.magnetic_track_degrees));
                        LOG_DBG(INDENT_SPACES "        speed knots = %f",
                                minmea_tofloat(&frame.speed_knots));
                        LOG_DBG(INDENT_SPACES "        speed kph = %f",
                                minmea_tofloat(&frame.speed_kph));
                }
                else {
                        LOG_WRN(INDENT_SPACES "$xxVTG sentence is not parsed");
                }
                } break;

                case MINMEA_SENTENCE_ZDA: {
                    struct minmea_sentence_zda frame;
                    if (minmea_parse_zda(&frame, line)) {
                        LOG_DBG(INDENT_SPACES "$xxZDA: %d:%d:%d %02d.%02d.%d UTC%+03d:%02d",
                            frame.time.hours,
                            frame.time.minutes,
                            frame.time.seconds,
                            frame.date.day,
                            frame.date.month,
                            frame.date.year,
                            frame.hour_offset,
                            frame.minute_offset);
                    }
                    else {
                        LOG_WRN(INDENT_SPACES "$xxZDA sentence is not parsed");
                    }
                } break;

                case MINMEA_INVALID: {
                    LOG_WRN(INDENT_SPACES "$xxxxx sentence is not valid");
                } break;

                default: {
                    LOG_WRN(INDENT_SPACES "$xxxxx sentence is not parsed");
                } break;
            }
    }
}

K_THREAD_DEFINE(gpsparser_id, 2048, gpsparser, NULL, NULL, NULL,
		7, 0, 0);