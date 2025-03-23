/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

// Includes

#include <stdio.h>

#include <ncs_version.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/gpio.h>

#include <openthread/platform/logging.h>
#include "openthread/instance.h"
#include "openthread/thread.h"

#include <zephyr/drivers/i2c.h>

#ifdef CONFIG_NRFX_TEMP
#include "nrfx_temp.h"
#endif

#include "utils.h"
#include "mqttsn.h"
#include "app.h"
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
    "Starting INST CLI build: " __DATE__ " " __TIME__ "\n\r"\
	"NCS stack: " NCS_VERSION_STRING "\n\r"\
	"\n\r"\

// Statics

enum TriageStatus triage_status = UNKNOWN;

// Functions

// Accelerometer support

#if 0

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static inline float out_ev(struct sensor_value *val)
{
	return (val->val1 + (float)val->val2 / 1000000);
}

static void fetch_and_display(const struct device *dev)
{
	struct sensor_value x, y, z;
	static int trig_cnt;

	trig_cnt++;

	/* lsm6dso accel */
	sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &z);

	LOG_INF("accel x:%f ms/2 y:%f ms/2 z:%f ms/2\n",
			(double)out_ev(&x), (double)out_ev(&y), (double)out_ev(&z));

	/* lsm6dso gyro */
	sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

	LOG_INF("gyro x:%f rad/s y:%f rad/s z:%f rad/s\n",
			(double)out_ev(&x), (double)out_ev(&y), (double)out_ev(&z));

	LOG_INF("trig_cnt:%d\n\n", trig_cnt);
}

static int set_sampling_freq(const struct device *dev)
{
	int ret = 0;
	struct sensor_value odr_attr;

	/* set accel/gyro sampling frequency to 12.5 Hz */
	odr_attr.val1 = 12.5;
	odr_attr.val2 = 0;

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
			SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
	if (ret != 0) {
		LOG_ERR("Cannot set sampling frequency for accelerometer.\n");
		return ret;
	}

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
			SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
	if (ret != 0) {
		LOG_ERR("Cannot set sampling frequency for gyro.\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_LSM6DSO_TRIGGER
static void trigger_handler(const struct device *dev,
			    const struct sensor_trigger *trig)
{
	fetch_and_display(dev);
}

static void test_trigger_mode(const struct device *dev)
{
	struct sensor_trigger trig;

	if (set_sampling_freq(dev) != 0) {
		return;
	}

	trig.type = SENSOR_TRIG_DATA_READY;
	trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	if (sensor_trigger_set(dev, &trig, trigger_handler) != 0) {
		LOG_ERR("Could not set sensor type and channel\n");
		return;
	}
}

#else
static void test_polling_mode(const struct device *dev)
{
	if (set_sampling_freq(dev) != 0) {
		return;
	}

	while (1) {
		fetch_and_display(dev);
		k_sleep(K_MSEC(1000));
	}
}
#endif

#endif // ACC

// Main Function

#ifdef CONFIG_ADC

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

const struct gpio_dt_spec flex_enable = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, flex_enable_gpios);

#if NCS_VERSION_NUMBER >= 0x20901

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

#else

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
        !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
        ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
        DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
                             DT_SPEC_AND_COMMA)
};

#endif

#endif

int main(int aArgc, char *aArgv[])
{
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	int ret;
	const struct device *dev;
//	uint32_t dtr = 0U;

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

	// Short delay to let the host detect the USB serial port
	k_msleep(2000);

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

	// RGB LED
	uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

	#define I2C_DEV_NODE DT_ALIAS(i2c2)

	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device is not ready\n");
	}
	/* 1. Verify i2c_configure() */
	else if (i2c_configure(i2c_dev, i2c_cfg)) {
		LOG_ERR("I2C config failed\n");
	}

	uint8_t datas[2];

	// RGB LED setup
	datas[0] = 0x0A;
	datas[1] = 0x19;
	i2c_write(i2c_dev, datas, 2, 0x60);
	datas[0] = 0x0B;
	datas[1] = 0x19;
	i2c_write(i2c_dev, datas, 2, 0x60);
	datas[0] = 0x0C;
	datas[1] = 0x19;
	i2c_write(i2c_dev, datas, 2, 0x60);
	datas[0] = 0x0D;
	datas[1] = 0x19;
	i2c_write(i2c_dev, datas, 2, 0x60);

	while(1)
	{
		// Colours
		datas[0] = 0x01;
		datas[1] = 0x0B;
		i2c_write(i2c_dev, datas, 2, 0x60);
		k_sleep(K_MSEC(1000));
		datas[0] = 0x01;
		datas[1] = 0x00;
		i2c_write(i2c_dev, datas, 2, 0x60);
		k_sleep(K_MSEC(1000));
	}

#if 0 // ACC

	// Test IMS
//	const struct device *const acc_dev = DEVICE_DT_GET_ONE(lsm6dso16is);

//	if (!device_is_ready(acc_dev)) {
//		printk("%s: device not ready.\n", acc_dev->name);
//		return 0;
//	}
#ifdef CONFIG_LSM6DSO_TRIGGER
	printf("Testing LSM6DSO sensor in trigger mode.\n\n");
	test_trigger_mode(acc_dev);
#else
	printf("Testing LSM6DSO sensor in polling mode.\n\n");
