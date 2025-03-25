// Includes

#include <stdio.h>
#include <stdbool.h>

#include "flexstrap.h"
#include "leds.h"

#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

// Defines

LOG_MODULE_REGISTER(flexstrap, 4);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

const struct gpio_dt_spec flex_enable = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, flex_enable_gpios);
const struct gpio_dt_spec flex_detect = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, flex_detect_gpios);

#if NCS_VERSION_NUMBER >= 0x20901

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

#else

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
        !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
        ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
        DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
                             DT_SPEC_AND_COMMA)
};

#endif

// Enumerations

EnumTriageStatus _last_triage_status = OFF;

// Statics


// Functions

bool flexStrapInit(void)
{
    int err;

    LOG_DBG("Flex Strap Init");

	if (!gpio_is_ready_dt(&flex_enable)) { 
		LOG_ERR("Flex Enable pin not ready");
	} else {
		if ( gpio_pin_configure_dt(&flex_enable, GPIO_OUTPUT_INACTIVE) < 0 ) {
			LOG_ERR("Can't configure Flex Enable pin");
		} else {
			if ( gpio_pin_set_dt(&flex_enable, 1)  < 0) {
				LOG_ERR("Can't set Flex Enable pin HI");
			}
		}
    }

    if (!gpio_is_ready_dt(&flex_detect)) { 
		LOG_ERR("Flex Detect pin not ready");
	} else {
#ifdef CONFIG_FLEXSTRAP_PULL_DOWN
        LOG_ERR("*** CHANGE THIS FOR FLEX STRAP ENABLED HARDWARE *** Configuring Flex Detect pin with GPIO_PULL_DOWN");
		if ( gpio_pin_configure_dt(&flex_detect, GPIO_INPUT | GPIO_PULL_DOWN) < 0 ) {
#else
    if ( gpio_pin_configure_dt(&flex_detect, GPIO_INPUT) < 0 ) {
#endif
			LOG_ERR("Can't configure Flex Detect pin");
		}
    }

	// Now setup ADC channel
#if NCS_VERSION_NUMBER < 0x20901
	if (!device_is_ready(adc_channels[0].dev)) {
		LOG_ERR("ADC controller device %s not ready", adc_channels[0].dev->name);
	#else
	if (!adc_is_ready_dt(&adc_channel)) {
		LOG_ERR("ADC controller device %s not ready", adc_channel.dev->name);
#endif
		return false;
    }

#if NCS_VERSION_NUMBER < 0x20901
	err = adc_channel_setup_dt(&adc_channels[0]);
#else
	err = adc_channel_setup_dt(&adc_channel);
#endif
	if (err < 0) {
		LOG_ERR("Could not setup channel #%d (%d)", 0, err);
		return false;
    }
    return true;
}

char *getTriageStatusString(EnumTriageStatus status) {
    switch(status) {
        case P3:
            return "P3";
        case P2:
            return "P2";
        case P1:
            return "P1";
        case NB:
            return "NB";
        case DEAD:
            return "DEAD";
        case S1:
            return "S1";
        case S2:
            return "S2";
        case FAULT:
            return "FAULT";
        case UNUSED:
            return "UNUSED";
        case UNKNOWN:
            return "UNKNOWN";
        default:
            return "INVALID";
    }
}

int flexstrap_thread(void)
{
    int err;
	int16_t buf;

	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
		//Optional
		//.calibrate = true,
	};

    flexStrapInit();

    // Wait for the main loop to setup
    k_sleep(K_MSEC(5000));

#if NCS_VERSION_NUMBER < 0x20901
	err = adc_sequence_init_dt(&adc_channels[0], &sequence);
#else
	err = adc_sequence_init_dt(&adc_channel, &sequence);
#endif
	if (err < 0) {
		LOG_ERR("Could not initialise sequnce");
		return 0;
	}

	// Main loop
    LOG_DBG("Flex strap reading ADC");
	while(1) 
	{
#ifdef CONFIG_FLEXSTRAP_CONTROLS_POWER
        int pin_state = gpio_pin_get_dt(&flex_detect);
        if(pin_state == 1) {
            _last_triage_status = OFF;
            LOG_INF("Flex Detect pin is HI - Flex not strapped on - Waiting 5s");
            k_sleep(K_MSEC(5000));
            continue;
        }
#endif

#if NCS_VERSION_NUMBER < 0x20901
		err = adc_read(adc_channels[0].dev, &sequence);
#else
		err = adc_read(adc_channel.dev, &sequence);
#endif
		if (err < 0) {
			LOG_ERR("Could not read (%d)", err);
			continue;
		}

		int32_t val_mv = buf;
		err = adc_raw_to_millivolts	(	600, ADC_GAIN_1_3, 12, &val_mv);
		if (err < 0) {
			LOG_WRN(" (value in mV not available)\n");
		} else {
//			LOG_INF(" = %d mV", val_mv);
		}

        EnumTriageStatus triage_status = UNKNOWN;

		/* 
			< 460 mV there's possibly a fault with the flexi - FAULT

			>= 460 mV < 570 mV       no cuts	-	P3
			>= 570 mV < 701 mV       1 cut		-	P2
			>= 701 mV < 858 mV       2 cuts		-	P1
			>= 858 mV < 1049 mV      3 cuts		-	NB
			>= 1049 mV < 1282 mV     4 cuts		-	DEAD
			>= 1282 mV < 1594 mV     5 cuts		-	S1
			>= 1594 mV               6 cuts		-	S2
		*/
		if(val_mv < 460) {
			triage_status = FAULT;
		} else if(val_mv >= 460 && val_mv < 570) {
			triage_status = P3;
		} else if(val_mv >= 570 && val_mv < 701) {
			triage_status = P2;
		} else if(val_mv >= 701 && val_mv < 858) {
			triage_status = P1;
		} else if(val_mv >= 858 && val_mv < 1049) {
			triage_status = NB;
		} else if(val_mv >= 1049 && val_mv < 1282) {
			triage_status = DEAD;
		} else if(val_mv >= 1282 && val_mv < 1594) {
			triage_status = S1;
		} else if(val_mv >= 1594) {
			triage_status = S2;
		}

        if(triage_status != _last_triage_status) {

            LOG_INF("Triage Status changed to %s (code: %d, raw: %d, mv: %d)", getTriageStatusString(triage_status), triage_status, buf, val_mv);

            _last_triage_status = triage_status;

            switch(triage_status) {
                case P3:
                    // Green
                    LOG_DBG("- setting LED to SLOW Green");
                    ledBlink(GREEN, LED_BLINK_SLOW, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case P2:
                    // Yellow
                    LOG_DBG("- setting LED to SLOW Yellow");
                    ledBlink(YELLOW, LED_BLINK_SLOW, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case P1:
                    // Red
                    LOG_DBG("- setting LED to SLOW Red");
                    ledBlink(RED, LED_BLINK_SLOW, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case NB:
                    // Purple
                    LOG_DBG("- setting LED to SLOW PURPLE");
                    ledBlink(PURPLE, LED_BLINK_SLOW, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case DEAD:
                    // White
                    LOG_DBG("- setting LED to SLOW WHITE");
                    ledBlink(WHITE, LED_BLINK_SLOW, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case S1:
                    // White
//                    LOG_DBG("- setting LED to FAST White");
//                    ledBlink(WHITE, LED_BLINK_FAST, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
//                    break;
                case S2:
                    // White
//                    LOG_DBG("- setting LED to FAST White");
//                    ledBlink(WHITE, LED_BLINK_FAST, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case FAULT:
                    // Flash Red
                    LOG_DBG("- setting LED to FAST RED");
                    ledBlink(RED, LED_BLINK_FAST, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case UNUSED:
                    // Flash Red
                    LOG_DBG("- setting LED to FAST RED");
                    ledBlink(RED, LED_BLINK_FAST, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                case UNKNOWN:
                    // Flash Red
                    LOG_DBG("- setting LED to FAST RED");
                    ledBlink(RED, LED_BLINK_FAST, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
                default:
                    // Flash Red
                    LOG_DBG("- setting LED to FAST RED");
                    ledBlink(RED, LED_BLINK_FAST, DUTY_10_PERCENT, BLINK_CYCLES_FOREVER);
                    break;
            }
        }

		k_sleep(K_MSEC(5000));
	}

    return 0;
}

K_THREAD_DEFINE(flexstrap_id, 4096, flexstrap_thread, NULL, NULL, NULL, 7, 0, 0);