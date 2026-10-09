/*
Fun experiments to do:

 - On line 181, try:

   while (k_work_busy_get()) {
      // Do nothing untill NOT busy anymore
   }

   // Then send BLE
   
   Hypothesis: Code will error out and complain something about memory)

*/

// ________________________________________________________________
// main.c
// ________________________________________________________________


// Import Zephyr and other nRF Connect SDK v2.5.1 libraries

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble.h"

#include <nrfx_saadc.h>
#include <nrfx_timer.h>
#include <helpers/nrfx_gppi.h>
#if defined(DPPI_PRESENT)
#include <nrfx_dppi.h>
#else
#include <nrfx_ppi.h>
#endif

LOG_MODULE_REGISTER(MY_ADC_BLE, LOG_LEVEL_DBG);

static struct bt_conn *conn;

volatile uint16_t packet[50] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

volatile uint8_t packet_index = 0;

// Sample window size for envelope detecting moving average filter
#define ENV_WINDOW 40

static int32_t env_buffer[ENV_WINDOW] = {0};
static uint32_t env_index = 0;
static int32_t env_sum = 0;

static void adc_work_handler(struct k_work *work)
{
    if (conn)
    {
        int err = send_ble_notification(conn, packet, 50);

        if (err)
        {
            LOG_WRN("Notify failed: %d", err);
        }
    }
}

K_WORK_DEFINE(adc_work, adc_work_handler);

// ---------------------------------------------------------------------------------------------------------------------------
// Calculating BLE transmission sample rate
// ---------------------------------------------------------------------------------------------------------------------------

// ADC buffer properties
#define SAADC_SAMPLE_INTERVAL_US 5
#define SAADC_BUFFER_SIZE 20 // Had to be changed to 70 after sampling bug discovered (discussed in section 5.5 in final report)

// 5 microseconds per sample (is period) and 200 values required to trigger event.
// f=1/T, therefore 1/(5*10-6) = 200,000 hz
// but thats for ADC sample averages
// the actuall number of samples per second that BLE will transmit is 200,000 hz / 200 buffer = 1000 averaged samples per second

// BLE_SAMP_FREQUENCY = (1 / (SAADC_SAMPLE_INTERVAL_US / 1000000)) / (SAADC_BUFFER_SIZE)
// print((1 / (5 / 1000000)) / (200))

// ---------------------------------------------------------------------------------------------------------------------------

const nrfx_timer_t timer_instance = NRFX_TIMER_INSTANCE(2);

static int16_t saadc_sample_buffer[2][SAADC_BUFFER_SIZE];
static uint32_t saadc_current_buffer;

// SAADC Events and callback functions:

static void configure_timer(void)
{
    nrfx_err_t err;

    nrfx_timer_config_t timer_config =
        NRFX_TIMER_DEFAULT_CONFIG(1000000);

    err = nrfx_timer_init(&timer_instance, &timer_config, NULL);
    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Timer init failed: 0x%08X", err);
        return;
    }

    uint32_t ticks =
        nrfx_timer_us_to_ticks(&timer_instance,
                               SAADC_SAMPLE_INTERVAL_US);

    LOG_INF("Timer ticks = %u", ticks);

    nrfx_timer_extended_compare(
        &timer_instance,
        NRF_TIMER_CC_CHANNEL0,
        ticks,
        NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
        false);
}

static uint32_t adc_counter = 0;
static uint32_t time = 0;


static void saadc_event_handler(nrfx_saadc_evt_t const *p_event)
{
    nrfx_err_t err;

    switch (p_event->type)
    {

    case NRFX_SAADC_EVT_READY:
        LOG_INF("SAADC READY");
        nrfx_timer_enable(&timer_instance);
        break;

    case NRFX_SAADC_EVT_BUF_REQ:

        err = nrfx_saadc_buffer_set(
            saadc_sample_buffer[(saadc_current_buffer++) % 2],
            SAADC_BUFFER_SIZE);

        if (err != NRFX_SUCCESS)
        {
            LOG_ERR("Buffer set failed: 0x%08X", err);
        }

        break;

    case NRFX_SAADC_EVT_DONE:
    {
        adc_counter++;

        if (adc_counter >= 5000)
        {
            adc_counter = 0;
            printk(k_cycle_get_32());
        }

        break;
    }

    default:
        LOG_INF("SAADC event %d", p_event->type);
        break;
    }
}

