// An Arduino project that automatically detects if a shower is running and shows a timer for how long you’ve been showering.
// TinyML using microphone data is used to determine if the shower is running or not. This is a great machine learning application,
// since there are many different “white noises” in a bathroom, so it’s challenging to distinguish between them algorithmically.
// E.g. shower, hair dryer, filling a bathtub, toilet flush, extractor fan, electric toothbrush and shaver, vacuum cleaner, etc.
//
// NOTE: There’s a possibility that the device might not be accurate in your own bathroom, since your shower might sound different
//       to mine, have different water pressure, different acoustics, different white noise, etc. In that case, refer to the section
//       "Building your own" in the README.md document.
//
// NOTE: This is considered version 1, which doesn't have fantastic battery life, since I didn't consider that even though the
//       device goes into deepsleep, the microphone will still draw power at 1.4mA. Refer to the section "Battery life" in the
//       README.md file, which also includes ideas for future improvements.
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

// 12 second time intervals to see if shower turned ON, i.e. check 5 times per minute
const unsigned long TimeIntervalInSecondsWhileShowerOFF = 12;
const unsigned long TimeIntervalInMsWhileShowerOFF = TimeIntervalInSecondsWhileShowerOFF * 1000;

// 60 second time intervals to see if shower turned OFF. To save battery power, we only need to check once a minute to see if the shower turned OFF
#ifdef DEBUG
const unsigned long TimeIntervalInSecondsWhileShowerON = 10;
#else
const unsigned long TimeIntervalInSecondsWhileShowerON = 60;
#endif
const unsigned long TimeIntervalInMsWhileShowerON = TimeIntervalInSecondsWhileShowerON * 1000;

unsigned long StartTime = 0;

// RTC_DATA_ATTR keeps these variable alive during deep sleep
RTC_DATA_ATTR uint32_t g_ShowerOnTimeInSeconds = 0;
RTC_DATA_ATTR bool g_bIsShowerOn = false;               // Global variable to keep track of the state of the shower over the full 12 sec time interval, even during deepsleep
RTC_DATA_ATTR bool g_bNeedDoubleCheck = false;          // Flag to indicate if we need to double-check that shower is actually off in case of a false negative

// Termonlogy: A false negative is when we detect the shower turned OFF, but it is still actually ON
//             A false positive is when we detect the shower turned ON, but it is actually still OFF

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

// Perform audio preprocessing and ML inference to detect shower state
// Returns true if shower is detected as ON, false if OFF
// Also sets avgAmplitude and probability output parameters
bool DetectShowerState(int32_t& avgAmplitude, float& probability, const char* debugPrefix = "")
{
  bool bIsShowerOnRightNow = false;
  probability = 0.0f;
  
  avgAmplitude = 0;
  Audio::PreprocessData(AudioBuffer, NumSamplesInAudioBuffer, 5, avgAmplitude);
  
  // If the audio is loud enough, run inference
  if (avgAmplitude > AvgAmplitudeThreshold)
  {
    DebugPrintf("%sSound loud enough for shower to potentially be ON (Avg amplitude = %d)\n", debugPrefix, avgAmplitude);
    
#ifdef ENABLE_BLE
    snprintf(BLEText, sizeof(BLEText), "Maybe ON\n%d", avgAmplitude);
    SendBLEText(BLEText);
    delay(1000);
#endif  // ENABLE_BLE
    
    bIsShowerOnRightNow = TinyML::IsShowerOn(AudioBuffer, NumSamplesInAudioBuffer, probability);
  }
  else
  {
    DebugPrintf("%sSound too quiet, therefore shower must be OFF (Avg amplitude = %d)\n", debugPrefix, avgAmplitude);
    
#ifdef ENABLE_BLE
    snprintf(BLEText, sizeof(BLEText), "Too quiet\n%d", avgAmplitude);
    SendBLEText(BLEText);
    delay(1000);
#endif  // ENABLE_BLE
  }
  
  return bIsShowerOnRightNow;
}

