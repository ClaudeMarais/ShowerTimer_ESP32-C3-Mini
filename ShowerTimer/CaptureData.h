// Functions that can be used to record audio data and send it to a phone using BLE. The recorded data is used for ML training data.
//
// One thing that's interesting for capturing data, is that we need to capture the data in exactly the same way that the actuall app
// would capture data, which is capturing the data right after the device wakes up from deepsleep. Deepsleeping the device is required
// to maximise battery life. But, since there is DC drift on the audio data for about 2 seconds right after the device reboots, we need
// to capture data by first deepsleeping, so that we can capture training data with the same DC drift. See the readme file for more details.
//
// The data capture flow is:
//  1) Boot device
//  2) Connect to phone using BLE
//  3) Wait for a recording request from the phone
//  4) Deepsleep for 1 second
//  5) Wake up
//  6) Immediately capture audio data
//  7) Connect to phone using BLE
//  8) Transfer audio data to phone

#ifndef _CAPTURE_DATA
#define _CAPTURE_DATA

#include "Audio.h"
#include "BLEPeripheral.h"

namespace CaptureData
{
  // Instead of streaming audio data to the phone, we just do short recordings into a big DMA copy buffer and then transfer the whole buffer to the phone.
  // You can easily record 3 seconds at 16KHz, or 6 seconds at 8KHz into a static buffer on the ESP32-C3. Our ML data window of audio data is only 250ms,
  // so 2 seconds capture more than enough useful data in one recording.
  const uint32_t TotalSecondsToRecord = 2;

  // Audio data size info
  const uint32_t BytesPerSecond = Audio::NumChannels * AudioSampleRate * Audio::BitsPerSample / 8;
  const uint32_t TotalAudioSizeInBytes = TotalSecondsToRecord * BytesPerSecond;
  const uint32_t TotalNumSamples = TotalSecondsToRecord * AudioSampleRate;
  const uint32_t DMACopyBufferSizeInBytes = BytesPerSecond * TotalSecondsToRecord;  // Size of the DMA copy buffer that can hold audio data for the duration of TotalSecondsToRecord
  const uint32_t NumSamplesInDMACopyBuffer = DMACopyBufferSizeInBytes / (Audio::BitsPerSample / 8);

  // Static audio buffer for a duration of TotalSecondsToRecord. We will only record a few seconds, so we can use one big DMA copy buffer.
  // If the duration is too long for one big DMA copy buffer, this app won't work, since sending BLE packets is very slow. If we need to
  // send longer audio, then the data can first be copied to an SD card or perhaps the data can be transferred over Wifi on a seperate thread
  DMA_ATTR static uint8_t DMACopyBuffer[DMACopyBufferSizeInBytes];  // I2S DMA buffers are copied into this buffer when calling i2s_read()

  // Header commands to start communication with phone
  uint8_t CommandStartRecording[] = { 'R', 'E', 'C', 'O', 'R', 'D' };           // Let phone know recording started and how for how many seconds, so that a progress bar can be shown
  uint8_t CommandStartTransfer[] = { 'T', 'R', 'A', 'N', 'S', 'F', 'E', 'R'};   // Let phone know that transfer of audio data will start and how many bytes to expect

  // BLE packets are only 20 bytes and has to be sent a minimum of 7.5ms apart
  const uint32_t MaxBLEBytesPerPacket = 20;
  const uint32_t DelayForPhoneUIToUpdate = 250;   // in milliseconds

  // This is set to true in a callback function when data is received from phone
  volatile bool bRequestRecording = false;

  // Set this true right before going into deepsleep so that we know that we woke up to start a recording in the Setup function
  RTC_DATA_ATTR bool bSendAudioToPhoneOnStartup = false;

  // After receiving a request to record audio, we'll first deepsleep, wakeup and then record audio
  const uint64_t DeepSleepDurationInSeconds = 1ULL;
  const uint64_t DeepSleepDurationInMicroSeconds = DeepSleepDurationInSeconds * 1000000ULL;

  // Callback that can process values as soon as they are received from the phone in the Notify callback
  // We actually don't care what the data is, the only time data will be received is when the phone asks
  // to start audio recording
  void NewDataReceivedFromPhoneCallback(uint8_t data)
  {
    bRequestRecording = true;
  }

  // Setup
  void SetupBLE()
  {
    DebugPrintln("CaptureData::Setup()");

    // Setup Bluetooth to communicate with phone
    BLEPeripheral::Setup();
    BLEPeripheral::SetOnValuesReceivedCallback(NewDataReceivedFromPhoneCallback);

    // Wait for BLE to connect
    while (!BLEPeripheral::isCentralConnected)
    {
      delay(100);
    }

    // Although BLE is connected by now, we need to add an extra wait to make sure all the BLE callbacks are setup and ready to go
    delay(1000);
  }

