// Includes

#include <stdio.h>
#include <stdbool.h>

#include "leds.h"

#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>

// Defines

#define LED_ADDRESS 0x60

#define I2C_DEV_NODE DT_ALIAS(i2c2)

// - MP3320A Registers

#define MP3320A_REG_DEVICE_ID                   0x00
#define MP3320A_REG_BLINKING_MODE               0x01
#define MP3320A_REG_CHARGE_PUMP                 0x02
#define MP3320A_REG_CHANNEL_SETTING             0x03
#define MP3320A_REG_STEP_UP_DOWN                0x04
#define MP3320A_REG_PWM_DIMMING                 0x05
#define MP3320A_REG_BLINKING_FREQ               0x06
#define MP3320A_REG_BLINKING_DUTY               0x07
#define MP3320A_REG_BLINKING_CYCLES             0x08
#define MP3320A_REG_PROT_PHASE                  0x09
#define MP3320A_REG_CH1_LED_CURRENT_AMP         0x0A
#define MP3320A_REG_CH2_LED_CURRENT_AMP         0x0B
#define MP3320A_REG_CH3_LED_CURRENT_AMP         0x0C
#define MP3320A_REG_CH4_LED_CURRENT_AMP         0x0D
#define MP3320A_REG_CH1_LED_CURRENT_DIM_LOW     0x0E
#define MP3320A_REG_CH1_LED_CURRENT_DIM_HIGH    0x0F
#define MP3320A_REG_CH2_LED_CURRENT_DIM_LOW     0x10
#define MP3320A_REG_CH2_LED_CURRENT_DIM_HIGH    0x11
#define MP3320A_REG_CH3_LED_CURRENT_DIM_LOW     0x12
#define MP3320A_REG_CH3_LED_CURRENT_DIM_HIGH    0x13
#define MP3320A_REG_CH4_LED_CURRENT_DIM_LOW     0x14
#define MP3320A_REG_CH4_LED_CURRENT_DIM_HIGH    0x15
#define MP3320A_REG_OPEN_SHORT_STATUS           0x16

#define MP3320A_DMBLK                           0x10
#define MP3320A_EN                              0x08
#define MP3320A_CH4MD                           0x04
#define MP3320A_BLK_PWM                         0x02
#define MP3320A_EN_CP                           0x01

// Statics

LOG_MODULE_REGISTER(leds, CONFIG_LED_LOG_LEVEL);

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

uint16_t _lastRed = 0;
uint16_t _lastGreen = 0;
uint16_t _lastBlue = 0;  
uint16_t _lastWhite = 0;

bool _lastBlink = false;
uint8_t _lastFreq = 0;
uint8_t _lastDuty = 0;
uint8_t _lastCycles = 0;

// Static Functions

bool ledWriteReg(const struct device *i2c_dev, uint8_t reg, uint8_t val)
{
    uint8_t datas[2];

    datas[0] = reg;
    datas[1] = val;
    return i2c_write(i2c_dev, datas, 2, LED_ADDRESS);
}

bool ledWriteRGBW(uint16_t red, uint16_t green, uint16_t blue, uint16_t white)
{
    LOG_DBG("Writing to LED R 0x%03X, G 0x%03X, G 0x%03X, W 0x%03X", red, green, blue, white);

    _lastBlink = false;
    _lastRed = red;
    _lastGreen = green;
    _lastBlue = blue;
    _lastWhite = white;
    
    // Set driver colour mode
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_MODE, MP3320A_EN | MP3320A_BLK_PWM | MP3320A_EN_CP);

    // Write Red low then high
    ledWriteReg(i2c_dev, MP3320A_REG_CH1_LED_CURRENT_DIM_LOW, red & 0x07);
    ledWriteReg(i2c_dev, MP3320A_REG_CH1_LED_CURRENT_DIM_HIGH, red >> 3);
    // Write Green low then high
    ledWriteReg(i2c_dev, MP3320A_REG_CH2_LED_CURRENT_DIM_LOW, green & 0x07);
    ledWriteReg(i2c_dev, MP3320A_REG_CH2_LED_CURRENT_DIM_HIGH, green >> 3);
    // Write Blue low then high
    ledWriteReg(i2c_dev, MP3320A_REG_CH3_LED_CURRENT_DIM_LOW, blue & 0x07);
    ledWriteReg(i2c_dev, MP3320A_REG_CH3_LED_CURRENT_DIM_HIGH, blue >> 3);
    // Write White low then high
    ledWriteReg(i2c_dev, MP3320A_REG_CH4_LED_CURRENT_DIM_LOW, white & 0x07);
    ledWriteReg(i2c_dev, MP3320A_REG_CH4_LED_CURRENT_DIM_HIGH, white >> 3);

    return true;
}

bool ledWriteBlink(uint8_t freq, uint8_t duty, uint8_t cycles)
{
    LOG_DBG("Writing to LEDS Blink Freq %d, Duty %d, Cycles %d", freq, duty, cycles);

    _lastBlink = true;
    _lastFreq = freq;
    _lastDuty = duty;
    _lastCycles = cycles;

    // Set driver blinking mode
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_MODE, MP3320A_DMBLK | MP3320A_EN | MP3320A_CH4MD | MP3320A_BLK_PWM | MP3320A_EN_CP);

    // Write Blinking Frequency
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_FREQ, freq);
    // Write Blinking Duty Tblinkon = 16.384 * DUTYBLK[7:0] (ms) 
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_DUTY, duty);
    // Write Blinking Cycles
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_CYCLES, cycles);

    return true;
}

