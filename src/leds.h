#ifndef LEDS_H_
#define LEDS_H_

// Includes

#include <stdio.h>
#include <stdint.h>

// Defines

#define LED_MAX_BRIGHTNESS                      0x7FF

#define BLINK_CYCLES_FOREVER                    0x00

#define DUTY_10_PERCENT                         0x03

enum LedBlinkSpeed
{
    LED_BLINK_SLOW = 0x80,
    LED_BLINK_MED =  0x40,
    LED_BLINK_FAST = 0x10,
    LED_BLINK_CONT = 0x01,
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

enum LedBlinkDuty
{
    DUTY_10 = 0x03
};
typedef enum LedBlinkColour EnumLedBlinkDuty;

// Prototypes

bool ledInit(void);
bool ledTest();
bool ledBlink(EnumLedBlinkColour colour, EnumLedBlinkSpeed speed, EnumLedBlinkDuty duty_percentage, uint8_t cycles);
bool ledColour(EnumLedBlinkColour colour);
bool ledWriteRGBW(uint16_t red, uint16_t green, uint16_t blue, uint16_t white);

#endif
