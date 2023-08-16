#include "mqttsn.h"

// Includes

#include <stdio.h>

#include "openthread/thread.h"
#include "openthread/mqttsn.h"
#include "openthread/link.h"

#include <zephyr/logging/log.h>

#include "gpio.h"
#include "app_bluetooth.h"
#include "bluetooth/lns_client.h"

// Definitions

// Protototypes

void mqttsnPublishHandler(struct k_timer *dummy);

// Globals

static otMqttsnTopic _aTopic;
static K_TIMER_DEFINE(mqttsnPublishTimer, mqttsnPublishHandler, NULL);
static uint32_t _stateCount = 0;

// Functions

LOG_MODULE_REGISTER(mqttsn, CONFIG_MQTT_SNCLIENT_LOG_LEVEL);

// Support functions
static void mqttsnHandlePublished(otMqttsnReturnCode aCode, void* aContext)
{
    OT_UNUSED_VARIABLE(aCode);
    OT_UNUSED_VARIABLE(aContext);

    // Handle published
    LOG_INF("Published");
    otLedToggle(LED_YELLOW);
}

static void mqttsnHandleRegistered(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, void* aContext)
{
    // Handle registered
//    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        LOG_DBG("HandleRegistered - OK");
        otLedToggle(LED_YELLOW);
        memcpy(&_aTopic, aTopic, sizeof(otMqttsnTopic));
    }
    else
    {
        LOG_WRN("HandleRegistered - Error");
    }
}

static void mqttsnHandleConnected(otMqttsnReturnCode aCode, void* aContext)
{
    // Handle connected
    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        LOG_DBG("HandleConnected - Accepted");
        otLedToggle(LED_YELLOW);;

        // Get ID
        otExtAddress extAddress;
        otLinkGetFactoryAssignedIeeeEui64(instance, &extAddress);

        char data[128];
        sprintf(data, "%s/%02x%02x%02x%02x%02x%02x%02x%02x", TOPIC_PREFIX,
		extAddress.m8[0],
		extAddress.m8[1],
		extAddress.m8[2],
		extAddress.m8[3],
		extAddress.m8[4],
		extAddress.m8[5],
		extAddress.m8[6],
		extAddress.m8[7]
        );

        LOG_DBG("Registering Topic: %s", data);
        otLedToggle(LED_YELLOW);

        // Obtain target topic ID
        otMqttsnRegister(instance, data, mqttsnHandleRegistered, (void *)instance);
    }
    else
    {
        switch(aCode)
        {
            case kCodeAccepted:
                    break;
            case kCodeRejectedCongestion:
                    LOG_WRN("HandleConnected - kCodeRejectedCongestion");
                    break;
            case kCodeRejectedTopicId:
                    LOG_WRN("HandleConnected - kCodeRejectedTopicId");
                    break;
            case kCodeRejectedNotSupported:
                    LOG_WRN("HandleConnected - kCodeRejectedNotSupported");
                    break;
            case kCodeTimeout:
                    LOG_WRN("HandleConnected - kCodeTimeout");
                    break;
        }
    }
}

static void mqttsnHandleSearchGw(const otIp6Address* aAddress, uint8_t aGatewayId, void* aContext)
{
    OT_UNUSED_VARIABLE(aGatewayId);

    LOG_DBG("Got search gateway response");
     otLedToggle(LED_YELLOW);

    // Handle SEARCHGW response received
    // Connect to received address
    otInstance *instance = (otInstance *)aContext;
    otIp6Address address = *aAddress;
    // Set MQTT-SN client configuration settings
    otMqttsnConfig config;

    otExtAddress extAddress;
    otLinkGetFactoryAssignedIeeeEui64(instance, &extAddress);

    char data[256];
    sprintf(data, "%s-%02x%02x%02x%02x%02x%02x%02x%02x", CLIENT_PREFIX,
                extAddress.m8[0],
                extAddress.m8[1],
                extAddress.m8[2],
                extAddress.m8[3],
                extAddress.m8[4],
                extAddress.m8[5],
                extAddress.m8[6],
                extAddress.m8[7]
    );

    config.mClientId = data;
    config.mKeepAlive = 30;
    config.mCleanSession = true;
    config.mPort = GATEWAY_MULTICAST_PORT;
    config.mAddress = &address;
    config.mRetransmissionCount = 3;
    config.mRetransmissionTimeout = 10;

    // Register connected callback
    otMqttsnSetConnectedHandler(instance, mqttsnHandleConnected, (void *)instance);

    LOG_DBG("Trying to connect");

    // Connect to the MQTT broker (gateway)
    otMqttsnConnect(instance, &config);
}

