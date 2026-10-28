#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>


#define SERVICE_UUID "73332f45-02d1-46ff-a88d-edba7bfc0dc2"
#define CHARACTERISTIC_UUID "3112abef-cfd7-4bb2-b1fc-ab83c5e3f3d6"


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

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Hello world");

  BLEDevice::init("BeeSP-Bluetooth");
  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
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
  if (connected) {
    pCharacteristic->setValue("hello " + String(millis() / 1000));
    pCharacteristic->notify();
  }


  delay(1000);
}