#include "mqttsn.h"

// Includes

#include <stdio.h>

#include "openthread/mqttsn.h"
#include "openthread/link.h"

#include <zephyr/logging/log.h>

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
}

static void mqttsnHandleRegistered(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, void* aContext)
{
    // Handle registered
//    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        LOG_DBG("HandleRegistered - OK");
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
        LOG_DBG("HandleConnected -Accepted");

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

        // Obtain target topic ID
        otMqttsnRegister(instance, data, mqttsnHandleRegistered, (void *)instance);
    }
    else
    {
        LOG_WRN("HandleConnected - Error");
    }
}

static void mqttsnHandleSearchGw(const otIp6Address* aAddress, uint8_t aGatewayId, void* aContext)
{
    OT_UNUSED_VARIABLE(aGatewayId);

    LOG_DBG("Got search gateway response");

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
    // Connect to the MQTT broker (gateway)
    otMqttsnConnect(instance, &config);
}

void mqttsnSearchGateway(otInstance *instance)
{
    otIp6Address address;
    otIp6AddressFromString(GATEWAY_MULTICAST_ADDRESS, &address);

    LOG_DBG("Searching for gateway on %s", GATEWAY_MULTICAST_ADDRESS);

    otMqttsnSetSearchgwHandler(instance, mqttsnHandleSearchGw, (void *)instance);
    // Send SEARCHGW multicast message
    otMqttsnSearchGateway(instance, &address, GATEWAY_MULTICAST_PORT, GATEWAY_MULTICAST_RADIUS);
}

void mqttsnPublishWorkHandler(struct k_work *work)
{
    static int count = 0;

	LOG_DBG("Publish Handler %d", _stateCount);

	otInstance *instance = openthread_get_default_instance();

#if 0
    _stateCount++;

    if(_stateCount == 3)
    {
    	LOG_WRN("*** STOP THREAD");
        otThreadSetEnabled(instance, false);
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }
    if(_stateCount == 6)
    {
    	LOG_WRN("*** START BLUETOOTH SCAN");
    	bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }

    if(_stateCount == 9)
    {
    	LOG_WRN("*** STOPPING BLUETOOTH SCAN");
        bt_scan_stop();
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }
    if(_stateCount == 12)
    {
    	LOG_WRN("*** STARTING THREAD");    
        otThreadSetEnabled(instance, true);
        stateCount = 0;
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }
#endif

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
        // Get ID
        otExtAddress extAddress;
        otLinkGetFactoryAssignedIeeeEui64(instance, &extAddress);

// Disable Bluetooth for now
//        struct ble_lns_loc_speed_s *lns_data = bt_lns_get_last_location_and_speed(&lns);

        uint32_t latitude = 0;
        uint32_t longitude = 0;
        uint32_t elevation = 0;
        uint8_t battery = 100;
        char *triage_state = "P1";

        // Publish message to the registered topic
        LOG_INF("Publishing...");
        const char* strdata = "{\"id\":%02x%02x%02x%02x%02x%02x%02x%02x, \"count\":%d, \"status\":%s, \"batt\":%d, \"lat\":%d, \"lon\",%d, \"ele\":%d, \"temp\":24.0}";
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
            latitude,
            longitude,
            elevation);
        
        int32_t length = strlen(data);

        otError err = otMqttsnPublish(instance, (const uint8_t*)data, length, kQos1, false, &_aTopic,
            mqttsnHandlePublished, NULL);

        LOG_DBG("Publishing %d bytes rsp %d", length, err);
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
    otError error = otMqttsnStart(instance, CLIENT_PORT);

    /* start one shot timer that expires after 10s */
    if(error == OT_ERROR_NONE)
        k_timer_start(&mqttsnPublishTimer, K_SECONDS(10), K_NO_WAIT);

    return error;
}