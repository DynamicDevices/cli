#ifndef LEDS_H_
#define LEDS_H_

// Includes

#include <stdio.h>
#include <stdint.h>

// Defines

#define LEDS_MAX_VAL 0x7FF

#define BLINK_SLOW 0x80
#define BLINK_FAST 0x01

#define BLINK_CYCLES_FOREVER 0x00

// Prototypes

bool ledsInit(void);
bool ledsTest();
bool ledsWriteRGBW(uint16_t red, uint16_t green, uint16_t blue, uint16_t white);

#endif
