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

#include <openthread/platform/logging.h>
#include "openthread/instance.h"
#include "openthread/thread.h"

#include <zephyr/drivers/i2c.h>

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
    "Starting INST CLI build: " __DATE__ " " __TIME__ "\n\r"\
	"NCS stack: " NCS_VERSION_STRING "\n\r"\
	"\n\r"\

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
static const struct gpio_dt_spec flexi_en = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
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

//#define DEBUG_SCAN_FOR_DEVICES

#ifdef DEBUG_SCAN_FOR_DEVICES

	printk("The I2C scanner started\n");
    const struct device *i2c_dev;
    int error;

	// Show I2C devices
    i2c_dev = device_get_binding("I2C_2");
    if (!i2c_dev) {
        printk("Binding failed.");
        return;
    }

    /* Demonstration of runtime configuration */
    i2c_configure(i2c_dev, I2C_SPEED_SET(I2C_SPEED_STANDARD));
    printk("Value of NRF_TWIM2->PSEL.SCL : %d \n",NRF_TWIM2->PSEL.SCL);
    printk("Value of NRF_TWIM2->PSEL.SDA : %d \n",NRF_TWIM2->PSEL.SDA);
    printk("Value of NRF_TWIM2->FREQUENCY: %d \n",NRF_TWIM2->FREQUENCY);
    printk("26738688 -> 100k\n");

    printk("Scanning for devices\n");

	for (uint8_t i = 4; i <= 0x7F; i++) {
        struct i2c_msg msgs[1];
        uint8_t dst = 1;

        msgs[0].buf = &dst;
        msgs[0].len = 1U;
        msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        error = i2c_transfer(i2c_dev, &msgs[0], 1, i);
        if (error == 0) {
            printk("0x%2x FOUND\n", i);
        }
        else {
            //printk("error %d \n", error);
        }
    }

	printk("Scan complete\n");

#endif

#if 0
printk("I2C write to RGBW\n");
{
	struct i2c_msg msgs[1];
	uint8_t dst[] = { 0x0A, 0x19 };

	msgs[0].buf = &dst;
	msgs[0].len = 2U;
	msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	error = i2c_transfer(i2c_dev, &msgs[0], 2, 0x60);
	if (error == 0) {
		printk("I2C write success\n");
	}
	else {
		printk("error %d \n", error);
	}
}
#endif

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

	// Test ADC
#ifdef CONFIG_ADC

    // Setup ADC VREF pin
	if (!gpio_is_ready_dt(&flexi_en)) {
		LOG_ERR("Flexi EN not ready");
		return 0;
	}
	gpio_pin_configure_dt(&flexi_en, GPIO_OUTPUT);
	gpio_pin_set_dt(&flexi_en, 1);
	
	// Now setup ADC channel
	if (!adc_is_ready_dt(&adc_channel)) {
		LOG_ERR("ADC controller device %s not ready", adc_channel.dev->name);
		return 0;
	}

	int err = adc_channel_setup_dt(&adc_channel);
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

	err = adc_sequence_init_dt(&adc_channel, &sequence);
	if (err < 0) {
		LOG_ERR("Could not initialise sequnce");
		return 0;
	}

	// ADC
	while(1) 
	{
		err = adc_read(adc_channel.dev, &sequence);
		if (err < 0) {
			LOG_ERR("Could not read (%d)", err);
			continue;
		}
	
		int32_t val_mv;
		err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
		/* conversion to mV may not be supported, skip if not */
		if (err < 0) {
			LOG_WRN(" (value in mV not available)\n");
		} else {
			LOG_INF(" = %d mV", val_mv);
		}
			
		k_sleep(K_MSEC(1000));
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
