#ifndef MQTTSN_H_

// Includes

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>

#include "openthread/instance.h"

// Definitions

#define GATEWAY_MULTICAST_PORT CONFIG_MQTT_SNCLIENT_GATEWAY_PORT
#define GATEWAY_MULTICAST_ADDRESS CONFIG_MQTT_SNCLIENT_GATEWAY_ADDRESS
#define GATEWAY_MULTICAST_RADIUS CONFIG_MQTT_SNCLIENT_HOPS

#define CLIENT_PREFIX CONFIG_MQTT_SNCLIENT_PREFIX
#define CLIENT_PORT CONFIG_MQTT_SNCLIENT_PORT

#define TOPIC_PREFIX CONFIG_MQTT_SNCLIENT_TOPIC_PREFIX

#define PUBLISH_INTERVAL_MS CONFIG_MQTT_SNCLIENT_PUBLISH_INTERVAL_S

// Prototypes

otError mqttsnInit(void);
void mqttsnSearchGateway(otInstance *instance);

#endif