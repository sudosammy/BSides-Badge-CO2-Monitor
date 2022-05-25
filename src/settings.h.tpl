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

