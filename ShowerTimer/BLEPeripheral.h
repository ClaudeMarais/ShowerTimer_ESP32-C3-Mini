// Based on code snippets from here: https://randomnerdtutorials.com/esp32-ble-server-client/

#ifndef _BLE_PERIPHERAL
#define _BLE_PERIPHERAL

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

namespace BLEPeripheral
{
  // Custom callback for when connected to Central
  void (*OnConnectedCallback)();
  void SetOnConnectedCallback(void (*onConnectedFunction)()) { OnConnectedCallback = onConnectedFunction; }

  // Custom callback for when disconnected from Central
  void (*OnDisconnectedCallback)();
  void SetOnDisconnectedCallback(void (*onDisconnectedFunction)()) { OnDisconnectedCallback = onDisconnectedFunction; }

  // Custom callback for when values are received from Central
  void (*OnValuesReceivedCallback)(uint8_t);
  void SetOnValuesReceivedCallback(void (*onValuesReceivedFunction)(uint8_t)) { OnValuesReceivedCallback = onValuesReceivedFunction; }

  static bool isCentralConnected = false;

  // Central is allowed to read values and get notified when values are received
  BLECharacteristic dataFromArduinoToPhone(DATA_FROM_ARDUINO_TO_PHONE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLECharacteristic dataFromPhoneToArduino(DATA_FROM_PHONE_TO_ARDUINO_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  class MyServerCallbacks: public BLEServerCallbacks
  {
      void onConnect(BLEServer* pServer) override
      {
          DebugPrintln("Central connected");
          isCentralConnected = true;

          if (OnConnectedCallback)
          {
            OnConnectedCallback();
          }
      }

      void onDisconnect(BLEServer* pServer) override
      {
          DebugPrintln("Central disconnected");
          isCentralConnected = false;
          BLEDevice::startAdvertising();  // When disconnected, start advertising again
          DebugPrintln("Peripheral started advertising");

          if (OnDisconnectedCallback)
          {
            OnDisconnectedCallback();
          }
      }
  };

  class PhoneToArduinoCallbacks: public BLECharacteristicCallbacks
  {
      void onWrite(BLECharacteristic* pCharacteristic)
      {
        uint8_t* pData = pCharacteristic->getData();

        if (OnValuesReceivedCallback)
        {
          uint8_t value = pData[0];
          OnValuesReceivedCallback(value);
        }
      }
  };

  void Setup()
  {
    DebugPrintln("Initializing BLE Peripheral");

    // Create BLE device
    BLEDevice::init(PERIPHERAL_NAME);

    // Create the BLE Server
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Add a service to the server
    BLEService* pService = pServer->createService(SHOWER_TIMER_SERVICE_UUID);

    // Add a characteristic to the service
    pService->addCharacteristic(&dataFromArduinoToPhone);
    pService->addCharacteristic(&dataFromPhoneToArduino);

    // Add BLE2902 descriptor to the characteristic to use Notify property
    // Each characteristic needs its own BLE2902 instance
    BLE2902* pBLE2902_toPhone = new BLE2902();
    pBLE2902_toPhone->setNotifications(true);
    dataFromArduinoToPhone.addDescriptor(pBLE2902_toPhone);

    BLE2902* pBLE2902_fromPhone = new BLE2902();
    pBLE2902_fromPhone->setNotifications(true);
    dataFromPhoneToArduino.addDescriptor(pBLE2902_fromPhone);

    dataFromPhoneToArduino.setCallbacks(new PhoneToArduinoCallbacks());

    // Configure advertising
    pServer->getAdvertising()->addServiceUUID(SHOWER_TIMER_SERVICE_UUID);

    // Start the service
    pService->start();

    // Start advertising
    pServer->getAdvertising()->start();

    DebugPrintln("Peripheral started advertising");
    DebugPrintln("Waiting for Central to connect...");
  }
  
  // BLE requires at least 7.5ms delay between sending packets
  void SendData(uint8_t* pData, size_t sizeInBytes, uint32_t delayAfterSend = 10)
  {
    if (isCentralConnected)
    {
      // Set the new values and notify the central device
      dataFromArduinoToPhone.setValue(pData, sizeInBytes);
      dataFromArduinoToPhone.notify();
      delay(delayAfterSend);
    }
  }

  uint8_t ReadValue()
  {
    uint8_t value = 0;

    if (isCentralConnected)
    {
      uint8_t* data = dataFromPhoneToArduino.getData();
      if (data != nullptr)
      {
        value = data[0];
      }
    }
    return value;
  }

  void Shutdown()
  {
    // To properly shutdown BLE, we need to first disconnect if we're currently connected, otherwise the ESP32 will crash when we try to stop advertising while connected
    if (isCentralConnected)
    {
      BLEDevice::getServer()->disconnect(0);
      delay(100); // Add a small delay to ensure the disconnect is processed before shutting down BLE
    }

    // Stop advertising before deinitializing
    BLEDevice::stopAdvertising();
    
     // Deinitialize BLE and release all resources
    BLEDevice::deinit(true);

    DebugPrintln("BLE shutdown");
  }

} // namespace BLEPeripheral

#endif  // _BLE_PERIPHERAL