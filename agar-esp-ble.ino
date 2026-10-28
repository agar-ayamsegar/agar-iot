#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>

#define SERVICE_UUID "73332f45-02d1-46ff-a88d-edba7bfc0dc2"
#define CHARACTERISTIC_UUID "3112abef-cfd7-4bb2-b1fc-ab83c5e3f3d6"
#define BLE_PASSKEY 0


BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

bool connected = false;


// BLECharacteristicCallbacks -> will get callback when client write to esp ( and esp will get value )
// BLEServerCallbacks -> will get callback when something connect or disconnect

class ServerCallback: public BLEServerCallbacks {
  void onConnect(BLEServer *s) override {
    connected = true;
  }

  void onDisconnect(BLEServer *s) override {
    connected = false;
    s->startAdvertising();
  }
};

static bool peerAuthenticated() {
  if (pServer == NULL || pServer->getConnectedCount() < 1) return false; // server empty? return false

  ble_gap_conn_desc desc;
  if (ble_gap_conn_find(pServer->getConnId(), &desc) != 0) return false; // 0 mean success, not 0 means failed, need return false

  return desc.sec_state.encrypted && desc.sec_state.authenticated;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Hello world");

  if (BLE_PASSKEY == 0) {
    Serial.println("ERROR: Please provide BLE_PASSKEY");
    return;
  }

  BLEDevice::init("BeeSP-Bluetooth");
  BLESecurity::setPassKey(true, BLE_PASSKEY);
  BLESecurity::setAuthenticationMode(true, true, true); // secure connections
  BLESecurity::setCapability(ESP_IO_CAP_OUT); // board display, client type it


  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setAccessPermissions(
    ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM
  );

  pServer->setCallbacks(new ServerCallback());
  pCharacteristic->setValue("Hello world");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); 
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! We can send to the client on the loop()");
}

void loop() {
  if (connected && peerAuthenticated()) {
    pCharacteristic->setValue("hello " + String(millis() / 1000));
    pCharacteristic->notify();
  }


  delay(1000);
}