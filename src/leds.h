#ifndef LEDS_H_
#define LEDS_H_

// Includes

#include <stdio.h>
#include <stdint.h>

// Defines

#define LEDS_MAX_VAL 0x7FF

#define BLINK_CYCLES_FOREVER 0x00

enum LedBlinkSpeed
{
    LED_BLINK_SLOW = 0x80,
    LED_BLINK_FAST = 0x01
};
typedef enum LedBlinkSpeed EnumLedBlinkSpeed;

enum LedBlinkColour
{
    RED = 0x01,
    GREEN = 0x02,
    BLUE = 0x03,    
    WHITE = 0x04,
};
typedef enum LedBlinkColour EnumLedBlinkColour;

// Prototypes

bool ledsInit(void);
bool ledsTest();
bool ledsWriteRGBW(uint16_t red, uint16_t green, uint16_t blue, uint16_t white);

#endif
