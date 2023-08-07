/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "lns_client.h"

#include <zephyr/logging/log.h>

// TODO: Configure this
LOG_MODULE_REGISTER(lns_client, 4);

/**
 * @brief Process location and speed value notification
 *
 * Internal function to process report notification and pass it further.
 *
 * @param conn   Connection handler.
 * @param params Notification parameters structure - the pointer
 *               to the structure provided to subscribe function.
 * @param data   Pointer to the data buffer.
 * @param length The size of the received data.
 *
 * @retval BT_GATT_ITER_STOP     Stop notification
 * @retval BT_GATT_ITER_CONTINUE Continue notification
 */
static uint8_t notify_process(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length)
{
	struct bt_lns_client *lns;
	const uint8_t *bdata = data;
	struct ble_lns_loc_speed_s lns_data;

    LOG_INF("notify_process");

	lns = CONTAINER_OF(params, struct bt_lns_client, notify_params);
	if (!data || !length) {
		LOG_INF("Notifications disabled.");
		if (lns->notify_location_and_speed_cb) {
			lns->notify_location_and_speed_cb(lns, BT_LNS_VAL_INVALID);
		}
		return BT_GATT_ITER_STOP;
	}

	uint16_t flags = *((uint16_t *)&data[0]);
    LOG_INF("Length: %d", length);
    LOG_INF("Flags 0x%04X", flags);

    LOG_INF("Data: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
	 bdata[0], bdata[1], bdata[2], bdata[3], bdata[4], bdata[5], bdata[6], bdata[7], bdata[8], bdata[9]
	);
    LOG_INF("Data: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
	 bdata[10], bdata[11], bdata[12], bdata[13], bdata[14], bdata[15], bdata[16], bdata[17], bdata[18], bdata[19]
	);
	
	memset(&lns_data, 0, sizeof(struct ble_lns_loc_speed_s));

	lns_data.instant_speed_present = (flags & 0x01) == 0x01;
    lns_data.total_distance_present = (flags & 0x02) == 0x02;
    lns_data.location_present = (flags & 0x04) == 0x04;
    lns_data.elevation_present = (flags & 0x08) == 0x08;
    lns_data.heading_present = (flags & 0x10) == 0x10;
    lns_data.rolling_time_present = (flags & 0x20) == 0x20;
    lns_data.utc_time_time_present = (flags & 0x40) == 0x40;

	const uint8_t *pData = &bdata[2];

	if(lns_data.instant_speed_present)
	{
		lns_data.instant_speed = *((uint16_t *)pData);
		pData += 2;
	}
	if(lns_data.total_distance_present)
	{
		// Ignore this for now
		pData += 3;		
	}
	if(lns_data.location_present)
	{
		// Latitude - Unit is in degrees with a resolution of 1/(10^7)
		lns_data.latitude = *((int32_t *)pData);
		pData += 4;

		// Longitude - Unit is in degrees with a resolution of 1/(10^7)
		lns_data.longitude = *((int32_t *)pData);
		pData += 4;
	}
	if(lns_data.elevation_present)
	{
		// Ignore this for now
		pData += 3;
	}
	if(lns_data.heading_present)
	{
		lns_data.heading = *((uint16_t *)pData);
		pData += 2;
	}
	if(lns_data.rolling_time_present)
	{
		lns_data.rolling_time = *((uint8_t *)pData);
		pData += 1;
	}
	if(lns_data.utc_time_time_present)
	{
		// Smallest units in seconds

		// YYMMDDHHMMSS

		// Ignore this for now
	}

	if (flags == BT_LNS_VAL_INVALID) {
		LOG_ERR("Unexpected notification value.");
		if (lns->notify_location_and_speed_cb) {
			lns->notify_location_and_speed_cb(lns, NULL);
		}
		return BT_GATT_ITER_STOP;
	}

	// Need to copy across LNS data here
	memcpy(&lns->lns_data, &lns_data, sizeof(lns_data));

	if (lns->notify_location_and_speed_cb) {
		lns->notify_location_and_speed_cb(lns, &lns_data);
	}

	return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Process speed and location value read
 *
 * Internal function to process report read and pass it further.
 *
 * @param conn   Connection handler.
 * @param err    Read ATT error code.
 * @param params Notification parameters structure - the pointer
 *               to the structure provided to read function.
 * @param data   Pointer to the data buffer.
 * @param length The size of the received data.
 *
 * @retval BT_GATT_ITER_STOP     Stop notification
 * @retval BT_GATT_ITER_CONTINUE Continue notification
 */
static uint8_t read_process(struct bt_conn *conn, uint8_t err,
			     struct bt_gatt_read_params *params,
			     const void *data, uint16_t length)
{
	struct bt_lns_client *lns;
	uint16_t flags = BT_LNS_VAL_INVALID;
	const uint8_t *bdata = data;

    LOG_WRN("TODO: read_process");

#if 0
	lns = CONTAINER_OF(params, struct bt_lns_client, read_params);

	if (!lns->read_cb) {
		LOG_ERR("No read callback present");
	} else  if (err) {
		LOG_ERR("Read value error: %d", err);
		lns->read_cb(lns,  flags, err);
    }

    LOG_INF("Length: %d", length);
    LOG_INF("Flags 0x%04X", *((uint16_t *) data) );

	lns->flags = flags;
			lns->read_cb(lns, flags, err);
		}
	}
