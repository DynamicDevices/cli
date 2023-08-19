#include "mqttsn.h"

// Includes

#include <stdio.h>
#include <string.h>

#include "openthread/thread.h"
#include "openthread/mqttsn.h"
#include "openthread/link.h"

#include <zephyr/logging/log.h>

#include "gpio.h"
#include "app_bluetooth.h"
#include "bluetooth/lns_client.h"

#ifdef CONFIG_NRFX_TEMP
#include <nrfx_temp.h>
#endif

#include "app.h"

// Definitions

// Enumerations

enum MQTTSN_CLIENT_STATE {
    STATE_NONE = 0,
    STATE_CONNECTING = 1,
    STATE_REGISTERING_PUB_TOPIC = 2,
    STATE_SUBSCRIBING = 4,
    STATE_RUNNING = 5,
};

// Prototypes

void mqttsnPublishHandler(struct k_timer *dummy);

// Globals

static char _eui64[16+1];

static otMqttsnTopic _aTopicPub;
static K_TIMER_DEFINE(mqttsnPublishTimer, mqttsnPublishHandler, NULL);
static uint32_t _stateCount = 0;
static enum MQTTSN_CLIENT_STATE _eMQTTSNClientState = STATE_NONE;

// Functions

LOG_MODULE_REGISTER(mqttsn, CONFIG_MQTT_SNCLIENT_LOG_LEVEL);

// Support functions

