// Functions to capture audio data from an i2s microphone
// This project was developed using a small INMP441 i2s microphone

#ifndef _AUDIO
#define _AUDIO

#include "driver/i2s.h"

namespace Audio
{
  // Duration in ms of when the device is powered on, but does not actually produce actual audio data. I.e. how long it takes to not see zeros
  // from calling i2s_zero_dma_buffer after the device powers on. With the INMP441 used during development, actual data was only produced
  // 35ms after the device powers up
  const unsigned long MicrophoneDeadTimeInMs = 35;
  const unsigned long IgnoreMicrophoneDataTimeInMs = MicrophoneDeadTimeInMs + 5;  // Add an extra 5ms for safety

  // Info about audio, 16-bit per sample, mono
  const i2s_bits_per_sample_t BitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
  const uint32_t BytesPerSample = BitsPerSample / 8;
  const i2s_channel_t NumChannels = I2S_CHANNEL_MONO;

  // Number bytes to drain from i2s to make sure we get actual microphone data
  const size_t BytesToDrain = IgnoreMicrophoneDataTimeInMs * 8 * 2; // 8 samples per ms, 2 bytes per sample, e.g 40ms -> 640 bytes
  uint8_t DrainBuffer[BytesToDrain];

  // i2s DAM buffer setup
  const uint32_t NumSamplesPerDMABuffer = 1024;
  const uint32_t NumDMABuffers = 2;

  // Setup
  bool Setup(const uint32_t sampleRate)
  {
    DebugPrintln("Audio::Setup()");

    i2s_config_t i2s_config =
    {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sampleRate,
      .bits_per_sample = BitsPerSample,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = 0,
      .dma_buf_count = NumDMABuffers,
      .dma_buf_len = NumSamplesPerDMABuffer,
      .use_apll = true
    };

    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK)
    {
      DebugPrintln("ERROR: i2s_driver_install() failed");
      return false;
    }

    const i2s_pin_config_t pin_config =
    {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD
    };

    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK)
    {
      DebugPrintln("ERROR: i2s_set_pin() failed");
      return false;
    }
  
    // Clear DMA buffers
    if (i2s_zero_dma_buffer(I2S_NUM_0) != ESP_OK)
    {
      DebugPrintln("ERROR: i2s_zero_dma_buffer() failed");
      return false;
    }

    // Most microphones need a short amount of startup stabilization time to prevent getting garbage data. But, we can't just wait it out, we have to
    // actully drain the unwanted data form the DMA buffers
    //delay(IgnoreMicrophoneDataTimeInMs);   
    size_t numBytesRead;
    if (i2s_read(I2S_NUM_0, (void*)DrainBuffer, BytesToDrain, &numBytesRead, portMAX_DELAY) != ESP_OK)
    {
      DebugPrintln("ERROR: i2s_read() failed");
      return false;
    }

    return true;
  }

  void Shutdown(void)
  {
    i2s_driver_uninstall(I2S_NUM_0);
  }

  // Get audio data
  bool GetData(int16_t* audioBuffer, const size_t numSamplesInBuffer)
  {
#ifdef DEBUG
    auto startTime = millis();
#endif

    size_t bufferSizeInBytes = numSamplesInBuffer * BytesPerSample;
    size_t numBytesRead;

    if (i2s_read(I2S_NUM_0, (void*)audioBuffer, bufferSizeInBytes, &numBytesRead, portMAX_DELAY) != ESP_OK)
    {
      DebugPrintln("ERROR: i2s_read() failed");
      return false;
    }

#ifdef DEBUG
    DebugPrintf("Audio::GetData() %d ms\n", uint32_t(millis() - startTime));
#endif

    return true;
  }

  // Preprocess audio data and also calculate the average amplitude as a loudness metric. Since there can be DC drift on the microphone
  // right after the device powers up or wakes up from deepsleep, we use "first-difference energy" instead of regular averaging
  void PreprocessData(int16_t* audioBuffer, const size_t numSamplesInBuffer, const int32_t volumeScale, int32_t &avgAmplitude)
  {
#ifdef DEBUG
    auto startTime = millis();
#endif

    int64_t diffSum = 0;

    // Process first sample
    int32_t prev = audioBuffer[0];
    prev *= volumeScale;
    if (prev > INT16_MAX) prev = INT16_MAX;
    if (prev < INT16_MIN) prev = INT16_MIN;
    audioBuffer[0] = int16_t(prev);

    // Process the rest
    for (size_t i = 1; i < numSamplesInBuffer; i++)
    {
      int32_t value = audioBuffer[i];

      // Scale and clamp
      value *= volumeScale;
      if (value > INT16_MAX) value = INT16_MAX;
      if (value < INT16_MIN) value = INT16_MIN;

      // DC drift invariant loudness metric
      diffSum += abs(value - prev);

      // Store processed sample
      audioBuffer[i] = int16_t(value);

      // Update previous sample
      prev = value;
    }

    avgAmplitude = diffSum / (numSamplesInBuffer - 1);

#ifdef DEBUG
    DebugPrintf("Audio::PreprocessData() %d ms\n", uint32_t(millis() - startTime));
#endif

  }
}

#endif  // _AUDIO