#endif

	lns->read_cb = NULL;

	return BT_GATT_ITER_STOP;
}

/**
 * @brief Process periodic location and speed value read
 *
 * Internal function to process report read and pass it further.
 * And the end the new read request is triggered.
 *
 * @param conn   Connection handler.
 * @param err    Read ATT error code.
 * @param params Notification parameters structure - the pointer
 *               to the structure provided to read function.
 * @param data   Pointer to the data buffer.
 * @param length The size of the received data.
 *
 * @retval BT_GATT_ITER_STOP     Stop notification
 * @retval BT_GATT_ITER_CONTINUE Continue notification
 */
static uint8_t periodic_read_process(struct bt_conn *conn, uint8_t err,
				  struct bt_gatt_read_params *params,
				  const void *data, uint16_t length)
{
	int32_t interval;
	struct bt_lns_client *lns;
	const uint8_t *bdata = data;

    LOG_WRN("TODO: periodic_read_process");

	lns = CONTAINER_OF(params, struct bt_lns_client,
			periodic_read.params);

	if (!lns->notify_location_and_speed_cb) {
		LOG_ERR("No notification callback present");
	} else  if (err) {
		LOG_ERR("Read value error: %d", err);
    }

    LOG_INF("Length: %d", length);
    LOG_INF("Flags 0x%04X", *((uint16_t *)data));

#if 0
	} else if (!data || length != 1) {
		LOG_ERR("Unexpected read value size.");
	} else {
		battery_level = bdata[0];
		if (battery_level > BT_BAS_VAL_MAX) {
			LOG_ERR("Unexpected read value.");
		} else if (bas->battery_level != battery_level) {
			bas->battery_level = battery_level;
			bas->notify_cb(bas, battery_level);
		} else {
			/* Do nothing. */
		}
	}
#endif

	interval = atomic_get(&lns->periodic_read.interval);
	if (interval) {
		k_work_schedule(&lns->periodic_read.read_work,
				K_MSEC(interval));
	}
	return BT_GATT_ITER_STOP;
}


/**
 * @brief Periodic read workqueue handler.
 *
 * @param work Work instance.
 */
