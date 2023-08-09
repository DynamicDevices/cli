#include "gpio.h"

// Includes
#include <zephyr/logging/log.h>

// Definitions

// Protototypes

// Globals

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

// Functions
LOG_MODULE_REGISTER(gpio, CONFIG_GPIO_LOG_LEVEL);

void otLedSetOff(uint8_t aLed) {
    int err = 0;
	switch (aLed) {
        case 1:
            err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
            break;
        case 2:
            err = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
            break;
        case 3:
            err = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
            break;
        case 4:
            err = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);
            break;
    }
	if (err < 0) {
		LOG_WRN("LED set OFF failed");
	}

}

void otLedSetOn(uint8_t aLed) {
	int err = 0;
	switch (aLed) {
        case 1:
            err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
            break;
        case 2:
            err = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
            break;
        case 3:
            err = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
            break;
        case 4:
            err = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);
            break;
    }
	if (err < 0) {
		LOG_WRN("LED set ON failed");
	}
}

void otLedToggle(uint8_t aLed) {
	int err = 0;
	switch (aLed) {
        case 1:
            err = gpio_pin_toggle_dt(&led0);
            break;
        case 2:
            err = gpio_pin_toggle_dt(&led1);
            break;
        case 3:
            err = gpio_pin_toggle_dt(&led2);
            break;
        case 4:
            err = gpio_pin_toggle_dt(&led3);
            break;
    }
	if (err < 0) {
		LOG_WRN("LED toggle failed");
	}
}

void otLedBlink(uint8_t aLed) {
    switch (aLed) {
        case 1:
            otLedToggle(1);
            k_msleep(SLEEP_TIME_MS);
            otLedToggle(1);
            break;
        case 2:
            otLedToggle(2);
            k_msleep(SLEEP_TIME_MS);
            otLedToggle(2);
            break;
        case 3:
            otLedToggle(3);
            k_msleep(SLEEP_TIME_MS);
            otLedToggle(3);
            break;
        case 4:
            otLedToggle(4);
            k_msleep(SLEEP_TIME_MS);
            otLedToggle(4);
            break;
    }
}

void otLedRoleIndicator(otDeviceRole role) {
    switch (role)
    {
        case OT_DEVICE_ROLE_LEADER:
            otLedSetOff(3);
            otLedSetOff(4);
            otLedSetOn(2);
            break;
        case OT_DEVICE_ROLE_ROUTER:
            otLedSetOff(2);
            otLedSetOff(4);
            otLedSetOn(3);
            break;
        case OT_DEVICE_ROLE_CHILD:
            otLedSetOff(2);
            otLedSetOff(3);
            otLedSetOn(4);
            break;
        case OT_DEVICE_ROLE_DETACHED:
        case OT_DEVICE_ROLE_DISABLED:
            otLedToggle(2);
            otLedToggle(3);
            otLedToggle(4);
            otLedBlink(1);
            break;
    }
}

void otLedInit(void) {
    if (!gpio_is_ready_dt(&led0)) { LOG_WRN("GPIO port validation failed."); }
    if (!gpio_is_ready_dt(&led1)) { LOG_WRN("GPIO port validation failed."); }
    if (!gpio_is_ready_dt(&led2)) { LOG_WRN("GPIO port validation failed."); }
    if (!gpio_is_ready_dt(&led3)) { LOG_WRN("GPIO port validation failed."); }
	
    otLedSetOff(1);
    otLedSetOff(2);
	otLedSetOff(3);
    otLedSetOff(4);
}