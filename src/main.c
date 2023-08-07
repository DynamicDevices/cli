/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>

#include <openthread/config.h>

#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/logging.h>
#include <openthread/platform/logging.h>

#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/tasklet.h"
#include "openthread/ip6.h"
#include "openthread/mqttsn.h"
#include "openthread/dataset.h"
#include "openthread/link.h"
#include "openthread-system.h"

#if defined(CONFIG_BT)
#include "ble.h"
#endif

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
#include "low_power.h"
#endif

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include "bluetooth/lns_client.h"
#include <zephyr/settings/settings.h>

#define NETWORK_NAME "OTBR4444"
#define PANID 0x4444
#define EXTPANID {0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44}

#define GATEWAY_MULTICAST_PORT 10000
#define GATEWAY_MULTICAST_ADDRESS "ff03::1"
#define GATEWAY_MULTICAST_RADIUS 8

#define CLIENT_PREFIX "tc-"
#define CLIENT_PORT 10000

#define TOPIC_PREFIX "ot/alex"

static const uint8_t sExpanId[] = EXTPANID;
static const uint8_t sMasterKey[] = MASTER_KEY;

#define PUBLISH_INTERVAL_MS 10000

void mqttsn_publish_handler(struct k_timer *dummy);
K_TIMER_DEFINE(mqttsn_publish_timer, mqttsn_publish_handler, NULL);

LOG_MODULE_REGISTER(cli_sample, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

#define WELLCOME_TEXT \
	"\n\r"\
	"\n\r"\
	"OpenThread Command Line Interface is now running.\n\r" \
	"Use the 'ot' keyword to invoke OpenThread commands e.g. " \
	"'ot thread start.'\n\r" \
	"For the full commands list refer to the OpenThread CLI " \
	"documentation at:\n\r" \
	"https://github.com/openthread/openthread/blob/master/src/cli/README.md\n\r"

// Bluetooth
#define LNS_READ_VALUE_INTERVAL 10000
static struct bt_conn *default_conn;
static struct bt_lns_client lns;

// Prototypes
int init_bluetooth(void);

// Support functions
static void HandlePublished(otMqttsnReturnCode aCode, void* aContext)
{
    OT_UNUSED_VARIABLE(aCode);
    OT_UNUSED_VARIABLE(aContext);

    // Handle published
    otLogWarnPlat("Published");
}

otMqttsnTopic _aTopic;

static void HandleRegistered(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, void* aContext)
{
    // Handle registered
//    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        otLogWarnPlat("HandleRegistered - OK");
        memcpy(&_aTopic, aTopic, sizeof(otMqttsnTopic));
    }
    else
    {
        otLogWarnPlat("HandleRegistered - Error");
    }
}

static void HandleConnected(otMqttsnReturnCode aCode, void* aContext)
{
    // Handle connected
    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        otLogWarnPlat("HandleConnected -Accepted");

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

        otLogWarnPlat("Registering Topic: %s", data);

        // Obtain target topic ID
        otMqttsnRegister(instance, data, HandleRegistered, (void *)instance);
    }
    else
    {
        otLogWarnPlat("HandleConnected - Error");
    }
}

static void HandleSearchGw(const otIp6Address* aAddress, uint8_t aGatewayId, void* aContext)
{
    OT_UNUSED_VARIABLE(aGatewayId);

    otLogWarnPlat("Got search gateway response");

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
    otMqttsnSetConnectedHandler(instance, HandleConnected, (void *)instance);
    // Connect to the MQTT broker (gateway)
    otMqttsnConnect(instance, &config);
}

static void SearchGateway(otInstance *instance)
{
    otIp6Address address;
    otIp6AddressFromString(GATEWAY_MULTICAST_ADDRESS, &address);

    otLogWarnPlat("Searching for gateway on %s", GATEWAY_MULTICAST_ADDRESS);

    otMqttsnSetSearchgwHandler(instance, HandleSearchGw, (void *)instance);
    // Send SEARCHGW multicast message
    otMqttsnSearchGateway(instance, &address, GATEWAY_MULTICAST_PORT, GATEWAY_MULTICAST_RADIUS);
}

static void StateChanged(otChangedFlags aFlags, void *aContext)
{
    otInstance *instance = (otInstance *)aContext;

     otLogWarnPlat("State Changed");

    // when thread role changed
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        // If role changed to any of active roles then send SEARCHGW message
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER)
        {
            SearchGateway(instance);
        }
    }
}