static void mqttsnSubscribedHandler(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, otMqttsnQos aQos, void* aContext)
{
 // Handle registered
    if (aCode == kCodeAccepted)
    {
        if(aTopic->mType == kTopicId || aTopic->mType == kPredefinedTopicId)
            LOG_DBG("Subscribed OK ID %d", aTopic->mData.mTopicId);
        else
            LOG_DBG("Subscribed OK Name %s", aTopic->mData.mTopicName);
    }
    else
    {
        if(aTopic->mType == kTopicId || aTopic->mType == kPredefinedTopicId)
            LOG_WRN("Subscribed Error ID %d", aTopic->mData.mTopicId);
        else
            LOG_WRN("Subscribed Error Name %s", aTopic->mData.mTopicName);
    }
}

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
    // Handle registered - TODO: Fix support for short topic name
    if (aCode == kCodeAccepted)
    {
        if(aTopic->mType == kTopicId || aTopic->mType == kPredefinedTopicId)
            LOG_DBG("Registered OK ID %d", aTopic->mData.mTopicId);
        else
            LOG_DBG("Registered OK Name %s", aTopic->mData.mTopicName);
    }
    else
    {
        if(aTopic->mType == kTopicId || aTopic->mType == kPredefinedTopicId)
            LOG_WRN("Registered Error ID %d", aTopic->mData.mTopicId);
        else
            LOG_WRN("Registered Error Name %s", aTopic->mData.mTopicName);
    }

    otInstance *instance = (otInstance *)aContext;

    switch(_eMQTTSNClientState)
    {
        case STATE_NONE:
        case STATE_CONNECTING:
        case STATE_SUBSCRIBING:
        case STATE_RUNNING:
            LOG_WRN("Got invalid client state in registration handler: %d", _eMQTTSNClientState);
            break;

        case STATE_REGISTERING_PUB_TOPIC:

            // We've done registering the Publication topic now Subscribe to the subscriptitopic
            
            otLedToggle(LED_YELLOW);
            memcpy(&_aTopicPub, aTopic, sizeof(otMqttsnTopic));

            // Build topic
            char data[128];
            sprintf(data, "%s/%s/cmnd", TOPIC_PREFIX, _eui64);

            otMqttsnTopic aTopicSub;
            aTopicSub.mType = kTopicName;
            aTopicSub.mData.mTopicName = data;

            LOG_DBG("Subscribing to topic: %s", data);
            otLedToggle(LED_YELLOW);

            _eMQTTSNClientState = STATE_SUBSCRIBING;
            otMqttsnSubscribe(instance, &aTopicSub, kQos0, mqttsnSubscribedHandler, (void *)instance);
            break;
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

        // Build topic
        char data[128];
        sprintf(data, "%s/%s", TOPIC_PREFIX, _eui64);

        LOG_DBG("Registering Topic: %s", data);
        otLedToggle(LED_YELLOW);

        // Obtain target topic ID
        _eMQTTSNClientState = STATE_REGISTERING_PUB_TOPIC;
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

static otMqttsnReturnCode mqttsnHandlePublishReceived(const uint8_t* aPayload, int32_t aPayloadLength, const otMqttsnTopic* aTopic, void* aContext)
{
    char buffer[128];

    memset(buffer, 0, sizeof(buffer));

    // Truncate to buffer as not zero terminated and we're going to log this data
    if(aPayloadLength > sizeof(buffer))
        aPayloadLength = sizeof(buffer)-1;
    snprintf(buffer, aPayloadLength+1, "%s", aPayload);

    if(aTopic->mType == kTopicId || aTopic->mType == kPredefinedTopicId)
        LOG_DBG("Received message on topic ID %d: %s", aTopic->mData.mTopicId, buffer);
    else
        LOG_DBG("Registered message on topic name %s: %s", aTopic->mData.mTopicName, buffer);

    if(strstr(buffer, "identify") != NULL)
    {
        // TODO: Shouldn't really block but this should be called rarely...
        LOG_INF("Identify board");
        int cycles = 20;
        while(cycles--)
        {
            otLedToggle(LED_YELLOW);
	        k_sleep(K_MSEC(250));
        }
    }
    return kCodeAccepted;
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

    // Build Client ID
    char data[256];
    sprintf(data, "%s-%s", CLIENT_PREFIX, _eui64);

    config.mClientId = data;
    config.mKeepAlive = 30;
    config.mCleanSession = true;
    config.mPort = GATEWAY_MULTICAST_PORT;
    config.mAddress = &address;
    config.mRetransmissionCount = 3;
    config.mRetransmissionTimeout = 10;

    // Register connected callback
    otMqttsnSetConnectedHandler(instance, mqttsnHandleConnected, (void *)instance);

    // Register publish received callback
    otMqttsnSetPublishReceivedHandler(instance, mqttsnHandlePublishReceived, (void *)instance);

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

        int32_t whole_celsius = 0;
        uint8_t fraction_celsius = 0;

        // Get RLOC16
        uint16_t uRLOC16 = otLinkGetShortAddress(instance);

#ifdef CONFIG_NRFX_TEMP
        // Get temperature
        nrfx_err_t status = nrfx_temp_measure();
        if(status != NRFX_SUCCESS)
            LOG_WRN("Error reading temperature: %d", status);
            
        int32_t temperature = nrfx_temp_result_get();
        int32_t celsius_temperature = nrfx_temp_calculate(temperature);

        whole_celsius = celsius_temperature / 100;
        fraction_celsius = NRFX_ABS(celsius_temperature % 100);

        LOG_INF("Measured temperature: %d.%02u [C]", whole_celsius, fraction_celsius);
#endif

        // Get GNSS data
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
 
        const char* strdata = "{\"ID\":\"%s\", \"RLOC16\":\"%04X\", \"Version\":\"%s\", \"Count\":%d, \"Status\":\"%s\", \"Battery\":%d, \"GPSLock\": %d, \"Latitude\":%d, \"Longitude\":%d, \"Elevation\":%d, \"Temperature\":%d.%02u }";
        char data[256];
        sprintf(data, strdata, _eui64,
            uRLOC16,
            VERSION,
		    count++,
            triage_state, 
            battery,
            gps_lock,
            latitude,
            longitude,
            elevation,
            whole_celsius, fraction_celsius 
            );
        
        int32_t length = strlen(data);

        otError err = otMqttsnPublish(instance, (const uint8_t*)data, length, kQos1, false, &_aTopicPub,
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

    // Store EUI64
    otExtAddress extAddress;
    otLinkGetFactoryAssignedIeeeEui64(instance, &extAddress);
    snprintf(_eui64, sizeof(_eui64), "%02x%02x%02x%02x%02x%02x%02x%02x",
            extAddress.m8[0],
            extAddress.m8[1],
            extAddress.m8[2],
            extAddress.m8[3],
            extAddress.m8[4],
            extAddress.m8[5],
            extAddress.m8[6],
            extAddress.m8[7]
            );

    otError error = otMqttsnStart(instance, CLIENT_PORT);

    /* start one shot timer that expires after 10s */
    if(error == OT_ERROR_NONE)
        k_timer_start(&mqttsnPublishTimer, K_SECONDS(10), K_NO_WAIT);

    return error;
}