//	test_polling_mode(acc_dev);
#endif

//	k_sleep(K_MSEC(5000));
//	sys_reboot(SYS_REBOOT_COLD);
	
#endif // ACC

	// Test Flex Strap
#ifdef X_CONFIG_ADC

	if (!gpio_is_ready_dt(&flex_enable)) { 
		LOG_ERR("Flex Enable pin not ready");
	} else {
		if ( gpio_pin_configure_dt(&flex_enable, GPIO_OUTPUT_INACTIVE) < 0 ) {
			LOG_ERR("Can't configure Flex Enable pin");
		} else {
			if ( gpio_pin_set_dt(&flex_enable, 1)  < 0) {
				LOG_ERR("Can't set Flex Enable pin HI");
			}
			else {
				LOG_INF("Flex Enable pin set HI");
			}
		}
	}

	// Now setup ADC channel
#if NCS_VERSION_NUMBER < 0x20901
	if (!device_is_ready(adc_channels[0].dev)) {
		LOG_ERR("ADC controller device %s not ready", adc_channels[0].dev->name);
	#else
	if (!adc_is_ready_dt(&adc_channel)) {
		LOG_ERR("ADC controller device %s not ready", adc_channel.dev->name);
#endif
		return 0;
	}

#if NCS_VERSION_NUMBER < 0x20901
	int err = adc_channel_setup_dt(&adc_channels[0]);
#else
	int err = adc_channel_setup_dt(&adc_channel);
#endif
	if (err < 0) {
		LOG_ERR("Could not setup channel #%d (%d)", 0, err);
		return 0;
	}

	int16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
		//Optional
		//.calibrate = true,
	};

#if NCS_VERSION_NUMBER < 0x20901
	err = adc_sequence_init_dt(&adc_channels[0], &sequence);
#else
	err = adc_sequence_init_dt(&adc_channel, &sequence);
#endif
	if (err < 0) {
		LOG_ERR("Could not initialise sequnce");
		return 0;
	}

	// ADC
	while(1) 
	{
#if NCS_VERSION_NUMBER < 0x20901
		err = adc_read(adc_channels[0].dev, &sequence);
#else
		err = adc_read(adc_channel.dev, &sequence);
#endif
		if (err < 0) {
			LOG_ERR("Could not read (%d)", err);
			continue;
		}
	
//		LOG_INF("ADC value: %d", buf);

		int32_t val_mv = buf;
		err = adc_raw_to_millivolts	(	600, ADC_GAIN_1_3, 12, &val_mv);
		if (err < 0) {
			LOG_WRN(" (value in mV not available)\n");
		} else {
//			LOG_INF(" = %d mV", val_mv);
		}

		/* 
			< 460 mV there's possibly a fault with the flexi - FAULT

			>= 460 mV < 570 mV       no cuts	-	P3
			>= 570 mV < 701 mV       1 cut		-	P2
			>= 701 mV < 858 mV       2 cuts		-	P1
			>= 858 mV < 1049 mV      3 cuts		-	NB
			>= 1049 mV < 1282 mV     4 cuts		-	DEAD
			>= 1282 mV < 1594 mV     5 cuts		-	S1
			>= 1594 mV               6 cuts		-	S2
		*/
		if(val_mv < 460) {
			triage_status = FAULT;
		} else if(val_mv >= 460 && val_mv < 570) {
			triage_status = P3;
		} else if(val_mv >= 570 && val_mv < 701) {
			triage_status = P2;
		} else if(val_mv >= 701 && val_mv < 858) {
			triage_status = P1;
		} else if(val_mv >= 858 && val_mv < 1049) {
			triage_status = NB;
		} else if(val_mv >= 1049 && val_mv < 1282) {
			triage_status = DEAD;
		} else if(val_mv >= 1282 && val_mv < 1594) {
			triage_status = S1;
		} else if(val_mv >= 1594) {
			triage_status = S2;
		}

		switch(triage_status) {
			case P3:
				LOG_INF("Triage Status: P3 (%d)", triage_status);
				break;
			case P2:
				LOG_INF("Triage Status: P2 (%d)", triage_status);
				break;
			case P1:
				LOG_INF("Triage Status: P1 (%d)", triage_status);
				break;
			case NB:
				LOG_INF("Triage Status: NB (%d)", triage_status);
				break;
			case DEAD:
				LOG_INF("Triage Status: DEAD (%d)", triage_status);
				break;
			case S1:
				LOG_INF("Triage Status: S1 (%d)", triage_status);
				break;
			case S2:
				LOG_INF("Triage Status: S2 (%d)", triage_status);
				break;
			case UNUSED:
				LOG_INF("Triage Status: UNUSED (%d)", triage_status);
				break;
			case FAULT:
				LOG_INF("Triage Status: FAULT (%d)", triage_status);
				break;
			default:
				LOG_INF("Triage Status: UNKNOWN (%d)", triage_status);
				break;
		}
		k_sleep(K_MSEC(5000));
	}
#endif

	// Start Bluetooth
// CHECK LOCKUP
//    appbluetoothInit();

	// Start MQTT-SN client
//	mqttsnInit();

    return 0;
}

void mpsl_assert_handle(const char * const file, const uint32_t line)
{
	LOG_WRN("Error");
	sys_reboot(SYS_REBOOT_COLD);
}
