#include "gpio.h"

// Includes
#include <zephyr/logging/log.h>

// Definitions

// Protototypes

// Globals

static const struct gpio_dt_spec led_yellow = GPIO_DT_SPEC_GET(LED_NODE_YELLOW, gpios);
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_NODE_RED, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED_NODE_BLUE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_NODE_GREEN, gpios);

// Functions
LOG_MODULE_REGISTER(gpio, CONFIG_GPIO_LOG_LEVEL);

void otLedSetOff(uint8_t aLed) {
    int err = 0;
	switch (aLed) {
        case LED_YELLOW:
            err = gpio_pin_configure_dt(&led_yellow, GPIO_OUTPUT_INACTIVE);
            break;
        case LED_RED:
            err = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
            break;
        case LED_BLUE:
            err = gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
            break;
        case LED_GREEN:
            err = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
            break;
    }
	if (err < 0) {
		LOG_WRN("LED set OFF failed");
	}

}

void otLedSetOn(uint8_t aLed) {
	int err = 0;
	switch (aLed) {
        case LED_YELLOW:
            err = gpio_pin_configure_dt(&led_yellow, GPIO_OUTPUT_ACTIVE);
            break;
        case LED_RED:
            err = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_ACTIVE);
            break;
        case LED_BLUE:
            err = gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_ACTIVE);
            break;
        case LED_GREEN:
            err = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_ACTIVE);
            break;
    }
	if (err < 0) {
		LOG_WRN("LED set ON failed");
	}
}

void otLedToggle(uint8_t aLed) {
	int err = 0;
	switch (aLed) {
        case LED_YELLOW:
            err = gpio_pin_toggle_dt(&led_yellow);
            break;
        case LED_RED:
            err = gpio_pin_toggle_dt(&led_red);
            break;
        case LED_BLUE:
            err = gpio_pin_toggle_dt(&led_blue);
            break;
        case LED_GREEN:
            err = gpio_pin_toggle_dt(&led_green);
            break;
    }
	if (err < 0) {
		LOG_WRN("LED toggle failed");
	}
}

void otLedRoleIndicator(otDeviceRole role) {
    switch (role)
    {
        case OT_DEVICE_ROLE_LEADER:
            otLedSetOff(LED_BLUE);
            otLedSetOff(LED_GREEN);
            otLedSetOn(LED_RED);
            break;
        case OT_DEVICE_ROLE_ROUTER:
            otLedSetOff(LED_RED);
            otLedSetOff(LED_GREEN);
            otLedSetOn(LED_BLUE);
            break;
        case OT_DEVICE_ROLE_CHILD:
            otLedSetOff(LED_RED);
            otLedSetOff(LED_BLUE);
            otLedSetOn(LED_GREEN);
            break;
        case OT_DEVICE_ROLE_DETACHED:
        case OT_DEVICE_ROLE_DISABLED:
            otLedSetOn(LED_RED);
            otLedSetOn(LED_BLUE);
            otLedSetOn(LED_GREEN);
            break;
    }
}

void otLedInit(void) {
    if (!gpio_is_ready_dt(&led_yellow)) { LOG_WRN("GPIO port validation failed."); }
    if (!gpio_is_ready_dt(&led_red)) { LOG_WRN("GPIO port validation failed."); }
    if (!gpio_is_ready_dt(&led_blue)) { LOG_WRN("GPIO port validation failed."); }
    if (!gpio_is_ready_dt(&led_green)) { LOG_WRN("GPIO port validation failed."); }
	
    otLedSetOff(LED_YELLOW);
    otLedSetOff(LED_RED);
    otLedSetOff(LED_BLUE);
    otLedSetOff(LED_GREEN);
}
