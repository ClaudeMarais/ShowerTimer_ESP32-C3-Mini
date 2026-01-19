// An Arduino project that automatically detects if a shower is running and shows a timer for how long you’ve been showering.
// TinyML using microphone data is used to determine if the shower is running or not. This is a great machine learning application,
// since there are many different “white noises” in a bathroom, so it’s challenging to distinguish between them algorithmically.
// E.g. shower, hair dryer, filling a bathtub, toilet flush, extractor fan, electric toothbrush and shaver, vacuum cleaner, etc.
//
// NOTE: There’s a possibility that the device might not be accurate in your own bathroom, since your shower might sound different
//       to mine, have different water pressure, different acoustics, different white noise, etc. In that case, you’ll need to build
//       the device, close it up, record audio data and add the data to a cloned version of my Edge Impulse project and train the model
//       again. Use the CAPTURE_DATA define in the Arduino file \ShowerTimer\ShowerTimer.ino, together with the Android project
//       \Android\CollectDataOnPhone
//
// NOTE: This is considered version 1, which doesn't have fantastic battery life, since I didn't consider that even though the
//       device goes into deepsleep, the microphone will still draw power at 1.4mA. See the Readme file for detailed description
//       on power consumption. Essentially v1 is expected to last only about 1.2 months using 3x AAA batteries.
//
//       A version 1.1 will be introduced in the future which simply adds a simple MOSFET that can completely shutdown power to the
//       microphone, increasing battery life to about 7.6 months
//
//       A version 2 could be introduced in the future which uses an ESP32-S3 that has a LP (low power) Core which can can be used
//       to switch on the device only at a certain loudness level, instead of a fixed 10 second interval. This could increase battery
//       to about 15 months
//
// Hardware:
// - ESP32-C3 Super Mini
// - INMP441 i2s microphone
// - Waveshare 1.54inch E-Ink Display Module, black & white, 200x200 resolution, SPI interface
// - Battery holder for 3X 1.5V AAA batteries

//#define DEBUG 1           // Only define this during development, since it will output useful debug text to Serial
//#define ENABLE_BLE 1      // Only define this if you need to send debug/fine tuning information to your phone using BLE
//#define CAPTURE_DATA 1    // Only define this if you're capturing audio data for ML training that will be sent to your phone using BLE

// Data capturing requires BLE
#ifdef CAPTURE_DATA
#define ENABLE_BLE 1
#endif

// Pin connections to INMP441 I2S microphone
#define I2S_WS       A0
#define I2S_SCK      A1
#define I2S_SD       A2

// Pin connections to SPI e-paper display
#define DISPLAY_CS   7
#define DISPLAY_DC   10
#define DISPLAY_RST  20
#define DISPLAY_BUSY 21

// Below this threshold is too quiet for the shower to be ON. This was determined experimentally.
const int32_t AvgAmplitudeThreshold = 100;

// 10 second time intervals to see if shower turned ON
const unsigned long TimeIntervalInSecondsWhileShowerOFF = 10;
const unsigned long TimeIntervalInMsWhileShowerOFF = TimeIntervalInSecondsWhileShowerOFF * 1000;

// 60 second time intervals to see if shower turned OFF. To save battery power, we only need to check once a minute to see if the shower turned OFF
const unsigned long TimeIntervalInSecondsWhileShowerON = 60;
const unsigned long TimeIntervalInMsWhileShowerON = TimeIntervalInSecondsWhileShowerON * 1000;

unsigned long StartTime = 0;

// RTC_DATA_ATTR keeps these variable alive during deep sleep
RTC_DATA_ATTR uint32_t g_ShowerOnTimeInSeconds = 0;
RTC_DATA_ATTR bool g_bIsShowerOn = false; // Global variable to keep track of the state of the shower over the full 10 sec time interval, even during deepsleep

#ifdef ENABLE_BLE
// The name of our Bluetooth peripheral
#define PERIPHERAL_NAME "ShowerTimerDevice"

