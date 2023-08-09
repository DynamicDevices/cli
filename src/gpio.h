#ifndef GPIO_H_

// Includes

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "openthread/instance.h"
#include "openthread/thread.h"

// Definitions

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   300

/* The devicetree node identifier for the "led" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1_red)
#define LED2_NODE DT_ALIAS(led1_blue)
#define LED3_NODE DT_ALIAS(led1_green)

// Prototypes

void otLedInit(void);
void otLedSetOff(uint8_t aLed);
void otLedSetOn(uint8_t aLed);
void otLedToggle(uint8_t aLed);
void otLedBlink(uint8_t aLed);
void otLedRoleIndicator(otDeviceRole role);

#endif