int main(int aArgc, char *aArgv[])
{
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	int ret;
	const struct device *dev;
	uint32_t dtr = 0U;

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (dev == NULL) {
		LOG_ERR("Failed to find specific UART device");
		return 0;
	}

	LOG_INF("Waiting for host to be ready to communicate");

	/* Data Terminal Ready - check if host is ready to communicate */
	while (!dtr) {
		ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (ret) {
			LOG_ERR("Failed to get Data Terminal Ready line state: %d",
				ret);
			continue;
		}
		k_msleep(100);
	}

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#endif

	LOG_INF(WELLCOME_TEXT);

#if defined(CONFIG_BT)
//	ble_enable();
#endif

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
	low_power_enable();
#endif

	// New code
	otInstance *instance;
    otError error = OT_ERROR_NONE;
    otExtendedPanId extendedPanid;
    otNetworkKey masterKey;

    instance = openthread_get_default_instance();

	// Set default network settings
    // Set network name
    LOG_INF("Setting Network Name to %s", NETWORK_NAME);
    error = otThreadSetNetworkName(instance, NETWORK_NAME);
    // Set extended PANID
    memcpy(extendedPanid.m8, sExpanId, sizeof(sExpanId));
    LOG_INF("Setting PANID to 0x%04X", PANID);
    error = otThreadSetExtendedPanId(instance, &extendedPanid);
    // Set PANID
    LOG_INF("Setting Extended PANID");
    error = otLinkSetPanId(instance, PANID);
    // Set channel
    LOG_INF("Setting Channel to %d", DEFAULT_CHANNEL);
    error = otLinkSetChannel(instance, DEFAULT_CHANNEL);
    // Set masterkey
    LOG_INF("Setting Network Key");
    memcpy(masterKey.m8, sMasterKey, sizeof(sMasterKey));
    error = otThreadSetNetworkKey(instance, &masterKey);

    // Register notifier callback to receive thread role changed events
    error = otSetStateChangedCallback(instance, StateChanged, instance);

    // Start thread network
    otIp6SetSlaacEnabled(instance, true);
    error = otIp6SetEnabled(instance, true);
    error = otThreadSetEnabled(instance, true);

    init_bluetooth();

    // Start MQTT-SN client
    LOG_INF("Starting MQTT-SN on port %d", CLIENT_PORT);
    error = otMqttsnStart(instance, CLIENT_PORT);

    /* start one shot timer that expires after 10s */
    k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);

    return 0;
}

static uint32_t stateCount = 0;

void publish_work_handler(struct k_work *work)
{
	LOG_WRN("Publish Handler %d", stateCount);

	otInstance *instance = openthread_get_default_instance();

#if 0
    stateCount++;

    if(stateCount == 3)
    {
    	LOG_WRN("*** STOP THREAD");
        otThreadSetEnabled(instance, false);
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }
    if(stateCount == 6)
    {
    	LOG_WRN("*** START BLUETOOTH SCAN");
    	bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }

    if(stateCount == 9)
    {
    	LOG_WRN("*** STOPPING BLUETOOTH SCAN");
        bt_scan_stop();
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }
    if(stateCount == 12)
    {
    	LOG_WRN("*** STARTING THREAD");    
        otThreadSetEnabled(instance, true);
        stateCount = 0;
        k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
        return;
    }
#endif

    if(otMqttsnGetState(instance) == kStateDisconnected || otMqttsnGetState(instance)  == kStateLost)
    {
        LOG_INF("MQTT g/w disconnected or lost: %d", otMqttsnGetState(instance) );
        SearchGateway(instance);
    }
    else
    {
        static int count = 0;
        
        LOG_INF("Client state %d", otMqttsnGetState(instance));

        // Get ID
        otExtAddress extAddress;
        otLinkGetFactoryAssignedIeeeEui64(instance, &extAddress);

        struct ble_lns_loc_speed_s *lns_data = bt_lns_get_last_location_and_speed(&lns);

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
            "P1", 
            100,
            lns_data->latitude,
            lns_data->longitude,
            lns_data->elevation);
        
        int32_t length = strlen(data);

        otError err = otMqttsnPublish(instance, (const uint8_t*)data, length, kQos1, false, &_aTopic,
            HandlePublished, NULL);

        LOG_INF("Publishing %d bytes rsp %d", length, err);
    }

    // Restart timer
    k_timer_start(&mqttsn_publish_timer, K_SECONDS(10), K_NO_WAIT);
}

K_WORK_DEFINE(publish_work, publish_work_handler);

void mqttsn_publish_handler(struct k_timer *dummy)
{
    k_work_submit(&publish_work);
}

// Bluetooth code

static void notify_location_and_speed_cb(struct bt_lns_client *lns,
				    struct ble_lns_loc_speed_s *lns_data);

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	LOG_INF("Filters matched. Address: %s connectable: %s",
		addr, connectable ? "yes" : "no");
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	LOG_WRN("Connecting failed");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	default_conn = bt_conn_ref(conn);
}

static void scan_filter_no_match(struct bt_scan_device_info *device_info,
				 bool connectable)
{
	int err;
	struct bt_conn *conn;
	char addr[BT_ADDR_LE_STR_LEN];

	if (device_info->recv_info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_addr_le_to_str(device_info->recv_info->addr, addr,
				  sizeof(addr));
		LOG_INF("Direct advertising received from %s", addr);
		bt_scan_stop();

		err = bt_conn_le_create(device_info->recv_info->addr,
					BT_CONN_LE_CREATE_CONN,
					device_info->conn_param, &conn);

		if (!err) {
			default_conn = bt_conn_ref(conn);
			bt_conn_unref(conn);
		}
	}
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match,
		scan_connecting_error, scan_connecting);