// Custom generated UUIDs for this project (using https://www.uuidgenerator.net/)
#define SHOWER_TIMER_SERVICE_UUID "0372c18f-6bb1-4c2f-a8c0-8125d51c41a4"
#define DATA_FROM_PHONE_TO_ARDUINO_CHARACTERISTIC_UUID "157bd625-aa1f-474a-8e26-251cf2ca626e"
#define DATA_FROM_ARDUINO_TO_PHONE_CHARACTERISTIC_UUID "56de086a-9c2a-4765-8761-3b42f6398673"

#endif  // ENABLE_BLE

#include "Utils.h"
#include "TinyML.h"

static const uint32_t AudioSampleRate = EI_CLASSIFIER_FREQUENCY;
static const uint32_t NumSamplesInAudioBuffer = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
DMA_ATTR static int16_t AudioBuffer[NumSamplesInAudioBuffer];   // DMA_ATTR keeps this buffer in DMA-safe internal RAM for I2S

#include "Audio.h"
#include "Display.h"

#ifdef CAPTURE_DATA
#include "CaptureData.h"
#endif  // CAPTURE_DATA

#ifdef ENABLE_BLE
#include "BLEPeripheral.h"
char BLEText[32];
#endif  // ENABLE_BLE

#define ERROR_AUDIO_SETUP     1
#define ERROR_AUDIO_GET_DATA  2
#define ERROR_CAPTURE_DATA    3