  // Record audio data and copy it into one big DMA copy buffer
  bool RecordAudioData()
  {
    size_t numBytesRead;
    uint32_t numBytesRecorded = 0;
   
    DebugPrintf("\nRecording %d seconds of audio...\n", TotalSecondsToRecord);

    while (numBytesRecorded < TotalAudioSizeInBytes)
    {
      // Read data from DMA buffers into our copy buffer
      if (i2s_read(I2S_NUM_0, (void*)DMACopyBuffer, DMACopyBufferSizeInBytes, &numBytesRead, portMAX_DELAY) == ESP_OK)
      {
        numBytesRecorded += numBytesRead;
        DebugPrintf("    %u%%\n", numBytesRecorded * 100 / TotalAudioSizeInBytes);
      }
      else
      {
        DebugPrintln("ERROR: reading I2S failed!");
        return false;
      }
    }

    DebugPrintln("Done\n");
    return true;
  }

  // Send a simple header package to the phone
  void SendHeaderToPhone(uint8_t* string, uint32_t stringLength, uint32_t value)
  {
    uint8_t command[stringLength + sizeof(value)];
    memcpy(command, string, stringLength);
    memcpy(&command[stringLength], (uint8_t*)&value, sizeof(value));
    BLEPeripheral::SendData(command, sizeof(command), DelayForPhoneUIToUpdate);
  }

  // Send the audio data in the one big DMA copy buffer to phone using BLE
  void SendAudioDataToPhone()
  {
    DebugPrintln("Send audio data to phone");

    if (BLEPeripheral::isCentralConnected)
    {
      // Let the phone know that the data transfer is starting and how many bytes to expect, e.g. TRANSFER 64000
      SendHeaderToPhone(CommandStartTransfer, sizeof(CommandStartTransfer), DMACopyBufferSizeInBytes);

      uint32_t numBytesSent = 0;
      uint32_t numByteLeftToSend = DMACopyBufferSizeInBytes - numBytesSent;
      uint32_t numBytesToSend = (numByteLeftToSend > MaxBLEBytesPerPacket) ? MaxBLEBytesPerPacket : numByteLeftToSend;

      while (numBytesSent < DMACopyBufferSizeInBytes)
      {
        numByteLeftToSend = DMACopyBufferSizeInBytes - numBytesSent;
        numBytesToSend = (numByteLeftToSend > MaxBLEBytesPerPacket) ? MaxBLEBytesPerPacket : numByteLeftToSend;

        BLEPeripheral::SendData(&DMACopyBuffer[numBytesSent], numBytesToSend);
        numBytesSent += numBytesToSend;

  #ifdef DEBUG
        // Show a simple progress indicator while data is being transferred
        if ((numBytesSent % (MaxBLEBytesPerPacket * 100) == 0))
        DebugPrintf(".");
  #endif
      }

      DebugPrintf("\n%d bytes sent to phone\n\n", numBytesSent);
    }
    else
    {
      DebugPrintln("Data not sent, phone not connected\n");
    }
  }

  // Wait untile there is a request from the phone
  void WaitForRequestFromPhone()
  {
    while (true)
    {
      if (bRequestRecording)
      {
        DebugPrintln("Received request from phone to record audio");

        bRequestRecording = false;
        bSendAudioToPhoneOnStartup = true;

        // Let the phone know recording will start after the device wakes up from deepsleep, and for how many seconds so that it can show a progress bar, e.g. to record 3 seconds, this will send "RECORD 3"
        // The phone's progress bar will show for the number of seconds to record, but should include the time the device deep sleeps
        DebugPrintln("Notify phone that audio recording will start soon");
        SendHeaderToPhone(CommandStartRecording, sizeof(CommandStartRecording), TotalSecondsToRecord + DeepSleepDurationInSeconds + 3); // add extra 3 seconds for BLE to reconnect

        Audio::Shutdown();

        // Show 'R' on the display during recording
        Display::Init();
        Display::ShowCharacter('R');
        Display::PowerOff();

        DebugPrintln("Deep sleep");
        esp_sleep_enable_timer_wakeup(DeepSleepDurationInMicroSeconds);
        esp_deep_sleep_start();
      }
    }
  }

  // Capture audio data
  bool CaptureData()
  {
    // Record audio as soon as possible
    if (!RecordAudioData())
    {
      return false;
    }

    // Now that data is captured, setup BLE and send the data to the phone
    SetupBLE();

    if (bSendAudioToPhoneOnStartup)
    {
      bSendAudioToPhoneOnStartup = false;

      int32_t avgAmplitude = 0;
      Audio::PreprocessData((int16_t*)DMACopyBuffer, TotalNumSamples, 5, avgAmplitude);
      DebugPrintf("Avg Amplitude of captured audio = %d\n", avgAmplitude);

      SendAudioDataToPhone();
    }

    Display::Init();
    Display::Clear();
    Display::PowerOff();

    WaitForRequestFromPhone();

    return true;
  }
}

#endif  // _CAPTURE_DATA