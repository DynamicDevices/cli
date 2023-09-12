
// Includes

#include "app_bluetooth.h"

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include "bluetooth/lns_client.h"

// Definitions

// Statics

LOG_MODULE_REGISTER(app_bluetooth, CONFIG_APP_BLUETOOTH_LOG_LEVEL);

static struct bt_conn *default_conn;
static struct bt_lns_client lns;

// Prototypes

// Bluetooth code

struct bt_lns_client *getLNSClient()
{
	return &lns;
}

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

int appbluetoothInit(void)
{
    int err;

	LOG_INF("Starting Bluetooth Central LNS example");
    k_msleep(1000);
    
//	bt_lns_client_init(&lns);

	err = bt_enable(NULL);
	if (err) {
		LOG_WRN("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");
    k_msleep(1000);

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

#if 0
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
#endif

	LOG_INF("Bluetooth start scan");
    k_msleep(5000);

#if 1
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_WRN("Scanning failed to start (err %d)", err);
		return 0;
	}

	LOG_INF("Scanning successfully started\n");
#endif

	return 0;
}
