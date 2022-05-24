// GPIO mapping
// TFT_DC   D1
// TFT_RST  D2
// CO2_SDA  D3
// TFT_CS   D4
// TFT SCK  D5
// CO2_SCL  D6
// TFT SDA  D7
// LED_PIN  D8

// Edits to TFT_eSPI library
// User_Setup_Select.h - #include <User_Setups/Setup8_ILI9163_128x128.h>
// User_Setup.h - #define ILI9163_DRIVER
// Setup8_ILI9163_128x128.h - pins & height/width fix #define TFT_WIDTH 130 #define TFT_HEIGHT 129

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <User_Setup_Select.h>
#include <TJpg_Decoder.h>
#define FS_NO_GLOBALS
#include <FS.h>
#include <Wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <CircularBuffer.h>
#include <jled.h> // This is overkill for the single LED
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>

// User configurable
#include "credentials.h"

#define DEBUG true // toggle verbose serial output (bool)
#define ALTITUDE_ABOVE_SEA 17 // your altitude above sea level (int)

#define PPM_YELLOW 800
#define PPM_ORANGE 1200
#define PPM_RED 1600
#define LED_ALARM 1800 // ppm that LED should illuminate (int)

#define ENABLE_WIFI true
#define HOSTNAME "co2meter"

#define LED_PIN D8
#define ONBOARD_LED 2
#define AA_FONT_SMALL "fonts/NotoSansBold15"
#define AA_FONT_LARGE "fonts/NotoSansBold36"
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in Setup8_ILI9163_128x128.h
SCD30 airSensor;
AsyncWebServer server(80);

#define GRAPH_BEG_X 25
#define GRAPH_END_X 122
#define GRAPH_BEG_Y 48
#define GRAPH_END_Y 98

// Define LED function when in alarm state
// LED will fade-on in 250ms, stay on for 400ms, and fade-off in 250ms. Brightness is capped to 50/255
auto ledAlarm = JLed(LED_PIN).Breathe(250, 400, 250).DelayAfter(800).Repeat(5).MaxBrightness(50);

//====================================================================================
// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;
  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

void listFiles(void) {
  Serial.println();
  Serial.println("SPIFFS files found:");

  fs::Dir dir = SPIFFS.openDir("/"); // Root directory
  String  line = "=====================================";

  Serial.println(line);
  Serial.println("  File name               Size");
  Serial.println(line);

  while (dir.next()) {
    String fileName = dir.fileName();
    Serial.print(fileName);
    int spaces = 25 - fileName.length(); // Tabulate nicely
    while (spaces--) Serial.print(" ");
    fs::File f = dir.openFile("r");
    Serial.print(f.size()); Serial.println(" bytes");
  }

  Serial.println(line);
  Serial.println();
  delay(1000);
}

// Draw a + mark centred on x,y
void drawDatumMarker(int x, int y) {
  tft.drawLine(x - 5, y, x + 5, y, TFT_GREEN);
  tft.drawLine(x, y - 5, x, y + 5, TFT_GREEN);
}
//====================================================================================

int getYOffset(uint16_t co2) {
  // take co2 value and determine the Y axis offset
  // 50 pixel offset = 2000ppm
  int val = map(co2, 0, 2000, 0, (GRAPH_END_Y-GRAPH_BEG_Y));
  if (val > 50) {
    val = 50; // to stop >2000ppm from yeeting off the graph
  }
  return val;
}

CircularBuffer<uint16_t,(GRAPH_END_X-GRAPH_BEG_X-1)> measurement; // we -1 to not clash with our end x axis line
void updGraph(uint16_t co2) {
  if (DEBUG) { Serial.println("Updating graph data"); }
  // take the current co2 reading and push to buffer
  measurement.push(co2);

  // clear TFT area before plotting
  tft.fillRect(GRAPH_BEG_X+1, GRAPH_BEG_Y, (GRAPH_END_X-GRAPH_BEG_X-1), (GRAPH_END_Y-GRAPH_BEG_Y+1), TFT_BLACK);

  // for all readings in buffer
  for (int xOffset=0; xOffset<measurement.size(); xOffset++) {
    uint16_t co2Value = measurement[xOffset];
    int xValue = GRAPH_BEG_X + 1 + xOffset; // we +1 here so that xValue doesn't clash with our x axis line
    int yValue = GRAPH_END_Y - getYOffset(co2Value);

    // draw graph line w/ colour
    if (co2Value >= PPM_RED) {
      tft.drawLine(xValue, yValue, xValue, yValue, TFT_RED);
    } else if (co2Value >= PPM_ORANGE) {
      tft.drawLine(xValue, yValue, xValue, yValue, TFT_ORANGE);
    } else if (co2Value >= PPM_YELLOW) {
      tft.drawLine(xValue, yValue, xValue, yValue, TFT_YELLOW);
    } else {
      tft.drawLine(xValue, yValue, xValue, yValue, TFT_WHITE);
    }
  }
}

