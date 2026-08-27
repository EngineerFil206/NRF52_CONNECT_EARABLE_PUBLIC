#include <bluefruit.h>

const char* DEVICE_NAME = "NRF52_EMG";
const char* SERVICE_UUID = "e9ea0001-e19b-482d-9293-c7907585fc48";
const char* CHARACTERISTIC_UUID = "e9ea0002-e19b-482d-9293-c7907585fc48";

BLEService emgService(SERVICE_UUID);
BLECharacteristic emgCharacteristic(CHARACTERISTIC_UUID);

volatile bool isConnected = false;
uint32_t lastSend = 0;

uint16_t packet[10];
uint8_t packetIndex = 0;

void connect_callback(uint16_t conn_handle)
{
    isConnected = true;

    BLEConnection* conn = Bluefruit.Connection(conn_handle);

    if (conn)
    {
        conn->requestPHY(BLE_GAP_PHY_2MBPS);
        conn->requestConnectionParameter(6, 0, 400);
    }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
    isConnected = false;
    Bluefruit.Advertising.start(0);
}

void setup()
{
    analogReadResolution(12);
    pinMode(A0, INPUT);

    Bluefruit.autoConnLed(false);
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    Bluefruit.begin();
    Bluefruit.setTxPower(8);
    Bluefruit.setName(DEVICE_NAME);

    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

    emgService.begin();

    emgCharacteristic.setProperties(CHR_PROPS_NOTIFY);
    emgCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    emgCharacteristic.setMaxLen(sizeof(packet));
    emgCharacteristic.begin();

    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(emgService);

    Bluefruit.ScanResponse.addName();

    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 32);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

void loop()
{
    if (!isConnected)
    {
        delay(10);
        return;
    }

    uint32_t now = millis();

    if ((int32_t)(now - lastSend) >= 10)
    {
        lastSend += 10;

        packet[packetIndex++] = analogRead(A0);

        if (packetIndex >= 10)
        {
            packetIndex = 0;

            emgCharacteristic.notify((uint8_t *)packet, sizeof(packet));
        }
    }
}