// Functions

bool ledInit(void)
{
    LOG_DBG("Initialising LED");

	// RGB LED
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device is not ready\n");
        return false;
	}
	/* 1. Verify i2c_configure() */
	else if (i2c_configure(i2c_dev, i2c_cfg)) {
		LOG_ERR("I2C config failed\n");
        return false;
	}

    ledWriteReg(i2c_dev, MP3320A_REG_CH1_LED_CURRENT_AMP, 0x19); // *** Set the current for the red LED to 5 mA
    ledWriteReg(i2c_dev, MP3320A_REG_CH2_LED_CURRENT_AMP, 0x19); // *** Set the current for the green LED to 5 mA
    ledWriteReg(i2c_dev, MP3320A_REG_CH3_LED_CURRENT_AMP, 0x19); // *** Set the current for the blue LED to 5 mA
    ledWriteReg(i2c_dev, MP3320A_REG_CH4_LED_CURRENT_AMP, 0x19); // *** Set the current for the white LED to 5 mA

    ledWriteReg(i2c_dev, MP3320A_REG_PROT_PHASE, 0x00); // *** Turn off the LED open and short protection

    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_MODE, MP3320A_EN | MP3320A_BLK_PWM | MP3320A_EN_CP); // *** *** Enable the driver in dimming mode

    return true;
}

bool ledColour(EnumLedBlinkColour colour)
{
    LOG_DBG("Setting LED %d", colour);

    switch(colour)
    {
        case RED:
            ledWriteRGBW(LED_MAX_BRIGHTNESS, 0, 0, 0);
            break;
        case GREEN:
            ledWriteRGBW(0, LED_MAX_BRIGHTNESS, 0, 0);
            break;
        case BLUE:
            ledWriteRGBW(0, 0, LED_MAX_BRIGHTNESS, 0);
            break;
        case PURPLE:
            ledWriteRGBW(LED_MAX_BRIGHTNESS/2, 0, LED_MAX_BRIGHTNESS/2, 0);
            break;
        case ORANGE:
            // (165/255)*LED_MAX_BRIGHTNESS
            ledWriteRGBW(LED_MAX_BRIGHTNESS, 1240, 0, 0);
            break;
        case YELLOW:
            ledWriteRGBW(LED_MAX_BRIGHTNESS, LED_MAX_BRIGHTNESS, 0, 0);
            break;
        case CYAN:
            ledWriteRGBW(0, LED_MAX_BRIGHTNESS, LED_MAX_BRIGHTNESS, 0);
            break;
        case MAGENTA:
            ledWriteRGBW(LED_MAX_BRIGHTNESS, 0, LED_MAX_BRIGHTNESS, 0);
            break;
        case HOT_PINK:
            // (255,105,180)
            ledWriteRGBW(LED_MAX_BRIGHTNESS, (105*LED_MAX_BRIGHTNESS/255), (180*LED_MAX_BRIGHTNESS/255), 0);
            break;
        case WHITE:
            ledWriteRGBW(0, 0, 0, LED_WHITE_MAX_BRIGHTNESS);
            break;
        default:
            return false;
    }

    return true;
}

bool ledBlink(EnumLedBlinkColour colour, EnumLedBlinkSpeed speed, EnumLedBlinkDuty duty_percentage, uint8_t cycles)
{
    LOG_DBG("Blinking LED %d at speed %d for %d cycles", colour, speed, cycles);

    ledColour(colour);

    // TODO: Calculate correct duty percentage based on frequency
    ledWriteBlink(speed, duty_percentage, cycles);
    
    return true;
}

bool ledSetColourAndWaitMs(EnumLedBlinkColour colour, uint16_t delay_ms)
{
    LOG_DBG("Setting LED %d and waiting %d ms", colour, delay_ms);

    uint16_t red = _lastRed;
    uint16_t green = _lastGreen;
    uint16_t blue = _lastBlue;
    uint16_t white = _lastWhite;

    bool blink = _lastBlink;
    uint8_t freq = _lastFreq;
    uint8_t duty = _lastDuty;
    uint8_t cycles = _lastCycles;

    ledColour(colour);

    k_sleep(K_MSEC(delay_ms));

    ledWriteRGBW(red, green,blue, white);
    if(blink)
        ledWriteBlink(freq, duty, cycles);

    return true;
}

bool ledTest()
{
    LOG_DBG("Testing LED");

    // Cycle through R->G>B>W
    ledWriteRGBW(LED_MAX_BRIGHTNESS, 0, 0, 0);
	k_sleep(K_MSEC(500));
	ledWriteRGBW(0, LED_MAX_BRIGHTNESS, 0, 0);
	k_sleep(K_MSEC(500));
	ledWriteRGBW(0, 0, LED_MAX_BRIGHTNESS, 0);
	k_sleep(K_MSEC(500));
	ledWriteRGBW(0, 0, 0, LED_WHITE_MAX_BRIGHTNESS);
	k_sleep(K_MSEC(500));
	ledWriteRGBW(0, 0, 0, 0);

    return true;
}