void setup()
{
  // Everytime the device wakes up from deepsleep, Setup() will run, so we need to include any processing time from Setup() when calculating how long
  // to deepsleep to keep a strict 10 second time interval while the shower is OFF, and 60 seconds while the shower is ON
  StartTime = millis();

  // After the device powers on, immediatly initialize the microphone
  while (!Audio::Setup(AudioSampleRate))
  {
    ShowError(ERROR_AUDIO_SETUP);
    delay(1000);
  }

#ifdef CAPTURE_DATA
  while (!CaptureData::CaptureData())
  {
    ShowError(ERROR_CAPTURE_DATA);
    delay(1000);
  }
#endif  // CAPTURE_DATA

  while (!Audio::GetData(AudioBuffer, NumSamplesInAudioBuffer))
  {
    ShowError(ERROR_AUDIO_GET_DATA);
    delay(1000);
    return;
  }

#ifdef DEBUG
  Serial.begin(115200);
  delay(1000);
#endif

  DebugPrintf("\n\n********* ShowerTimer with ESP32-C3 Super Mini *********\n\n");

#ifdef ENABLE_BLE
  // Setup BLE to communicate with phone
  BLEPeripheral::Setup();

  // Wait for BLE to connect
  while (!BLEPeripheral::isCentralConnected)
  {
    delay(100);
  }
 
  // Although BLE is connected by now, it seems that we need to add an extra wait to make sure all the BLE callbacks are setup and ready to go
  delay(1000);
#endif  // ENABLE_BLE

  bool bIsShowerOnRightNow = false;
  float probability = 0.0f;

  int32_t avgAmplitude = 0;
  Audio::PreprocessData(AudioBuffer, NumSamplesInAudioBuffer, 5, avgAmplitude);

  // If the audio is too quiet, then the shower isn't ON, so no need to even run inference
  if (avgAmplitude > AvgAmplitudeThreshold)
  {
    DebugPrintf("Sound loud enough for shower to potentially be ON (Avg amplitude = %d)\n", avgAmplitude);

#ifdef ENABLE_BLE
    sprintf(BLEText, "Could be ON\n%d", avgAmplitude);
    SendBLEText(BLEText);
    delay(1000);
#endif  // ENABLE_BLE

    bIsShowerOnRightNow = TinyML::IsShowerOn(AudioBuffer, NumSamplesInAudioBuffer, probability);
  }
  else
  {
    DebugPrintf("Sound too quiet, therefore shower must be OFF (Avg amplitude = %d)\n", avgAmplitude);

#ifdef ENABLE_BLE
    sprintf(BLEText, "Too quiet\n%d", avgAmplitude);
    SendBLEText(BLEText);
    delay(1000);
#endif  // ENABLE_BLE

  }

  unsigned long timeIntervalInSeconds;
  unsigned long timeIntervalInMs;

  if (bIsShowerOnRightNow)  // Right now the shower is ON, but we need to determine if it just turned ON, or has it been ON already
  {
    timeIntervalInSeconds = TimeIntervalInSecondsWhileShowerON;
    timeIntervalInMs = TimeIntervalInMsWhileShowerON;

    auto oldShowerOnTimeInMinutes = g_ShowerOnTimeInSeconds / 60;

    if (!g_bIsShowerOn)
    {
      // Shower just turned ON
      DebugPrintln("Shower just turned ON");

#ifdef ENABLE_BLE
      sprintf(BLEText, "Turned ON\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE

      g_bIsShowerOn = true;
      g_ShowerOnTimeInSeconds = 0;

      // Update the display with 0 minutes of ON time
      Display::Init();
      Display::ShowNumber(0);
      Display::PowerOff();
    }
    else
    {
      // Shower still running
      DebugPrintln("Still running");

#ifdef ENABLE_BLE
      sprintf(BLEText, "Still running\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE

      g_ShowerOnTimeInSeconds += timeIntervalInSeconds;

      auto newShoweOnTimeInMinutes = g_ShowerOnTimeInSeconds / 60;

      if (newShoweOnTimeInMinutes != oldShowerOnTimeInMinutes)
      {
        // Update the display with the new number of minutes ON time
        Display::Init();
        Display::ShowNumber(newShoweOnTimeInMinutes);
        Display::PowerOff();
      }
    }
  }
  else  // Right now the shower is OFF, but we need to determine if it just turned OFF, or has it been OFF already
  {
    timeIntervalInSeconds = TimeIntervalInSecondsWhileShowerOFF;
    timeIntervalInMs = TimeIntervalInMsWhileShowerOFF;

    if (g_bIsShowerOn)
    {
      DebugPrintln("Shower just turned OFF");

      // NOTE: Here we could clear the display if we detect the shower turned OFF, but it's more interesting to keep showing the shower duration
      //       until the shower turns ON again. This way you can see for how long the previous person showered

#ifdef ENABLE_BLE
      sprintf(BLEText, "Turned OFF\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE
    }
    else
    {
      DebugPrintln("Shower is OFF");

#ifdef ENABLE_BLE
      sprintf(BLEText, "Shower is OFF\n%1.2f", probability);
      SendBLEText(BLEText);
#endif
    }
    
    g_bIsShowerOn = false;
  }

  // Go into deepsleep until the next time interval
  SleepUntilNextTimeInterval(timeIntervalInMs, StartTime);
}

void loop()
{
}

// The device does not have a realtime clock, so we need to as accurately as possible calculate for how long we need to
// deepsleep in order to keep our 10 second time interval, otherwise the shower ON duration will be calculated wrongly
void SleepUntilNextTimeInterval(unsigned long intervalInMs, unsigned long startTimeInMs)
{
  auto now = millis();
  auto elapsed = now - startTimeInMs;
  DebugPrintf("Total elapsed time = %lu ms\n", elapsed);

  // If processing took longer than the time interval, sleep minimally
  if (elapsed > intervalInMs)
  {
    // Sleep 1 ms to avoid immediate re-wake
    Audio::Shutdown();
    esp_sleep_enable_timer_wakeup(1000ULL);
    esp_deep_sleep_start();
  }

  unsigned long remainingTimeInMs = intervalInMs - elapsed;
  uint64_t remainingTimeInUs = (uint64_t)remainingTimeInMs * 1000ULL;

  DebugPrintf("Deepsleep for %lu ms\n", remainingTimeInMs);

  Audio::Shutdown();
  esp_sleep_enable_timer_wakeup(remainingTimeInUs);
  esp_deep_sleep_start();
}

// Show an error number on the display
void ShowError(const int32_t number)
{
  Display::Init();
  Display::ShowNumber(-number);
  Display::PowerOff();
}

#ifdef ENABLE_BLE
void SendBLEText(char* text)
{
  if (BLEPeripheral::isCentralConnected && text)
  {
    BLEPeripheral::SendData((unsigned char*)text, strlen(text));
  }
  delay(250);
}
#endif  // ENABLE_BLE
