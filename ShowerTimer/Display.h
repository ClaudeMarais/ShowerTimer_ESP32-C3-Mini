// Functions to display our information on an e-paper display. E-paper is used in order to extend battery life of the device
// This uses the GxEPD2 Arduino library, see https://github.com/ZinggJM/GxEPD2, which has a dependancy on Adafruit_GFX
//
// This project was developed using Waveshare 1.54inch E-Ink Display Module, black & white, 200x200 resolution, SPI interface
// Refresh rate of display: 0.3 seconds for a partial update, 2 seconds full update
// A full update is required when the display powers up, after that only partial updates are required

#ifndef _DISPLAY
#define _DISPLAY

// Defines for e-paper display
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_154_GDEY0154D67    // Specific driver that works with the above mentioned Waveshare e-paper display
#define GxEPD2_BW_IS_GxEPD2_BW true
#define MAX_DISPLAY_BUFFER_SIZE 65536ul
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

#include <GxEPD2_BW.h>
#include <FreeSansBold80pt7b.h>   // Large font, since we only intent to display a maximum of two characters, e.g. 10 for 10 minutes. Hopefully nobody showers for more than 99 minutes!

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(DISPLAY_CS, DISPLAY_DC, DISPLAY_RST, DISPLAY_BUSY));

namespace Display
{
  void Init()
  {
    DebugPrintln("Display::Init()");
    display.init(115200, false, 2, false); // NOTE: Use this for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  }

  void PowerOff()
  {
    display.powerOff();
  }

  void Clear()
  {
    display.fillScreen(GxEPD_WHITE);
    display.display(false); // This does a full slow update but, ensures the screen is truly clear without random ghost pixels
  }

  // Shows characters centered on the screen. Not that quickUpdate cannot be used right after the device powers up, it requires a full/slow update,
  // otherwise you see random ghost pixels
  void ShowCenteredString(const char* string, bool quickUpdate = false)
  {
    display.fillScreen(GxEPD_WHITE);
    display.setRotation(1);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold80pt7b);
    display.setTextSize(1);

    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(string, 0, 0, &tbx, &tby, &tbw, &tbh);

    // center bounding box by transposition of origin
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    uint16_t y = ((display.height() - tbh) / 2) - tby;

    display.setCursor(x, y);
    display.print(string);
    display.display(quickUpdate);
  }

  void ShowNumber(const int32_t number, bool quickUpdate = false)
  {
    char string[8];
    sprintf(string, "%d", number);
    ShowCenteredString(string, quickUpdate);
  }

  void ShowCharacter(const char character, bool quickUpdate = false)
  {
    char string[8];
    sprintf(string, "%c", character);
    ShowCenteredString(string, quickUpdate);
  }

}

#endif  // _DISPLAY