static void discovery_completed_cb(struct bt_gatt_dm *dm,
				   void *context)
{
	int err;

	LOG_INF("The discovery procedure succeeded");

	bt_gatt_dm_data_print(dm);

	err = bt_lns_handles_assign(dm, &lns);
	if (err) {
		LOG_WRN("Could not init LNS client object, error: %d", err);
	}

	if (bt_lns_notify_supported(&lns)) {
		err = bt_lns_subscribe_location_and_speed(&lns,
						     notify_location_and_speed_cb);
		if (err) {
			LOG_WRN("Cannot subscribe to LNS value notification "
				"(err: %d)", err);
			/* Continue anyway */
		}
	} else {
		err = bt_lns_start_per_read_location_and_speed(
			&lns, LNS_READ_VALUE_INTERVAL, notify_location_and_speed_cb);
		if (err) {
			LOG_WRN("Could not start periodic read of LNS value");
		}
	}

	err = bt_gatt_dm_data_release(dm);
	if (err) {
		LOG_WRN("Could not release the discovery data, error "
		       "code: %d", err);
	}
}

static void discovery_service_not_found_cb(struct bt_conn *conn,
					   void *context)
{
	LOG_WRN("The service could not be found during the discovery");
}

static void discovery_error_found_cb(struct bt_conn *conn,
				     int err,
				     void *context)
{
	LOG_WRN("The discovery procedure failed with %d", err);
}

static struct bt_gatt_dm_cb discovery_cb = {
	.completed = discovery_completed_cb,
	.service_not_found = discovery_service_not_found_cb,
	.error_found = discovery_error_found_cb,
};

static void gatt_discover(struct bt_conn *conn)
{
	int err;

	if (conn != default_conn) {
		return;
	}

	err = bt_gatt_dm_start(conn, BT_UUID_LNS, &discovery_cb, NULL);
	if (err) {
		LOG_WRN("Could not start the discovery procedure, error "
		       "code: %d", err);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_WRN("Failed to connect to %s (%u)", addr, conn_err);
		if (conn == default_conn) {
			bt_conn_unref(default_conn);
			default_conn = NULL;

			/* This demo doesn't require active scan */
			err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
			if (err) {
				LOG_WRN("Scanning failed to start (err %d)",
				       err);
			}
		}

		return;
	}

	err = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (err) {
		LOG_WRN("Failed to set security: %d", err);

		gatt_discover(conn);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Disconnected: %s (reason %u)", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_WRN("Scanning failed to start (err %d)", err);
	}
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_WRN("Security changed: %s level %u", addr, level);
	} else {
		LOG_WRN("Security failed: %s level %u err %d", addr, level,
			err);
	}

	gatt_discover(conn);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed
};

static void scan_init(void)
{
	int err;

	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
		.scan_param = NULL,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_LNS);
	if (err) {
		LOG_WRN("Scanning filters cannot be set (err %d)", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		LOG_WRN("Filters cannot be turned on (err %d)", err);
	}
}

static void notify_location_and_speed_cb(struct bt_lns_client *lns,
				    struct ble_lns_loc_speed_s *lns_data)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(bt_lns_conn(lns)),
			  addr, sizeof(addr));
	if (lns_data == NULL) {
		LOG_WRN("[%s] Speed and Location notification aborted", addr);
	} else {

		LOG_INF("[%s] Speed and Location notification: Speed: %u, Lat: %d, Long: %d, Ele: %d",
		       addr, 
               lns_data->instant_speed,
               lns_data->latitude,
               lns_data->longitude,
               lns_data->elevation);
	}
}

#if 0
static void read_speed_and_location_cb(struct bt_lns_client *lns,
				  uint16_t flags,
				  int err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(bt_lns_conn(lns)),
			  addr, sizeof(addr));
	if (err) {
		LOG_WRN("[%s] Speed and location read ERROR: %d", addr, err);
		return;
	}

	LOG_INF("[%s] Speed and location read: %"PRIu8"%%", addr, flags);
}

static void button_readval(void)
{
	int err;

	LOG_INF("Reading LNS value:");
	err = bt_lns_read_battery_level(&lns, read_speed_and_location_cb);
	if (err) {
		LOG_WRN("LNS read call error: %d", err);
	}
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;

	if (button & KEY_READVAL_MASK) {
		button_readval();
	}
}
#endif

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing cancelled: %s", addr);
}


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing failed conn: %s, reason %d", addr, reason);
}


static struct bt_conn_auth_cb conn_auth_callbacks = {
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};

int init_bluetooth(void)
{
    int err;

	LOG_INF("Starting Bluetooth Central LNS example");
    
	bt_lns_client_init(&lns);

	err = bt_enable(NULL);
	if (err) {
		LOG_WRN("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	scan_init();

	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_WRN("Failed to register authorization callbacks.");
		return 0;
	}

	err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	if (err) {
		LOG_WRN("Failed to register authorization info callbacks.");
		return 0;
	}

#if 0
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_WRN("Scanning failed to start (err %d)", err);
		return 0;
	}

	LOG_INF("Scanning successfully started\n");
#endif

	return 0;
}