void mqttsnSearchGateway(otInstance *instance)
{
    otIp6Address address;
    otIp6AddressFromString(GATEWAY_MULTICAST_ADDRESS, &address);

    LOG_DBG("Searching for gateway on %s", GATEWAY_MULTICAST_ADDRESS);
    otLedToggle(LED_YELLOW);

    otMqttsnSetSearchgwHandler(instance, mqttsnHandleSearchGw, (void *)instance);
    // Send SEARCHGW multicast message
    otMqttsnSearchGateway(instance, &address, GATEWAY_MULTICAST_PORT, GATEWAY_MULTICAST_RADIUS);
}

void mqttsnPublishWorkHandler(struct k_work *work)
{
    static int count = 0;

	LOG_DBG("Publish Handler %d", _stateCount);
    otLedToggle(LED_YELLOW);

	otInstance *instance = openthread_get_default_instance();

    otMqttsnClientState state = otMqttsnGetState(instance);

    switch(state)
    {
        case kStateDisconnected:
            LOG_INF("Client is not connected to gateway");
            break;
        case kStateActive:
            LOG_INF("Client is connected to gateway and currently alive.");
            break;
        case kStateAsleep:
            LOG_INF("Client is in sleeping state.");
            break;
        case kStateAwake:
            LOG_INF("Client is awaken from sleep.");
            break;
        case kStateLost:
            LOG_INF("Client connection is lost due to communication error.");
            break;
    }

    if(state == kStateDisconnected || otMqttsnGetState(instance)  == kStateLost)
    {
        LOG_WRN("MQTT g/w disconnected or lost: %d", otMqttsnGetState(instance) );
        mqttsnSearchGateway(instance);
    }
    else
    {
        static int count = 0;
        
        LOG_DBG("Client state %d", otMqttsnGetState(instance));
        otLedToggle(LED_YELLOW);

        // Get ID
        otExtAddress extAddress;
        otLinkGetFactoryAssignedIeeeEui64(instance, &extAddress);

        struct bt_lns_client *lns = getLNSClient();
        struct ble_lns_loc_speed_s *lns_data = bt_lns_get_last_location_and_speed(lns);

        uint32_t latitude = lns_data->latitude;
        uint32_t longitude = lns_data->longitude;
        uint32_t elevation = lns_data->elevation;

        uint8_t gps_lock = lns_data->location_present;
        uint8_t battery = 100;

        otDeviceRole role = otThreadGetDeviceRole(instance);
        const char* triage_state = otThreadDeviceRoleToString(role);
        //char *triage_state = "P1";

        // Publish message to the registered topic
        LOG_INF("Publishing...");

        otLedToggle(LED_YELLOW);

 
        const char* strdata = "{\"ID\":%02x%02x%02x%02x%02x%02x%02x%02x, \"Count\":%d, \"Status\":\"%s\", \"Batt\":%d, \"Latitude\":%d, \"Longitude\",%d, \"Ele\":%d, \"Temp\":24.0}";
        char data[256];
        sprintf(data, strdata,
		    extAddress.m8[0],
		    extAddress.m8[1],
		    extAddress.m8[2],
		    extAddress.m8[3],
		    extAddress.m8[4],
		    extAddress.m8[5],
		    extAddress.m8[6],
		    extAddress.m8[7],
		    count++, 
            triage_state, 
            battery,
            gps_lock,
            latitude,
            longitude,
            elevation);
        
        int32_t length = strlen(data);

        otError err = otMqttsnPublish(instance, (const uint8_t*)data, length, kQos1, false, &_aTopic,
            mqttsnHandlePublished, NULL);

        LOG_DBG("Publishing %d bytes rsp %d", length, err);
        otLedToggle(LED_YELLOW);
    }

    // Restart timer
    k_timer_start(&mqttsnPublishTimer, K_SECONDS(10), K_NO_WAIT);
}

K_WORK_DEFINE(mqttsnPublishWork, mqttsnPublishWorkHandler);

void mqttsnPublishHandler(struct k_timer *dummy)
{
    k_work_submit(&mqttsnPublishWork);
}

otError mqttsnInit()
{
    otInstance *instance = openthread_get_default_instance();

    // Start MQTT-SN client
    LOG_INF("Starting MQTT-SN on port %d", CLIENT_PORT);
    otLedToggle(LED_YELLOW);
    
    otError error = otMqttsnStart(instance, CLIENT_PORT);

    /* start one shot timer that expires after 10s */
    if(error == OT_ERROR_NONE)
        k_timer_start(&mqttsnPublishTimer, K_SECONDS(10), K_NO_WAIT);

    return error;
}