void setup()
{
  // Everytime the device wakes up from deepsleep, Setup() will run, so we need to include any processing time from Setup() when calculating how long
  // to deepsleep to keep a strict 12 second time interval while the shower is OFF, and 60 seconds while the shower is ON
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

  // Double-check from previous wake cycle just in case there was a false negative
  if (g_bNeedDoubleCheck)
  {
    DebugPrintln("Performing double-check after potential false negative shower OFF detection");
    g_bNeedDoubleCheck = false; // Clear the flag
    
    // Perform the detection using our common function
    bool bIsShowerOnRightNow = false;
    float probability = 0.0f;
    int32_t avgAmplitude = 0;
    
    bIsShowerOnRightNow = DetectShowerState(avgAmplitude, probability, "Double-check: ");
    
    if (bIsShowerOnRightNow)
    {
      // False negative detected! Shower is actually still ON
      DebugPrintln("Double-check FAILED: Shower is actually still ON - was a false negative!");

    
#ifdef ENABLE_BLE
      snprintf(BLEText, sizeof(BLEText), "False negative\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE

      // Revert the state
      g_bIsShowerOn = true;

      // Continue with normal shower ON logic
      auto oldShowerOnTimeInMinutes = g_ShowerOnTimeInSeconds / 60;
      g_ShowerOnTimeInSeconds += TimeIntervalInSecondsWhileShowerOFF; // Add the 12 seconds we slept
      auto newShowerOnTimeInMinutes = g_ShowerOnTimeInSeconds / 60;
      
      if (newShowerOnTimeInMinutes != oldShowerOnTimeInMinutes)
      {
        // Update the display with the new number of minutes ON time
        Display::Init();
        Display::ShowNumber(newShowerOnTimeInMinutes);
        Display::PowerOff();
      }
      
      SleepUntilNextTimeInterval(TimeIntervalInMsWhileShowerON, StartTime);
      return;
    }
    else
    {
      // Double-check confirmed shower is OFF
      DebugPrintln("Double-check PASSED: Shower is confirmed OFF");
      
#ifdef ENABLE_BLE
      snprintf(BLEText, sizeof(BLEText), "Confirmed OFF\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE
      
      // Continue with normal shower OFF logic     
      SleepUntilNextTimeInterval(TimeIntervalInMsWhileShowerOFF, StartTime);
      return;
    }
  }

  // Perform main shower detection
  bool bIsShowerOnRightNow = false;
  float probability = 0.0f;
  int32_t avgAmplitude = 0;
  
  bIsShowerOnRightNow = DetectShowerState(avgAmplitude, probability);

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
      snprintf(BLEText, sizeof(BLEText), "Turned ON\n%1.2f", probability);
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
      snprintf(BLEText, sizeof(BLEText), "Still running\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE

      g_ShowerOnTimeInSeconds += timeIntervalInSeconds;
      auto newShowerOnTimeInMinutes = g_ShowerOnTimeInSeconds / 60;

      if (newShowerOnTimeInMinutes != oldShowerOnTimeInMinutes)
      {
        // Update the display with the new number of minutes ON time
        Display::Init();
        Display::ShowNumber(newShowerOnTimeInMinutes);
        Display::PowerOff();
      }
    }
  }
  else  // Right now the shower is OFF, but we need to determine if it just turned OFF, or has it been OFF already
  {
    timeIntervalInMs = TimeIntervalInMsWhileShowerOFF;

    if (g_bIsShowerOn)
    {
      DebugPrintln("Shower just turned OFF - setting up double-check");

      // NOTE: Here we could clear the display if we detect the shower turned OFF, but it's more interesting to keep showing the shower duration
      //       until the shower turns ON again. This way you can see for how long the previous person showered

#ifdef ENABLE_BLE
      snprintf(BLEText, sizeof(BLEText), "Turned OFF\n%1.2f", probability);
      SendBLEText(BLEText);
#endif  // ENABLE_BLE
      // Set flag for double-check and temporarily mark as OFF
      g_bNeedDoubleCheck = true;
      g_bIsShowerOn = false;
    }
    else
    {
      DebugPrintln("Shower is OFF");

#ifdef ENABLE_BLE
      snprintf(BLEText, sizeof(BLEText), "Shower is OFF\n%1.2f", probability);
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
// deepsleep in order to keep our 12 second time interval, otherwise the shower ON duration will be calculated wrongly
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
