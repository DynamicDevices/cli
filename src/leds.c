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

#define BLINK_SLOW                              0x80
#define BLINK_FAST                              0x01

// Statics

LOG_MODULE_REGISTER(leds, CONFIG_LED_LOG_LEVEL);

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

// Static Functions

bool ledWriteReg(const struct device *i2c_dev, uint8_t reg, uint8_t val)
{
    uint8_t datas[2];

    datas[0] = reg;
    datas[1] = val;
    return i2c_write(i2c_dev, datas, 2, LED_ADDRESS);
}

// Functions

bool ledsInit(void)
{
    LOG_DBG("Initialising LEDs");

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

bool ledsWriteRGBW(uint16_t red, uint16_t green, uint16_t blue, uint16_t white)
{
    LOG_DBG("Writing to LEDS R 0x%X, G 0x%X, G 0x%X, W 0x%X", red, green, blue, white);

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

bool ledsWriteBlink(uint8_t freq, uint8_t duty, uint8_t cycles)
{
    LOG_DBG("Writing to LEDS Blink Freq 0x%X, Duty 0x%X, Cycles 0x%X", freq, duty, cycles);

    // Set driver blinking mode
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_MODE, MP3320A_DMBLK | MP3320A_EN | MP3320A_BLK_PWM | MP3320A_EN_CP);

    // Write Blinking Frequency
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_FREQ, freq);
    // Write Blinking Duty Tblinkon = 16.384 * DUTYBLK[7:0] (ms) 
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_DUTY, duty);
    // Write Blinking Cycles
    ledWriteReg(i2c_dev, MP3320A_REG_BLINKING_CYCLES, cycles);

    return true;
}

bool ledsTest()
{
    // Cycle through R->G>B>W
    ledsWriteRGBW(LEDS_MAX_VAL, 0, 0, 0);
	k_sleep(K_MSEC(500));
	ledsWriteRGBW(0, LEDS_MAX_VAL, 0, 0);
	k_sleep(K_MSEC(500));
	ledsWriteRGBW(0, 0, LEDS_MAX_VAL, 0);
	k_sleep(K_MSEC(500));
	ledsWriteRGBW(0, 0, 0, LEDS_MAX_VAL);
	k_sleep(K_MSEC(500));
	ledsWriteRGBW(0, 0, 0, 0);

	k_sleep(K_MSEC(500));
	ledsWriteRGBW(LEDS_MAX_VAL, 0, 0, 0);
    ledsWriteBlink(BLINK_SLOW, 0x03, BLINK_CYCLES_FOREVER);

    return true;
}


bool ledBlink(EnumLedBlinkColour colour, EnumLedBlinkSpeed speed, uint8_t cycles)
{
    LOG_DBG("Blinking LED %d at speed %d for %d cycles", colour, speed, cycles);

    switch(colour)
    {
        case RED:
            ledsWriteRGBW(LEDS_MAX_VAL, 0, 0, 0);
            break;
        case GREEN:
            ledsWriteRGBW(0, LEDS_MAX_VAL, 0, 0);
            break;
        case BLUE:
            ledsWriteRGBW(0, 0, LEDS_MAX_VAL, 0);
            break;
        case WHITE:
            ledsWriteRGBW(0, 0, 0, LEDS_MAX_VAL);
            break;
        default:
            return false;
    }

    switch(speed)
    {
        case LED_BLINK_SLOW:
            ledsWriteBlink(BLINK_FAST, DUTY_10, cycles);
            break;
        case LED_BLINK_FAST:
            ledsWriteBlink(BLINK_SLOW, DUTY_10, cycles);
            break;
        default:
            return false;
    }

    return true;
}