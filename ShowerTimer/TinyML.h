// Functions that use Edge Impulse SDK and TensorFlow Lite to do ML inferencing
// This specifically uses the library ShowerTimer_inferencing that was exported from Edge Impulse for this project
// The Edge Impulse project can be found here: <TODO>

#ifndef _TINY_ML
#define _TINY_ML

/* Edge Impulse Arduino examples
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

 // If your target is limited in memory remove this macro to save 10K RAM
#define EIDSP_QUANTIZE_FILTERBANK   0

/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

#include <ShowerTimer_inferencing.h>

namespace TinyML
{
  static int16_t* pAudioBuffer = nullptr;

  // Get raw audio signal data
  static int GetRawDataFromAudioBuffer(size_t offset, size_t length, float* pRawData)
  {
    numpy::int16_to_float(&pAudioBuffer[offset], pRawData, length);
    return 0;
  }

  // Run inference to determine if shower is on
  bool IsShowerOn(int16_t* audioBuffer, const size_t numSamplesInAudioBuffer, float &probability)
  {
    DebugPrintln("TinyML::IsShowerOn()");
    
    if (audioBuffer == nullptr)
    {
      return false;
    }

    pAudioBuffer = audioBuffer;

    signal_t signal;
    signal.total_length = numSamplesInAudioBuffer;
    signal.get_data = &GetRawDataFromAudioBuffer;

    ei_impulse_result_t result = { 0 };

    if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK)
    {
      DebugPrintln("ERROR: run_classifier() failed");
      return false;
    }

#ifdef DEBUG
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
    {
      DebugPrintf("%s: %1.3f\n", result.classification[i].label, result.classification[i].value);
    }
#endif

    DebugPrintf("DSP: %d ms, Classification: %d ms\n", result.timing.dsp, result.timing.classification);

    const int indexOff = 0;
    const int indexOn = 1;
    const float valueOff = result.classification[indexOff].value;
    const float valueOn = result.classification[indexOn].value;

    probability = valueOn;
    bool bIsShowerOn = (valueOn > valueOff);

    return bIsShowerOn;
  }
}

#endif  // _TINY_ML