static void configure_saadc(void)
{
    nrfx_err_t err;

    IRQ_CONNECT(DT_IRQN(DT_NODELABEL(adc)),
                DT_IRQ(DT_NODELABEL(adc), priority),
                nrfx_isr,
                nrfx_saadc_irq_handler,
                0);

    err = nrfx_saadc_init(
        DT_IRQ(DT_NODELABEL(adc), priority));

    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("SAADC init failed: 0x%08X", err);
        return;
    }

    nrfx_saadc_channel_t channel =
        NRFX_SAADC_DEFAULT_CHANNEL_SE(
            NRF_SAADC_INPUT_AIN0,
            0);

    channel.channel_config.gain = NRF_SAADC_GAIN1_6;

    err = nrfx_saadc_channels_config(&channel, 1);
    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Channel config failed: 0x%08X", err);
        return;
    }

    nrfx_saadc_adv_config_t cfg =
        NRFX_SAADC_DEFAULT_ADV_CONFIG;

    err = nrfx_saadc_advanced_mode_set(
        BIT(0),
        NRF_SAADC_RESOLUTION_12BIT,
        &cfg,
        saadc_event_handler);

    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Advanced mode failed: 0x%08X", err);
        return;
    }

    err = nrfx_saadc_buffer_set(
        saadc_sample_buffer[0],
        SAADC_BUFFER_SIZE);

    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Buffer0 failed: 0x%08X", err);
        return;
    }

    err = nrfx_saadc_buffer_set(
        saadc_sample_buffer[1],
        SAADC_BUFFER_SIZE);

    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Buffer1 failed: 0x%08X", err);
        return;
    }

    err = nrfx_saadc_mode_trigger();

    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Mode trigger failed: 0x%08X", err);
    }
}

static void configure_ppi(void)
{
    nrfx_err_t err;

    uint8_t sample_ch;
    uint8_t start_ch;

    err = nrfx_gppi_channel_alloc(&sample_ch);
    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Sample channel alloc failed");
        return;
    }

    err = nrfx_gppi_channel_alloc(&start_ch);
    if (err != NRFX_SUCCESS)
    {
        LOG_ERR("Start channel alloc failed");
        return;
    }

    nrfx_gppi_channel_endpoints_setup(
        sample_ch,
        nrfx_timer_compare_event_address_get(
            &timer_instance,
            NRF_TIMER_CC_CHANNEL0),
        nrf_saadc_task_address_get(
            NRF_SAADC,
            NRF_SAADC_TASK_SAMPLE));

    nrfx_gppi_channel_endpoints_setup(
        start_ch,
        nrf_saadc_event_address_get(
            NRF_SAADC,
            NRF_SAADC_EVENT_END),
        nrf_saadc_task_address_get(
            NRF_SAADC,
            NRF_SAADC_TASK_START));

    nrfx_gppi_channels_enable(BIT(sample_ch));
    nrfx_gppi_channels_enable(BIT(start_ch));

    LOG_INF("PPI configured");
}


// BLE connection events

static void connected(struct bt_conn *c, uint8_t err)
{
    if (err)
    {
        LOG_ERR("Connection failed (%u)", err);
        return;
    }

    LOG_INF("Connected");
    conn = bt_conn_ref(c);

    configure_timer();
    configure_saadc();
    configure_ppi();
}

static void disconnected(struct bt_conn *c, uint8_t reason)
{
    ARG_UNUSED(c);

    LOG_INF("Disconnected (%u)", reason);

    if (conn)
    {
        bt_conn_unref(conn);
        conn = NULL;
        ble_restart_advertising();
    }
    
    nrfx_timer_disable(&timer_instance);

    nrfx_saadc_abort();
    nrfx_saadc_uninit();
}

// Encryption functions

static void on_security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err)
    {
        LOG_INF("Security changed: %s level %u\n", addr, level);
    }
    else
    {
        LOG_INF("Security failed: %s level %u err %d\n", addr, level, err);
    }
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = on_security_changed,
};

static struct bt_conn_auth_cb conn_auth_callbacks = {
    .passkey_display = auth_passkey_display,
    .cancel = auth_cancel,
};

static void print_bond_cb(const struct bt_bond_info *info, void *user_data)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(&info->addr, addr, sizeof(addr));

    LOG_INF("Bonded: %s", addr);
}

// Run main program that initiates BLE stack

int main(void)
{
    int err = ble_init(&conn_callbacks);

    if (err)
    {
        LOG_ERR("Bluetooth init failed (%d)", err);
        return 0;
    }

    err = bt_conn_auth_cb_register(&conn_auth_callbacks);

    if (err)
    {
        LOG_INF("Failed to register authorization callbacks.\n");
        return -1;
    }

    k_sleep(K_FOREVER);
}