static void lns_read_value_handler(struct k_work *work)
{
	int err;
	struct bt_lns_client *lns;

    LOG_INF("lns_read_value_handler");

	lns = CONTAINER_OF(work, struct bt_lns_client,
			     periodic_read.read_work);

	if (!atomic_get(&lns->periodic_read.interval)) {
		/* disabled */
		return;
	}

	if (!lns->conn) {
		LOG_ERR("No connection object.");
		return;
	}

	lns->periodic_read.params.func = periodic_read_process;
	lns->periodic_read.params.handle_count  = 1;
	lns->periodic_read.params.single.handle = lns->val_handle;
	lns->periodic_read.params.single.offset = 0;

	err = bt_gatt_read(lns->conn, &lns->periodic_read.params);

	/* Do not treats reading after disconnection as error.
	 * Periodic read process is stopped after disconnection.
	 */
	if (err) {
		LOG_ERR("Periodic Location And Speed characteristic read error: %d",
			err);
	}
}


/**
 * @brief Reinitialize the LNS Client.
 *
 * @param lns LNS Client object.
 */
static void lns_reinit(struct bt_lns_client *lns)
{
    LOG_INF("lns_reinit");

	lns->ccc_handle = 0;
	lns->val_handle = 0;
	memset(&lns->lns_data, 0, sizeof(lns->lns_data));
	lns->conn = NULL;
	lns->notify_location_and_speed_cb = NULL;
	lns->read_cb = NULL;
	lns->notify = false;
}


void bt_lns_client_init(struct bt_lns_client *lns)
{
	memset(lns, 0, sizeof(*lns));

    LOG_INF("lns_client_init");

	memset(&lns->lns_data, 0, sizeof(struct ble_lns_loc_speed_s));

	k_work_init_delayable(&lns->periodic_read.read_work,
			      lns_read_value_handler);
}


int bt_lns_handles_assign(struct bt_gatt_dm *dm,
				 struct bt_lns_client *lns)
{
	const struct bt_gatt_dm_attr *gatt_service_attr =
			bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service =
			bt_gatt_dm_attr_service_val(gatt_service_attr);
	const struct bt_gatt_dm_attr *gatt_chrc;
	const struct bt_gatt_dm_attr *gatt_desc;
	const struct bt_gatt_chrc *chrc_val;

    LOG_INF("lns_handles_assign");

	if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_LNS)) {
		return -ENOTSUP;
	}
	LOG_DBG("Getting handles from locationa and speed service.");

	/* If connection is established again, cancel previous read request. */
	k_work_cancel_delayable(&lns->periodic_read.read_work);
	/* When workqueue is used its instance cannont be cleared. */
	lns_reinit(lns);

#define BT_UUID_LNS_LOCATION_AND_SPEED_VAL 0x2A67
#define BT_UUID_LNS_LOCATION_AND_SPEED BT_UUID_DECLARE_16(BT_UUID_LNS_LOCATION_AND_SPEED_VAL)
	/* SPEED and Location characteristic */
	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_LNS_LOCATION_AND_SPEED);
	if (!gatt_chrc) {
		LOG_ERR("No speed and location characteristic found.");
		return -EINVAL;
	}
	chrc_val = bt_gatt_dm_attr_chrc_val(gatt_chrc);
	__ASSERT_NO_MSG(chrc_val); /* This is internal function and it has to
				    * be called with characteristic attribute
				    */
	lns->properties = chrc_val->properties;
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc,
					    BT_UUID_LNS_LOCATION_AND_SPEED);
	if (!gatt_desc) {
		LOG_ERR("No speed and location characteristic value found.");
		return -EINVAL;
	}
	lns->val_handle = gatt_desc->handle;

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_GATT_CCC);
	if (!gatt_desc) {
		LOG_INF("No speed and location CCC descriptor found. Speed and location service does not supported notification.");
	} else {
		lns->notify = true;
		lns->ccc_handle = gatt_desc->handle;
	}

	/* Finally - save connection object */
	lns->conn = bt_gatt_dm_conn_get(dm);
	return 0;
}