// Thanks: https://arduino.stackexchange.com/questions/28603/the-most-effective-way-to-format-numbers-on-arduino
/*
 * Format an unsigned long (32 bits) into a string in the format
 * "23,854,972".
 *
 * The provided buffer must be at least 14 bytes long. The number will
 * be right-adjusted in the buffer. Returns a pointer to the first
 * digit.
 */
char *ultoa(unsigned long val, char *s)
{
    char *p = s + 13;
    *p = '\0';
    do {
        if ((p - s) % 4 == 2)
            *--p = ',';
        *--p = '0' + val % 10;
        val /= 10;
    } while (val);
    return p;
}

void initWiFi() {
  uint8_t connectionRetries = 0;
  uint8_t maxConnectionRetries = 30;
  if (DEBUG) { Serial.println("Attempting to connect to Wi-Fi"); }
  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PW);

  while (WiFi.status() != WL_CONNECTED) {
    ++connectionRetries;
    if (connectionRetries == maxConnectionRetries) {
      Serial.printf("\nUnable to connect to WiFi after %d tries. Disabling...\n", connectionRetries);
      Serial.printf("Wi-Fi SSID: %s\n", WIFI_SSID);
      WiFi.disconnect(true);  // Disconnect from the network
      WiFi.mode(WIFI_OFF);    // Switch WiFi off
      return;
    }
    delay(500);
  }
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup(void) {
  Serial.begin(115200);
  // prep D8 LED
  pinMode(LED_PIN, OUTPUT);

  // start SCD30 sensor
  // Wire.begin(D3, D6);
  // if (!airSensor.begin()) {
  //   Serial.println("Air sensor not detected. Please check wiring. Freezing...");
  //   while (1);
  // }

  // tell SCD30 our altitude
  //airSensor.setAltitudeCompensation(ALTITUDE_ABOVE_SEA);

  // Serial.print("Auto calibration set to: ");
  // if (airSensor.getAutoSelfCalibration() == true)
  //     Serial.println("true");
  // else
  //     Serial.println("false");

  // int interval = airSensor.getMeasurementInterval();
  // Serial.print("Measurement Interval: ");
  // Serial.println(interval);

  // unsigned int altitude = airSensor.getAltitudeCompensation();
  // Serial.print("Current altitude: ");
  // Serial.print(altitude);
  // Serial.println("m");

  // float offset = airSensor.getTemperatureOffset();
  // Serial.print("Current temp offset: ");
  // Serial.print(offset, 2);
  // Serial.println(" C");

  // start filesystem
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  if (DEBUG) { Serial.println("Initialisation done."); listFiles(); } // Lists the files so you can see what is in the SPIFFS

  // load fonts
  bool font_missing = false;
  if (SPIFFS.exists("/fonts/NotoSansBold15.vlw")    == false) font_missing = true;
  if (SPIFFS.exists("/fonts/NotoSansBold36.vlw")    == false) font_missing = true;

  if (font_missing) {
    Serial.println("\r\nFont missing in SPIFFS, did you upload it?");
    while(1) yield();
  }

  // start JPG decoder
  tft.setSwapBytes(true); // We need to swap the colour bytes (endianess)
  TJpgDec.setCallback(tft_output); // The decoder must be given the exact name of the rendering function above

  // start TFT
  tft.init();
  tft.setRotation(2); // because our screen is upside-down
  tft.fillScreen(TFT_BLACK);

  // draw static images
  TJpgDec.setJpgScale(2); // The jpeg image can be scaled down by a factor of 1, 2, 4, or 8
  TJpgDec.drawFsJpg(2, 110, "/icons/temp.jpg");
  TJpgDec.drawFsJpg(114, 110, "/icons/humid.jpg");

  // draw static graph elements
  tft.drawLine(GRAPH_BEG_X, GRAPH_BEG_Y, GRAPH_BEG_X, GRAPH_END_Y, TFT_LIGHTGREY);
  tft.drawLine(GRAPH_END_X, GRAPH_BEG_Y, GRAPH_END_X, GRAPH_END_Y, TFT_LIGHTGREY);
  // draw the labels
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setCursor(4, 41);
  tft.println("2k");
  tft.drawLine(GRAPH_BEG_X-2, GRAPH_BEG_Y, GRAPH_BEG_X, GRAPH_BEG_Y, TFT_LIGHTGREY); // 2k

  tft.setCursor(4, 66);
  tft.println("1k");
  tft.drawLine(GRAPH_BEG_X-2, 73, GRAPH_BEG_X, 73, TFT_LIGHTGREY); // 1k

  tft.setCursor(12, 92);
  tft.println("0");
  tft.drawLine(GRAPH_BEG_X-2, GRAPH_END_Y, GRAPH_BEG_X, GRAPH_END_Y, TFT_LIGHTGREY); // 0

  tft.unloadFont();

  // start Wi-Fi connection
  if (ENABLE_WIFI) {
    tft.loadFont(AA_FONT_SMALL);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(1);
    tft.drawString("Connecting", 65, 4);
    tft.drawString("WiFi (15 sec)", 65, 22);
    tft.unloadFont();

    initWiFi();
  }

  // start mDNS
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("MDNS responder started: http://%s\n", HOSTNAME);
  }

  // define HTTP routes
  SPIFFS.begin();
  server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html").setCacheControl("no-cache");

  // a solution to this problem is making a function that stores the last readings co2/temp/humidity
  // this function will need to mutex lock to avoid reading/writing to it at the same time
  // but then we should be able to call the function here as well from loop()

  // server.on("/co2", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(200, "text/plain", String(co2));
  // });

  // server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(200, "text/plain", String(temp));
  // });

  // server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(200, "text/plain", String(hum));
  // });

  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument json(1024);
    json["heap"] = ESP.getFreeHeap();
    // json["ssid"] = WiFi.SSID();
    // json["ip"] = WiFi.localIP();
    json["co2"] = 0; // careful of race conditions here...
    serializeJson(json, *response);
    request->send(response);
  });

  // start HTTP server
  server.onNotFound(notFound);
  server.begin();
}

