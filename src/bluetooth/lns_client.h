/*
 * Copyright (c) 2018 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef __LNS_C_H
#define __LNS_C_H

struct ble_lns_loc_speed_s
{
    bool                            instant_speed_present;                     /**< Instantaneous Speed present (0=not present, 1=present). */
    bool                            total_distance_present;                    /**< Total Distance present (0=not present, 1=present). */
    bool                            location_present;                          /**< Location present (0=not present, 1=present). */
    bool                            elevation_present;                         /**< Elevation present (0=not present, 1=present). */
    bool                            heading_present;                           /**< Heading present (0=not present, 1=present). */
    bool                            rolling_time_present;                      /**< Rolling Time present (0=not present, 1=present). */
    bool                            utc_time_time_present;                     /**< UTC Time present (0=not present, 1=present). */
//    ble_lns_pos_status_type_t       position_status;                           /**< Status of current position */
//    ble_lns_speed_distance_format_t data_format;                               /**< Format of data (either 2D or 3D). */
//    ble_lns_elevation_source_t      elevation_source;                          /**< Source of the elevation measurement. */
//    ble_lns_heading_source_t        heading_source;                            /**< Source of the heading measurement. */
    uint16_t                        instant_speed;                             /**< Instantaneous Speed (1/10 meter per sec). */
    uint32_t                        total_distance;                            /**< Total Distance (meters), size=24 bits. */
    int32_t                         latitude;                                  /**< Latitude (10e-7 degrees). */
    int32_t                         longitude;                                 /**< Longitude (10e-7 degrees). */
    int32_t                         elevation;                                 /**< Elevation (1/100 meters), size=24 bits. */
    uint16_t                        heading;                                   /**< Heading (1/100 degrees). */
    uint8_t                         rolling_time;                              /**< Rolling Time (seconds). */
//    ble_date_time_t                 utc_time;                                  /**< UTC Time. */
};

/**
 * @file
 * @defgroup bt_lns_client_api Location And Navigation Service Client API
 * @{
 * @brief API for the Bluetooth LE GATT Location And Navigation Service (LNS) Client.
 */

#include <zephyr/kernel.h>
#include <sys/types.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/gatt_dm.h>

/**
 * @brief Value that shows that the flags are invalid.
 *
 * This value is stored in the LNS Client flags object when the speed and location
 * information is unknown.
 *
 * The same value is used to mark the fact that a notification has been
 * aborted (see @ref bt_bas_notify_cb).
 */
#define BT_LNS_VAL_INVALID (0)

struct bt_lns_client;

/**
 * @brief Value notification callback.
 *
 * This function is called every time the server sends a notification
 * for a changed value.
 *
 * @param lns           LNS Client object.
 * @param lns_data      The notified data or NULL
 *                      if the notification
 *                      was interrupted by the server
 *                      (NULL received from the stack).
 */
typedef void (*bt_lns_notify_location_and_speed_cb)(struct bt_lns_client *lns,
				 struct ble_lns_loc_speed_s *lns_data);

/**
 * @brief Read complete callback.
 *
 * This function is called when the read operation finishes.
 *
 * @param lns           LNS Client object.
 * @param lns_data      The data that was read
 * @param err           ATT error code or 0.
 */
typedef void (*bt_lns_read_cb)(struct bt_lns_client *lns,
			       struct ble_lns_loc_speed_s *lns_data,
			       int err);

/* @brief LNS Client characteristic periodic read. */
struct bt_lns_periodic_read {
	/** Work queue used to measure the read interval. */
	struct k_work_delayable read_work;
	/** Read parameters. */
	struct bt_gatt_read_params params;
	/** Read value interval. */
	atomic_t interval;
};