int bt_lns_subscribe_location_and_speed(struct bt_lns_client *lns,
				   bt_lns_notify_location_and_speed_cb func)
{
	int err;

    LOG_INF("bt_lns_subscribe_location_and_speed");

	if (!lns || !func) {
		return -EINVAL;
	}
	if (!lns->conn) {
		return -EINVAL;
	}
	if (!(lns->properties & BT_GATT_CHRC_NOTIFY)) {
		return -ENOTSUP;
	}
	if (lns->notify_location_and_speed_cb) {
		return -EALREADY;
	}

	lns->notify_location_and_speed_cb = func;

	lns->notify_params.notify = notify_process;
	lns->notify_params.value = BT_GATT_CCC_NOTIFY;
	lns->notify_params.value_handle = lns->val_handle;
	lns->notify_params.ccc_handle = lns->ccc_handle;
	atomic_set_bit(lns->notify_params.flags,
		       BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	LOG_DBG("Subscribe: val: %u, ccc: %u",
		lns->notify_params.value_handle,
		lns->notify_params.ccc_handle);
	err = bt_gatt_subscribe(lns->conn, &lns->notify_params);
	if (err) {
		LOG_ERR("Report notification subscribe error: %d.", err);
		lns->notify_location_and_speed_cb = NULL;
		return err;
	}
	LOG_DBG("Report subscribed.");
	return err;
}


int bt_lns_unsubscribe_location_and_speed(struct bt_lns_client *lns)
{
	int err;

    LOG_INF("bt_lns_unsubscribe_location_and_speed");

	if (!lns) {
		return -EINVAL;
	}

	if (!lns->notify_location_and_speed_cb) {
		return -EFAULT;
	}

	err = bt_gatt_unsubscribe(lns->conn, &lns->notify_params);
	lns->notify_location_and_speed_cb = NULL;
	return err;
}


struct bt_conn *bt_lns_conn(const struct bt_lns_client *lns)
{
	return lns->conn;
}


int bt_lns_read_location_and_speed(struct bt_lns_client *lns, bt_lns_read_cb func)
{
	int err;

    LOG_INF("bt_lns_read_location_and_speed");

	if (!lns || !func) {
		return -EINVAL;
	}
	if (!lns->conn) {
		return -EINVAL;
	}
	if (lns->read_cb) {
		return -EBUSY;
	}
	lns->read_cb = func;
	lns->read_params.func = read_process;
	lns->read_params.handle_count  = 1;
	lns->read_params.single.handle = lns->val_handle;
	lns->read_params.single.offset = 0;

	err = bt_gatt_read(lns->conn, &lns->read_params);
	if (err) {
		lns->read_cb = NULL;
		return err;
	}
	return 0;
}


struct ble_lns_loc_speed_s *bt_lns_get_last_location_and_speed(struct bt_lns_client *lns)
{
	if (!lns) {
		return NULL;
	}

	return &lns->lns_data;
}


int bt_lns_start_per_read_location_and_speed(struct bt_lns_client *lns,
					int32_t interval,
					bt_lns_notify_location_and_speed_cb func)
{
    LOG_INF("bt_lns_start_per_read_location_and_speed");

	if (!lns || !func || !interval) {
		return -EINVAL;
	}

	if (bt_lns_notify_supported(lns)) {
		return -ENOTSUP;
	}

	lns->notify_location_and_speed_cb = func;
	atomic_set(&lns->periodic_read.interval, interval);
	k_work_schedule(&lns->periodic_read.read_work, K_MSEC(interval));

	return 0;
}


void bt_lns_stop_per_read_location_and_speed(struct bt_lns_client *lns)
{
    LOG_INF("bt_lns_stop_per_read_location_and_speed");

	/* If read is proccesed now, prevent triggering new
	 * characteristic read.
	 */
	atomic_set(&lns->periodic_read.interval, 0);

	/* If delayed workqueue pending, cancel it. If this fails, we'll exit
	 * early in the read handler due to the interval.
	 */
	k_work_cancel_delayable(&lns->periodic_read.read_work);
}
