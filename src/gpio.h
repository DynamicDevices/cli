#ifndef GPIO_H_

// Includes

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "openthread/instance.h"
#include "openthread/thread.h"

// Definitions

#define LED_YELLOW 1
#define LED_RED 2
#define LED_BLUE 3
#define LED_GREEN 4

/* The devicetree node identifier for the "led" alias. */
#define LED_NODE_YELLOW DT_ALIAS(led0)
#define LED_NODE_RED DT_ALIAS(led1_red)
#define LED_NODE_BLUE DT_ALIAS(led1_blue)
#define LED_NODE_GREEN DT_ALIAS(led1_green)

// Prototypes

void otLedInit(void);
void otLedSetOff(uint8_t aLed);
void otLedSetOn(uint8_t aLed);
void otLedToggle(uint8_t aLed);
void otLedRoleIndicator(otDeviceRole role);

#endif
