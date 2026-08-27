#ifndef BLE_SERVICE_BLE_H_
#define BLE_SERVICE_BLE_H_

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

/** @brief UUID of the BLE Service. **/
#define BT_UUID_BLE_SERV_VAL \
	BT_UUID_128_ENCODE(0xe9ea0001, 0xe19b, 0x482d, 0x9293, 0xc7907585fc48)

/** @brief UUID of the BLE Data Characteristic. **/
#define BT_UUID_BLE_DATA_CHRC_VAL \
	BT_UUID_128_ENCODE(0xe9ea0002, 0xe19b, 0x482d, 0x9293, 0xc7907585fc48)

#define BT_UUID_BLE_SERVICE      BT_UUID_DECLARE_128(BT_UUID_BLE_SERV_VAL)
#define BT_UUID_BLE_DATA_CHRC    BT_UUID_DECLARE_128(BT_UUID_BLE_DATA_CHRC_VAL)

void ble_restart_advertising(void);

int send_ble_notification(struct bt_conn *conn,
                          const uint16_t *values,
                          size_t count);
						  
int ble_init(struct bt_conn_cb *bt_cb);

#endif /* BLE_SERVICE_BLE_H_ */
