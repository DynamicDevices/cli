#ifndef APP_BLUETOOTH_H_
#define APP_BLUETOOTH_H_

// Includes
#include "bluetooth/lns_client.h"

// Defines

#define LNS_READ_VALUE_INTERVAL 10000

// Prototypes

int appbluetoothInit(void);
struct bt_lns_client *getLNSClient(void);

#endif