/** @brief LNS Client instance. */
struct bt_lns_client {
	/** Connection handle. */
	struct bt_conn *conn;
	/** Notification parameters. */
	struct bt_gatt_subscribe_params notify_params;
	/** Read parameters. */
	struct bt_gatt_read_params read_params;
	/** Read characteristic value timing. Used when characteristic do not
	 *  have a CCCD descriptor.
	 */
	struct bt_lns_periodic_read periodic_read;
	/** Notification callback. */
	bt_lns_notify_location_and_speed_cb notify_location_and_speed_cb;
	/** Read value callback. */
	bt_lns_read_cb read_cb;
	/** Handle of the Location and Speed Characteristic. */
	uint16_t val_handle;
	/** Handle of the CCCD of the Location and Speed Characteristic. */
	uint16_t ccc_handle;
	/** Current data */
	struct ble_lns_loc_speed_s lns_data;
	/** Properties of the service. */
	uint8_t properties;
	/** Notification supported. */
	bool notify;
};

/**
 * @brief Initialize the LNS Client instance.
 *
 * You must call this function on the LNS Client object before
 * any other function.
 *
 * @param bas  LNS Client object.
 */
void bt_lns_client_init(struct bt_lns_client *bas);

/**
 * @brief Assign handles to the LNS Client instance.
 *
 * This function should be called when a connection with a peer has been
 * established, to associate the connection to this instance of the module.
 * This makes it possible to handle multiple connections and associate each
 * connection to a particular instance of this module.
 * The GATT attribute handles are provided by the GATT Discovery Manager.
 *
 * @param dm    Discovery object.
 * @param lns LNS Client object.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 * @retval (-ENOTSUP) Special error code used when the UUID
 *         of the service does not match the expected UUID.
 */
int bt_lns_handles_assign(struct bt_gatt_dm *dm,
			  struct bt_lns_client *lns);

/**
 * @brief Subscribe to the Location And Speed change notification.
 *
 * @param lns LNS Client object.
 * @param func  Callback function handler.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 * @retval -ENOTSUP Special error code used if the connected server
 *         does not support notifications.
 */
int bt_lns_subscribe_location_and_speed(struct bt_lns_client *lns,
				   bt_lns_notify_location_and_speed_cb func);

/**
 * @brief Remove the subscription.
 *
 * @param lns LNS Client object.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_lns_unsubscribe_location_and_speed(struct bt_lns_client *lns);

/**
 * @brief Get the connection object that is used with a given LNS Client.
 *
 * @param lns LNS Client object.
 *
 * @return Connection object.
 */
struct bt_conn *bt_lns_conn(const struct bt_lns_client *lns);


/**
 * @brief Read the location and speed value from the device.
 *
 * This function sends a read request to the connected device.
 *
 * @param lns LNS Client object.
 * @param func  The callback function.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_lns_read_location_and_speed(struct bt_lns_client *lns, bt_lns_read_cb func);

/**
 * @brief Get the last known location and speed.
 *
 * This function returns the last known location and seped value.
 * The location and speed is stored when a notification or read response is
 * received.
 *
 * @param lns LNS Client object.
 *
 * @return Location and speed or negative error code.
 */
struct ble_lns_loc_speed_s *bt_lns_get_last_location_and_speed(struct bt_lns_client *lns);

/**
 * @brief Check whether notification is supported by the service.
 *
 * @param lns LNS Client object.
 *
 * @retval true If notifications are supported.
 *              Otherwise, @c false is returned.
 */
static inline bool bt_lns_notify_supported(struct bt_lns_client *lns)
{
	return lns->notify;
}

/**
 * @brief Periodically read the locatio and speed value from the device with
 *        specific time interval.
 *
 * @param lns LNS Client object.
 * @param interval Characteristic Read interval in milliseconds.
 * @param func The callback function.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_lns_start_per_read_location_and_speed(struct bt_lns_client *lns,
					int32_t interval,
					bt_lns_notify_location_and_speed_cb func);

/**
 * @brief Stop periodic reading of the battery value from the device.
 *
 * @param lns LNS Client object.
 */
void bt_lns_stop_per_read_location_and_speed(struct bt_lns_client *lns);

/**
 * @}
 */


#endif /* __LNS_C_H  */
