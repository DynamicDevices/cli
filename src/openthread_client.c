// Includes

#include <stdio.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/lora.h>

#include <openthread/platform/logging.h>
#include "openthread/instance.h"
#include "openthread/thread.h"

#ifdef CONFIG_NRFX_TEMP
#include "nrfx_temp.h"
#endif

#include "utils.h"
#include "mqttsn.h"
#include "app_bluetooth.h"
#include "gpio.h"

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
#include "low_power.h"
#endif

// Definitions

#define MAX_DATA_LEN 255

LOG_MODULE_REGISTER(openthread_client, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

// OpenThread Support Functions

static void otStateChanged(otChangedFlags aFlags, void *aContext)
{
    otInstance *instance = (otInstance *)aContext;

    // when thread role changed
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {

      otDeviceRole role = otThreadGetDeviceRole(instance);
		  otLedRoleIndicator(role);
      switch(role)
      {
          case 0: // OT_DEVICE_ROLE_DISABLED:
            LOG_INF("Role changed to disabled");
          break;
          case 1: // OT_DEVICE_ROLE_DETACHED:
            LOG_INF("Role changed to detached");
          break;
          case 2: // OT_DEVICE_ROLE_CHILD:
            LOG_INF("Role changed to child");
          break;
          case 3: // OT_DEVICE_ROLE_ROUTER:
            LOG_INF("Role changed to router");
          break;
          case 4: //OT_DEVICE_ROLE_LEADER:
            LOG_INF("Role changed to leader");
          break;
      }

      // If role changed to any of active roles then send SEARCHGW message
      if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER)
      {
        mqttsnSearchGateway(instance);
      }
    }
	else
	{
		LOG_INF("State change: Flags 0x%08X", aFlags);
		switch(aFlags)
		{
			case OT_CHANGED_IP6_ADDRESS_ADDED:
				LOG_INF("IPv6 address was added");
				break;
			case OT_CHANGED_IP6_ADDRESS_REMOVED:
				LOG_INF("IPv6 address was removed");
				break;
			case OT_CHANGED_THREAD_ROLE:
				LOG_INF("Role (disabled, detached, child, router, leader) changed");
				break;
			case OT_CHANGED_THREAD_LL_ADDR:
				LOG_INF("The link-local address changed");
				break;
			case OT_CHANGED_THREAD_ML_ADDR:
				LOG_INF("The mesh-local address changed");
				break;
			case OT_CHANGED_THREAD_RLOC_ADDED:
				LOG_INF("RLOC was added");
				break;
			case OT_CHANGED_THREAD_RLOC_REMOVED:
				LOG_INF("RLOC was removed");
				break;
			case OT_CHANGED_THREAD_PARTITION_ID:
				LOG_INF("Partition ID changed");
				break;
			case OT_CHANGED_THREAD_KEY_SEQUENCE_COUNTER:
				LOG_INF("Thread Key Sequence changed");
				break;
			case OT_CHANGED_THREAD_NETDATA:
				LOG_INF("Thread Network Data changed");
				break;
			case OT_CHANGED_THREAD_CHILD_ADDED:
				LOG_INF("Child was added");
				break;
			case OT_CHANGED_THREAD_CHILD_REMOVED:
				LOG_INF("Child was removed");
				break;
			case OT_CHANGED_IP6_MULTICAST_SUBSCRIBED:
				LOG_INF("Subscribed to a IPv6 multicast address");
				break;
			case OT_CHANGED_IP6_MULTICAST_UNSUBSCRIBED:
				LOG_INF("Unsubscribed from a IPv6 multicast address");
				break;
			case OT_CHANGED_THREAD_CHANNEL:
				LOG_INF("Thread network channel changed");
				break;
			case OT_CHANGED_THREAD_PANID:
				LOG_INF("Thread network PAN Id changed");
				break;
			case OT_CHANGED_THREAD_NETWORK_NAME:
				LOG_INF("Thread network name changed");
				break;
			case OT_CHANGED_THREAD_EXT_PANID:
				LOG_INF("Thread network extended PAN ID changed");
				break;
			case OT_CHANGED_NETWORK_KEY:
				LOG_INF("Network key changed");
				break;
			case OT_CHANGED_PSKC:
				LOG_INF("PSKc changed");
				break;
			case OT_CHANGED_SECURITY_POLICY:
				LOG_INF("Security Policy changed");
				break;
			case OT_CHANGED_CHANNEL_MANAGER_NEW_CHANNEL:
				LOG_INF("Channel Manager new pending Thread channel changed");
				break;
			case OT_CHANGED_SUPPORTED_CHANNEL_MASK:
				LOG_INF("Supported channel mask changed");
				break;
			case OT_CHANGED_COMMISSIONER_STATE:
				LOG_INF("Commissioner state changed");
				break;
			case OT_CHANGED_THREAD_NETIF_STATE:
				LOG_INF("Thread network interface state changed");
				break;
			case OT_CHANGED_THREAD_BACKBONE_ROUTER_STATE:
				LOG_INF("Backbone Router state changed");
				break;
			case OT_CHANGED_THREAD_BACKBONE_ROUTER_LOCAL:
				LOG_INF("Local Backbone Router configuration changed");
				break;
			case OT_CHANGED_JOINER_STATE:
				LOG_INF("Joiner state changed");
				break;
			case OT_CHANGED_ACTIVE_DATASET:
				LOG_INF("Active Operational Dataset changed");
				break;
			case OT_CHANGED_PENDING_DATASET:
				LOG_INF("Pending Operational Dataset changed");
				break;
			case OT_CHANGED_NAT64_TRANSLATOR_STATE:
				LOG_INF("The state of NAT64 translator changed");
				break;
		}
	}
}