unsigned long timeRun = 0L;
unsigned long MinuteCounter = (60*1000L);
int xpos = 65; // half the screen width
int ypos = 4;
uint16_t lastReading = 0; // strore most recent co2 reading
bool firstRead = true;
char co2StringBuffer[14];

void loop() {
  // stop internal LED from blinking
  digitalWrite(ONBOARD_LED, HIGH);
  
  //drawDatumMarker(xpos, ypos);
  
  // if (airSensor.dataAvailable()) {
  //   // CO2
  //   lastReading = airSensor.getCO2();
  // tft.loadFont(AA_FONT_LARGE); // Must load the font first
  // tft.setTextDatum(1); // TC_DATUM = 1 = Top centre
  // tft.setTextPadding(100);
  // if (lastReading >= PPM_RED) {
  //   tft.setTextColor(TFT_RED, TFT_BLACK);
  // }  else if (lastReading >= PPM_ORANGE) {
  //   tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  // } else if (lastReading >= PPM_YELLOW) {
  //   tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  // } else {
  //   tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // }
  //   // do processing to turn into string with commas
  //   ultoa(lastReading, co2StringBuffer)
  //   tft.drawString(ultoa(lastReading, co2StringBuffer), xpos, ypos);
  //   tft.unloadFont();

       // to get the first graph dotpoint without having to wait a minute
  //   if (firstRead) {
  //     updGraph(lastReading);
  //     firstRead = false;
  //   }

    // tft.loadFont(AA_FONT_SMALL); // Must load the font first
    // tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour AND the background colour so the anti-aliasing works

    // // Temp
    // tft.setCursor(22, 112);
    // tft.print(airSensor.getTemperature(), 1); tft.print(" °C");

    // // Humidity
    // tft.setCursor(92, 112);
    // tft.print(airSensor.getHumidity(), 0);

    // tft.unloadFont();

  //   // for serial plotter
  //   Serial.println(airSensor.getCO2());
  // } else {
  //   Serial.println("Waiting for new data");
  // }

  // update graph every 1 min
  if (millis() - timeRun >= MinuteCounter) {
    timeRun += MinuteCounter;
    updGraph(lastReading);
  }

  // determine whether LED should be on/off
  if (lastReading >= LED_ALARM) {
    if (!ledAlarm.IsRunning()) {
      ledAlarm.Reset();
    }
    ledAlarm.Update();
  } else {
    ledAlarm.Stop();
  }

  // run mDNS service
  MDNS.update();
}
