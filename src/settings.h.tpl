// self-explanatory file
#define ALTITUDE_ABOVE_SEA  17 // your altitude above sea level (int)

#define ENABLE_WIFI         true
#define HOSTNAME            "co2meter" // hostname for mDNS
#define WIFI_SSID           "MySSID"
#define WIFI_PW             "123456789"

#define DEBUG               false // toggle verbose serial output (bool)
#define FAKE_SENSOR         false // toggle fake sensor data (bool)

// CO2 parts per million that should trigger change in display/graph colours
#define PPM_YELLOW          800
#define PPM_ORANGE          1200
#define PPM_RED             1600
#define LED_ALARM           1800 // ppm that LED should illuminate

/*
If you're using the BSides Perth badge 2018 but *not*
using PlatformIO you'll need edit the TFT_eSPI library:

User_Setup.h:
//#define ILI9341_DRIVER
#define ILI9163_DRIVER

User_Setup_Select.h:
#include <User_Setups/Setup8_ILI9163_128x128.h>

Setup8_ILI9163_128x128.h:
#define TFT_WIDTH 130
#define TFT_HEIGHT 129
#define TFT_CS   PIN_D4
#define TFT_DC   PIN_D1
#define TFT_RST  PIN_D2

These are confgiured for PlatformIO in platformio.ini
*/