void openthread_client_thread(void)
{
    #if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
        low_power_enable();
    #endif

    #ifdef CONFIG_NRFX_TEMP
        // Initialise internal temperature reading
        nrfx_err_t status;
        nrfx_temp_config_t config = NRFX_TEMP_DEFAULT_CONFIG;
        status = nrfx_temp_init(&config, NULL);
        NRFX_ASSERT(status == NRFX_SUCCESS);
    #endif

        // New code
        otInstance *instance;
        otError error = OT_ERROR_NONE;

        instance = openthread_get_default_instance();

    #if defined(CONFIG_OPENTHREAD_MANUAL_START)
        otExtendedPanId extendedPanid;
        otNetworkKey masterKey;

        // Set default network settings
        // Set network name
        LOG_INF("Setting Network Name to %s", CONFIG_OPENTHREAD_NETWORK_NAME);
        error = otThreadSetNetworkName(instance, CONFIG_OPENTHREAD_NETWORK_NAME);
        // Set PANID
        LOG_INF("Setting PANID to 0x%04X", (uint16_t)CONFIG_OPENTHREAD_WORKING_PANID);
        error = otLinkSetPanId(instance, (const otPanId)CONFIG_OPENTHREAD_WORKING_PANID);
        // Set extended PANID
        LOG_INF("Setting extended PANID to %s", CONFIG_OPENTHREAD_XPANID);
        datahex(CONFIG_OPENTHREAD_XPANID, &extendedPanid.m8[0], 8);
        error = otThreadSetExtendedPanId(instance, (const otExtendedPanId *)&extendedPanid);
        // Set channel if configured
        if(CONFIG_OPENTHREAD_CHANNEL > 0)
        {
            LOG_INF("Setting Channel to %d", CONFIG_OPENTHREAD_CHANNEL);
            error = otLinkSetChannel(instance, CONFIG_OPENTHREAD_CHANNEL);
        }
        // Set masterkey
        LOG_INF("Setting Network Key to %s", CONFIG_OPENTHREAD_NETWORKKEY);
        datahex(CONFIG_OPENTHREAD_NETWORKKEY, &masterKey.m8[0], 16);
        error = otThreadSetNetworkKey(instance, (const otNetworkKey *)&masterKey);
    #endif

        // Start LED
        otLedInit();

        // Register notifier callback to receive thread role changed events
        error = otSetStateChangedCallback(instance, otStateChanged, instance);

        // Start thread network
    #ifdef OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE
        otIp6SetSlaacEnabled(instance, true);
    #endif
        error = otIp6SetEnabled(instance, true);
        error = otThreadSetEnabled(instance, true);

        error = otPlatRadioSetTransmitPower(instance, 8);

        int8_t txpower;
        error = otPlatRadioGetTransmitPower(instance, &txpower);
        LOG_INF("Tx Power is %d dB", txpower);
}

K_THREAD_DEFINE(openthread_client_id, 2048, openthread_client_thread, NULL, NULL, NULL,
		7, 0, 0);