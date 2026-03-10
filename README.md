# Shower Timer using ESP32-C3 Super Mini

An Arduino project that automatically detects if a shower is running and shows a timer for how long you’ve been showering. TinyML using microphone data is used to determine if the shower is running or not.

![Mounted](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Mounted.jpg)

![Front](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Front.jpg)

# Table of Contents
- [Introduction](#introduction)
- [Building your own](#building-your-own)
- [Building the device](#building-the-device)
- [Building the Arduino app](#building-the-arduino-app)
- [Creating your own ML model](#creating-your-own-ml-model)
- [Capturing ML training data](#capturing-ml-training-data)
- [Debugging inference results](#debugging-inference-results)
- [Detecting if the shower is ON/OFF](#detecting-if-the-shower-is-onoff)
- [Battery life](#battery-life)

---

# Introduction

An Arduino project that automatically detects if a shower is running and shows a timer for how long you’ve been showering. TinyML using microphone data is used to determine if the shower is running or not.

There are different ways to detect if the shower is on, e.g. small vibrations on the shower head could be detected using piezoelectric or gyroscope sensor. I tried a piezoelectric sensor, but the vibrations were too small to pick up, even though I could feel it with my hand. You could also modify the shower head pipe to install a small water flow sensor, but I didn't want to modify the shower. Therefore, I decided to use a small microphone.

This is a great machine learning application, since there are many different “white noises” in a bathroom, so it can be challenging to distinguish between them algorithmically. E.g. shower, hair dryer, filling a bathtub, toilet flush, extractor fan, electric toothbrush and shaver, vacuum cleaner, etc.

Since the device is in the shower, I had to waterproof it by cutting out two rubber gaskets, one sealing the display and one sealing the lid at the back. To protect the inside of the device from condensation, I applied transparent electronic silicone adhesive sealant on the circuit boards and solder points.

To conserve energy, an e-paper display is used which uses no power to display something. It only uses power to change something on the display, and that only happens once per minute while the shower is running.

This is considered version 1, which doesn't have fantastic battery life, since I didn't consider that even though the device goes into deepsleep, the microphone will still draw power at 1.4mA. This document includes a very detailed section on expected [battery life](#battery-life) and future improvements.

---

# Building your own

Building your own device involves
- [Building the actual device](#building-the-device)
- [Building the Arduino app](#building-the-arduino-app)
- Potentially [creating your own ML model](#creating-your-own-ml-model) if it doesn't work in your shower

<br>

| Folder                                  | Note                                        |
| --------------------------------------- |-------------------------------------------- |
| ShowerTimer/ShowerTimer/                | Main Arduino project                        |
| ShowerTimer/ShowerTimer_inferencing.zip | Edge Impulse Arduino lib to install         |
| ShowerTimer/3DModels/                   | Models to 3D print                          |
| ShowerTimer/Android/CollectDataOnPhone  | Android project to collect ML training data |
| ShowerTimer/Android/DebugOnPhone        | Android project to show debug info on phone |
| ShowerTimer/MLData.zip                  | My ML training audio recordings                     |

<br>

# Building the device

### **Image 1 - Components**

 - ESP32-C3 Super Mini
 - INMP441 i2s microphone
 - Waveshare 1.54inch E-Ink Display Module, black & white, 200x200 resolution, SPI interface
 - 0.8mm black silicone rubber
 - Battery holder for 3X 1.5V AAA batteries
 - PETG 3D printing filament

![Components](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Components.jpg)

---

### **Image 2 - 3D design**

- [MatterControl](#https://www.matterhackers.com/store/l/mattercontrol/sk/MKZGTDW6) source files included if you need to make modifications
- Print with PETG or anything that won't warp with heat and humidity

![Design](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Design.jpg)

---

### **Image 3 - Component placement**

![Assemble](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Assemble.jpg)

---

### **Image 4 - Wiring diagram**

![WireDiagram](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/WireDiagram.jpg)
---

### **Image 5 - Soldered**

![Soldered](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Soldered.jpg)

---

### **Image 6 - Waterproofing**

- Use sharp cutting tool or laser cutter to cut sealing gaskets out of 0.8mm silcone rubber sheet

![Waterproof](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Waterproof.jpg)

---

# Building the Arduino app

-	Install the Arduino IDE - https://www.arduino.cc/en/software
-	Configure Arduino IDE for ESP32 - https://randomnerdtutorials.com/installing-esp32-arduino-ide-2-0/
-	Install the below libraries - https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries/
    - Adafruit_GFX
    - [GxEPD2](https://github.com/ZinggJM/GxEPD2)
    - [ShowerTimer_inferencing](https://studio.edgeimpulse.com/public/879150/live) included as a zip file in this repro, generated by [Edge Impulse](https://www.edgeimpulse.com/)

For the final app, make sure to disable all defines for debugging and BLE connections. If not, your battery life will be extrememly short.

![DefinesFinalApp](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/DefinesFinalApp.jpg)

---

# Creating your own ML model

There’s a possibility that the device might not be accurate in your own bathroom, since your shower might sound different to mine, have different water pressure and water flow, different acoustics, different white noise, etc. In that case, you’ll need to build your own ML model using [Edge Impulse](#https://www.edgeimpulse.com/).

1) First, [build the device](#building-the-device) because you will need to close it up powered by batteries, and place it in your shower when capturing audio data. The shower will be on so you can’t have the device connected with wires that can get wet, and the acoustics will be different when the device is closed vs open.

2) Then, mount the device in your shower and [capture your own ML training data](#capturing-ml-training-data).


3) After that, clone my Edge Impulse project [ShowerTimer_inferencing](https://studio.edgeimpulse.com/public/879150/live) and add your own audio data to the project. It's a simple process, just follow documentation and examples.

4) Once added, click the button in Edge Impulse to retrain the model.

5) Click the button in Edge Impulse to export the model as an Arduino library. This will generate a .zip file that is similar to any other Ardauino library.

6) Replace the exsiting ShowerTimer_inferencing Arduino library with your own exported library.

7) Finally, rebuild the Arduino project.

---

# Capturing ML training data

ML will only work as well as your training data, i.e. bad training data will lead to poor results. Therefore, it's important to capture the training data exactly in the same way that you would use the device, i.e. similar location in the shower, similar everyday activities, device closed up for waterproofing, different water temperatures, etc.

Since the device will be closed up and installed in the shower, while the shower is running, data is captured and sent to your phone via BLE. This repo contains an Android app that connects with the device.

First, build the Arduino app with the following defines. Then, close it up and install it in the shower.

![DefinesCaptureData](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/DefinesCaptureData.jpg) 

The folder "ShowerTimer/Android/CollectDataOnPhone" contains an Android app for recording the audio data. The device and phone will automatically connect via BLE, no need for any pairing. There are two buttons on the phone app to record data. Pressing one of these buttons will send a request to the Arduino device to reboot and immediately start an audio recording, and when done it will send it to the phone. You should see the progress bar at the top while recording and transfer is happening. The recordings can be found on your phone's storage in the folder "Internal storage/Documents/ShowerTimer/"

![AndroidRecord](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/AndroidRecord.jpg)

What to record? Well, you want to record as much of a variety of the day-to-day sounds in the bathroom. I suggest first recording as many sounds as possible with the shower door closed and the shower OFF. E.g. do multiple recordings for each case while the bathtub is being filled, extractor fan is on\off, hair dryer on difference speeds, vacuum cleaner sweeping through the bathroom, toilet flushes, electric toothbrush or shaver on, walking around, etc. Then, do many recordings when the shower is running, perhaps even redo some of previous scenarios where the shower was previously OFF, but now the shower is ON. I.e. do a few recordings of the hair dryer on and the shower is on. It’s very important to not just do recordings while the shower is running, but also when you are inside the shower, since this will create different sounds as larger water droplets form and splash against the side of the shower, etc. Also, the shower will sound different when running cold, warm or hot water, since the water pressure might be different in those cases.

---

# Debugging inference results

If you're lucky, you'll train the new model with your new data and everything just works. If not, you will need to somehow debug what the ML model produces, perhaps even while you are standing in the shower. The folder "ShowerTimer/Android/DebugOnPhone" contains an Android app that will display results from the Arduino device. This includes a loudness metric, the ML probability that the shower is on, and the actual result. The device and phone will automatically connect via BLE, no need for any pairing. Compile the Arduino app with the following defines.

![DefinesDebugApp](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/DefinesDebugApp.jpg)

---

# Detecting if the shower is ON/OFF

Initially I thought this is a very simple problem to solve, since it’s very obvious when the shower is ON/OFF looking at the two graphics below which shows 250ms of audio data.

![ShowerOFF](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/ShowerOff.jpg)
*Figure 1 - Shower OFF*

![ShowerON](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/ShowerOn.jpg)
*Figure 2 - Shower ON*

But, I quickly realized that this isn’t always the case, since there can be lots of “white” noises in a bathroom, including shower running, bathtub getting filled/drowned, extractor fan, toilet flush, hair dryer at different speeds, electric toothbrush and shaver, etc. Shower sounds also change when the water is hot, cold and with different water pressure. See the two graphs below.

![ShowerOff_Loud](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/ShowerOff_Loud.jpg)
*Figure 3 - Hair dryer*

![ShowerOn_Quiet](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/ShowerOn_Quiet.jpg)
*Figure 4 - Shower ON*

 This let me decide to use [TinyML](#TinyML.org) as a solution, and it turned out to be fantastically reliable.

Initial results weren’t great though. The reason for this is that in order to save battery life, the device deepsleeps and right after it wakes up, it captures sound to process. The catch is that the microphone takes time to produce actual data, in my case 38ms, and after that it takes about 2 seconds to stabilize due to DC drift, we expect audio data to oscillate around zero. We expect data to show a straight line in a quiet environment, similar to Figure 1, but looking at the graph below there is a small section at the beginning of the graph that is just zero values, then a non-linear curve caused by DC drift, and only after about 2 seconds the data is stabilized, i.e. oscillating around zero.

![Quiet_DCDrift](https://github.com/ClaudeMarais/ShowerTimer_ESP32-C3-Mini/blob/main/Images/Quiet_DCDrift.jpg)
*Figure 5 - Non-linear audio data caused by DC drift*

 Training data was captured while the microphone already stabilized (Figure 1), but runtime inference was done while the microphone hasn’t produced data and was still unstable (Figure 3 & 4).

After realizing this, I captured new training data by starting the capture right after a wake from deepsleep. I also added a delay of 40ms after waking from a deepsleep to exclude any zeroed out data in the I2S DMA buffers from calling the API i2s_zero_dma_buffer(). This change gave an extremely reliable result, in fact I have yet to see a false positive or false negative!

I used Edge Impulse for my TinyML project, the full project with training data is available here: [Edge Impulse Shower Timer Project](https://studio.edgeimpulse.com/public/879150/live)

The design of the ML model is extremely simple. Audio MFE (Mel-frequency energy) is used as DSP (digital signal processing) to create ML features that is used as input into inferencing. The model itself basically consists of only two dense layers of 16 neurons each.

---

# Performance
Since the ML model is extremely simple, it is extremely fast to run on the microcontroller. Capturing the audio data and processing it take the majority of the time. A full cycle of the below takes 590ms on the ESP32-C3. For most of the day the bathroom will be quiet, and will run only for 260ms, which skips steps 4 and 5.
1)	Delay 40ms to wait for the microphone to start producing values
2)	Capture 210ms of audio data
3)	Determine if audio is loud enough, if not, no need to even run ML (<1ms)
4)	DSP (274 ms)
5)	ML inference (4ms)
6)	Deepsleep

---

# Battery Life 

This section summarizes three microphone/power architectures for the device:

1. **v1:** INMP441 always powered  
2. **v1.1:** INMP441 power-gated with a MOSFET  
3. **v2:** LP core + duty-cycled analog microphone  

It also explains how daily energy use is calculated so the numbers in each section are transparent and reproducible.

---

# How Daily Energy Use Is Calculated

All architectures share the same high-level behavior and the same usage assumptions:

- The household takes **4 showers per day**, each lasting **10 minutes**  
  → **40 minutes of shower time per day**
- The device wakes every 10 seconds to check whether the shower has started.
- Once the shower is confirmed ON, the device wakes **once per minute** to:
  - Run DSP + inference to confirm the shower is still active  
  - Refresh the e-paper display  
- DSP + inference requires ~590 ms of active ESP32 time at ~10 mA.
- However, most wakeups do NOT run the full 590 ms pipeline. 
  Each wake first performs a 260 ms low-cost audio capture to decide whether the sound is even loud enough to justify running the full ML pipeline.  
  - If quiet → go back to sleep  
  - If loud → run the full 590 ms DSP + inference  
- The microphone may be:
  - Always on (v1)
  - Powered only during wake intervals (v1.1)
  - Duty-cycled by the LP core (v2)

Daily energy use is computed from four components:

### 1. Baseline listening cost  
Energy consumed while the device is not in a shower state.  
This depends on:
- Wake frequency (every 10 seconds)
- Whether the microphone is powered continuously or only during wake windows
- Whether the LP core is running
- **Most wakeups only run the 260 ms "loudness check" not full DSP**

### 2. DSP + inference cost during showers  
During the **40 minutes of total shower time per day**, the device performs one full DSP window per minute.

- Each DSP + inferencing window: **590 ms at ~10 mA**
- Total windows per day: **40**

### 3. E-paper refresh cost  
The display updates once per minute during a shower.

- 40 refreshes per day  
- Each refresh: **2 seconds at ~8 mA**
- Total: **~0.18 mAh/day**

### 4. Shower off, but bathroom noisy  
If the environment produces ~1 hour/day of loud noise, the device may run additional DSP + inference windows.

This adds **~0.33 mAh/day**.

---

# 1. INMP441 Always On (Current v1 Hardware)

The INMP441 is an I²S digital microphone with no sleep mode.  
If wired directly to VDD (as in v1), it remains powered even during deep sleep.

On the **ESP32-C3 Super Mini**, measurements show:

- **Board deep-sleep baseline (no mic):** ~**0.7 mA**  
- **Deep sleep with mic powered:** ~**0.9 mA**  
  → Mic adds ~**0.2 mA** incremental current in deep sleep.

## Characteristics

| Property | Value |
|---------|--------|
| Microphone type | Digital (I²S) |
| Board deep-sleep baseline | ~0.7 mA |
| Mic current (always powered, incremental) | ~0.2 mA |
| ESP32 wake pattern | Every 10 s (shower OFF), once per minute (shower ON) |
| Loudness pre-check | 260 ms per wake |
| Full DSP + inference | Only when loud enough (or once per minute during shower) |
| E-paper refresh | Once per minute during showers |

## Daily Energy Use

| Component | mAh/day |
|----------|----------|
| Board deep-sleep baseline (0.7 mA × 24 h) | **16.8 mAh/day** |
| INMP441 incremental (0.2 mA × 24 h) | **4.8 mAh/day** |
| ESP32 + logic during wakes | ~6.6 mAh/day |
| E-paper | 0.18 mAh/day |
| **Total** | **~28.4 mAh/day** |

## Battery Life

| Battery | Life |
|---------|------|
| 1500 mAh AAA | **~1.7 months** |
| 2000 mAh LiPo (1600 usable) | **~1.9 months** |

# Why Deep Sleep Is ~0.7 mA on the ESP32-C3 Super Mini (Not Microamps)

Espressif’s datasheet advertises **10–20 µA** deep-sleep current — and that is accurate **for the bare ESP32-C3 chip or module**.

However, the **ESP32-C3 Super Mini** is a *development board*, and includes extra components that remain powered even in deep sleep:

| Component | Typical Current | Notes |
|----------|------------------|-------|
| **Red power LED** | 0.3–1.0 mA | Always on, powered from 3.3 V |
| **3.3 V regulator quiescent current** | 50–150 µA | Depends on regulator model |
| **USB-serial chip** | 0.3–1.0 mA | Powered whenever 5 V is present; off when powering via 3.3 V |
| **ESP32-C3 deep sleep** | 10–20 µA | As advertised |

When powering the Super Mini from **3.3 V directly**, the USB-serial chip is off, but the LED and regulator remain active.  
This results in a **real, measured deep-sleep current of ~0.7 mA**, which matches your measurements.

To reach microamp deep sleep, you would need:

- No power LED  
- No USB‑serial chip  
- A low-Iq regulator  
- A custom PCB or bare module  

The Super Mini is excellent for prototyping, but it cannot reach datasheet deep-sleep numbers without hardware modification.


---

# 2. INMP441 Controlled by MOSFET (v1.1 Concept)

In this design, the INMP441 is power-gated using a MOSFET.  
The microphone is powered only during wake intervals.

Measurements show:

- **Deep sleep with mic power-gated:** ~**0.7 mA** (board baseline)  
- **Deep sleep with mic always powered:** ~**0.9 mA**  
  → Power-gating saves ~**0.2 mA** continuously in deep sleep.

There is also a **small active-mode overhead**:

- **Init mic without MOSFET:** ~**20.1 mA**  
- **Init mic with MOSFET:** ~**23.2 mA**  
  → MOSFET path adds ~**3.1 mA** only during short “mic on” windows (260 ms pre-checks + 590 ms DSP windows).  
Because those windows are a tiny fraction of the day, the **0.2 mA 24/7 deep-sleep savings dominates**.

## Characteristics

| Property | Value |
|---------|--------|
| Microphone type | Digital (I²S) |
| Board deep-sleep baseline | ~0.7 mA |
| Mic current | 0 mA outside wake windows (power-gated) |
| ESP32 wake pattern | Every 10 s (shower OFF), once per minute (shower ON) |
| Loudness pre-check | 260 ms per wake |
| Full DSP + inference | Only when loud enough (or once per minute during shower) |
| E-paper refresh | Once per minute during showers |

## Daily Energy Use

| Component | mAh/day |
|----------|----------|
| Board deep-sleep baseline (0.7 mA × 24 h) | **16.8 mAh/day** |
| ESP32 + mic during wakes | ~6.6 mAh/day |
| E-paper | 0.18 mAh/day |
| **Total** | **~23.6 mAh/day** |

## Battery Life

| Battery | Life |
|---------|------|
| 1500 mAh AAA | **~2.1 months** |
| 2000 mAh LiPo (1600 usable) | **~2.2 months** |

## Summary

- Power-gating the INMP441 increases battery life from **~1.7 months → ~2.1 months** on the ESP32‑C3 Super Mini.  
- The 260 ms loudness pre-check avoids unnecessary DSP work.  
- The MOSFET introduces a small active-mode overhead (~3.1 mA during short mic-on windows), but the **0.2 mA deep‑sleep savings 24/7 still wins overall**.

---

# 3. LP Core + Duty-Cycled Analog Microphone (v2 Concept)

This design replaces the INMP441 with a low-power analog MEMS microphone and uses:

- An ESP32-**S3** with LP (low power) core to read an envelope detector or ADC with simple analog microphone
- Main cores wake only when loudness is sustained  
- Shower-ON behavior remains: one wake per minute
- A duty cycle (e.g., 200 ms ON every 2 s → 10%)  


## Characteristics

| Property | Value |
|---------|--------|
| Microphone type | Analog MEMS |
| Mic current (ON) | ~0.6 mA |
| Duty cycle | 10% |
| LP core current | ~0.05 mA |
| Loudness pre-check | Done by LP core, extremely cheap |
| Full DSP + inference | Once per minute during showers |
| E-paper refresh | Once per minute during showers |

## Daily Energy Use

| Component | mAh/day |
|----------|----------|
| Duty-cycled mic + LP core | 2.64 mAh/day |
| DSP + inference | 0.065 mAh/day |
| E-paper | 0.18 mAh/day |
| **Total** | **~2.89 mAh/day** |
| With 1 noisy hour/day | **~3.22 mAh/day** |

## Battery Life

| Battery | Quiet Day | Noisy Hour |
|---------|-----------|------------|
| 1500 mAh AAA | **~17.3 months** | **~15.5 months** |
| 2000 mAh LiPo (1600 usable) | **~18.4 months** | **~16.5 months** |

## Summary

- LP core + analog mic enables **~18 months** battery life.
- The LP core handles loudness checks with almost no cost.
- This is the most efficient architecture.

---

# Overall Comparison

| Architecture | Avg Current | Daily mAh | Battery Life (AAA) | Notes |
|--------------|-------------|-----------|---------------------|-------|
| **1. INMP441 always on** | ~**1.18 mA** | ~**28.4 mAh** | **~1.7 months** | Current v1 (Super Mini) |
| **2. INMP441 + MOSFET** | ~**0.98 mA** | ~**23.6 mAh** | **~2.1 months** | v1.1 (Super Mini) |
| **3. LP core + analog mic** | ~**0.12 mA** | ~**2.9 mAh** | **~17.3 months** | v2 concept (ESP32-S3) |

---

# Key Takeaways

- Most wakeups only run a **260 ms loudness check**, not full DSP.
- Waking once per minute during a shower dramatically reduces DSP work.
- Power-gating the INMP441 increases battery life only marginally.
- LP core + analog mic reaches **~17-18 months**.
- The longer the shower runs, the more efficient the device becomes.
