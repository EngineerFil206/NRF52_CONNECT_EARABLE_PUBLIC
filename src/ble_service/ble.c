#include "ble.h"
#include <string.h>
#include <zephyr/settings/settings.h>

#define LOG_MODULE_NAME ble
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static K_SEM_DEFINE(bt_init_ok, 0, 1);
static uint16_t ble_values[50];
static bool notify_enabled = false;

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_BLE_SERV_VAL),
};

static ssize_t read_ble_characteristic_cb(struct bt_conn *conn,
										  const struct bt_gatt_attr *attr,
										  void *buf,
										  uint16_t len,
										  uint16_t offset);
static void ble_chrc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static void on_sent(struct bt_conn *conn, void *user_data);

BT_GATT_SERVICE_DEFINE(ble_srv,
					   BT_GATT_PRIMARY_SERVICE(BT_UUID_BLE_SERVICE),
					   BT_GATT_CHARACTERISTIC(BT_UUID_BLE_DATA_CHRC,
											  BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
											  BT_GATT_PERM_READ_AUTHEN,
											  read_ble_characteristic_cb,
											  NULL,
											  NULL),
					   BT_GATT_CCC(ble_chrc_ccc_cfg_changed,
								   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static void bt_ready(int err)
{
	if (err)
	{
		LOG_ERR("bt_ready returned %d", err);
	}

	k_sem_give(&bt_init_ok);
}

static ssize_t read_ble_characteristic_cb(struct bt_conn *conn,
										  const struct bt_gatt_attr *attr,
										  void *buf,
										  uint16_t len,
										  uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
                         ble_values,
                         sizeof(ble_values));
}

static void ble_chrc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);

    notify_enabled = (value == BT_GATT_CCC_NOTIFY);

    LOG_INF("Notifications %s",
            notify_enabled ? "enabled" : "disabled");
}

static uint32_t sent_count;

static void on_sent(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);

	sent_count++;
}

#define BT_LE_ADV_CONN_ACCEPT_LIST                                                                 \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN |                    \
				BT_LE_ADV_OPT_ONE_TIME,                                            \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define BT_LE_ADV_CONN_NO_ACCEPT_LIST \
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME, \
            BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define BT_LE_ADV_CONN_NO_ACCEPT_LIST \
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME, \
            BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

static void setup_accept_list_cb(const struct bt_bond_info *info, void *user_data)
{
	int *bond_cnt = user_data;

	if ((*bond_cnt) < 0) {
		return;
	}

	int err = bt_le_filter_accept_list_add(&info->addr);
	LOG_INF("Added following peer to accept list: %x %x\n", info->addr.a.val[0],
		info->addr.a.val[1]);
	if (err) {
		LOG_INF("Cannot add peer to filter accept list (err: %d)\n", err);
		(*bond_cnt) = -EIO;
	} else {
		(*bond_cnt)++;
	}
}

static int setup_accept_list(uint8_t local_id)
{
	int err = bt_le_filter_accept_list_clear();

	if (err) {
		LOG_INF("Cannot clear accept list (err: %d)\n", err);
		return err;
	}

	int bond_cnt = 0;

	bt_foreach_bond(local_id, setup_accept_list_cb, &bond_cnt);

	return bond_cnt;
}

void advertise_with_acceptlist(struct k_work *work)
{
	int err = 0;
	int allowed_cnt = setup_accept_list(BT_ID_DEFAULT);
	if (allowed_cnt < 0) {
		LOG_INF("Acceptlist setup failed (err:%d)\n", allowed_cnt);
	} else {
		if (allowed_cnt == 0) {
			LOG_INF("Advertising with no Accept list \n");
			err = bt_le_adv_start(BT_LE_ADV_CONN_NO_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		} else {
			LOG_INF("Acceptlist setup number  = %d \n", allowed_cnt);
			err = bt_le_adv_start(BT_LE_ADV_CONN_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		}
		if (err) {
			LOG_INF("Advertising failed to start (err %d)\n", err);
			return;
		}
		LOG_INF("Advertising successfully started\n");
	}
}

K_WORK_DEFINE(advertise_acceptlist_work, advertise_with_acceptlist);

void ble_restart_advertising(void)
{
    k_work_submit(&advertise_acceptlist_work);
}

int send_ble_notification(struct bt_conn *conn,
						  const uint16_t *values,
						  size_t count)
{
	if (!notify_enabled)
	{
		return 0;
	}

	int err;
	struct bt_gatt_notify_params params = {0};
	const struct bt_gatt_attr *attr = &ble_srv.attrs[2];

	params.attr = attr;

	memcpy(ble_values, values, count * sizeof(uint16_t));
	
	params.data = ble_values;
	params.len = count * sizeof(uint16_t);
	params.func = on_sent;

	err = bt_gatt_notify_cb(conn, &params);

	return err;
}

int ble_init(struct bt_conn_cb *bt_cb)
{
	int err;

	LOG_INF("Initializing Bluetooth");

	if (bt_cb == NULL)
	{
		return NRFX_ERROR_NULL;
	}

	bt_conn_cb_register(bt_cb);

	err = bt_enable(bt_ready);
	if (err)
	{
		LOG_ERR("bt_enable returned %d", err);
		return err;
	}

	k_sem_take(&bt_init_ok, K_FOREVER);
	
	settings_load();

	// ### Uncomment to delete all bonds on startup
	// LOG_INF("Deleting all bonds...");
	// err = bt_unpair(BT_ID_DEFAULT, NULL);
	// LOG_INF("bt_unpair() = %d", err);
	// ### ----------------------------------------

	k_work_submit(&advertise_acceptlist_work);

	return err;
}
