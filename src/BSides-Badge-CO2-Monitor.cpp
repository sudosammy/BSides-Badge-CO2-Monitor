// GPIO mappings
//          D0  16
// TFT_DC   D1  5
// TFT_RST  D2  4
// CO2_SDA  D3  0
// TFT_CS   D4  2
// TFT_SCK  D5  14
// CO2_SCL  D6  12
// TFT_SDA  D7  13
// LED_PIN  D8  15

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
#include <jled.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>

// User configurations
#include "settings.h"

#define LED_PIN D8
#define ONE_HOUR 3600000UL

#define AA_FONT_SMALL "fonts/NotoSansBold15"
#define AA_FONT_LARGE "fonts/NotoSansBold36"
TFT_eSPI tft = TFT_eSPI();

#if FAKE_SENSOR
  #include <SCD30_Fake.h>
  SCD30_Fake airSensor;
#else
  SCD30 airSensor;
#endif

AsyncWebServer server(80);

#define GRAPH_BEG_X 25
#define GRAPH_END_X 122
#define GRAPH_BEG_Y 48
#define GRAPH_END_Y 98

// Define LED function when in alarm state
// LED will fade-on in 150ms, stay on for 400ms, and fade-off in 150ms. Brightness is capped to 50/255
auto ledAlarm = JLed(LED_PIN).Breathe(150, 400, 150).Repeat(1).MaxBrightness(50);

// Globals for storing the most recent measurements
uint16_t lastCo2 = 0;
float lastTemp, lastHumidity = 0.00;

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
//====================================================================================

void setTimezone(String timezone){
  setenv("TZ",timezone.c_str(),1);
  tzset();
}

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
  if (DEBUG) { Serial.println("Updating TFT graph data"); }
  measurement.push(co2); // take the most current co2 reading and push to CircularBuffer

  tft.fillRect(GRAPH_BEG_X+1, GRAPH_BEG_Y, (GRAPH_END_X-GRAPH_BEG_X-1), (GRAPH_END_Y-GRAPH_BEG_Y+1), TFT_BLACK); // clear TFT area before plotting

  for (int xOffset=0; xOffset<measurement.size(); xOffset++) { // plot all values in CircularBuffer
    uint16_t co2Value = measurement[xOffset];
    int xValue = GRAPH_BEG_X + 1 + xOffset; // we +1 here so that xValue doesn't clash with our x axis line
    int yValue = GRAPH_END_Y - getYOffset(co2Value);

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

CircularBuffer<time_t,120> timeBuffer; // 180 entries = 3 hours
CircularBuffer<uint16_t,120> co2Buffer;
CircularBuffer<float,120> tempBuffer;
CircularBuffer<float,120> humidityBuffer;
DynamicJsonDocument json(9920); // 4096 bytes = 51 minutes (9920 = 2 hours)
char* bigJSON = new char[9920];
void updTable(uint16_t co2, float temp, float humidity) {
  if (DEBUG) { Serial.println("Updating table data"); }
  timeBuffer.push(time(NULL));
  co2Buffer.push(co2);
  tempBuffer.push(temp);
  humidityBuffer.push(humidity);

  for (int i=0; i<timeBuffer.size(); i++) {
    json["data"][i][0] = timeBuffer[i];
    json["data"][i][1] = co2Buffer[i];
    json["data"][i][2] = tempBuffer[i];
    json["data"][i][3] = humidityBuffer[i];
  }

  serializeJson(json, bigJSON, measureJson(json));
}

// Thanks: https://arduino.stackexchange.com/questions/28603/the-most-effective-way-to-format-numbers-on-arduino
char *ultoa(unsigned long val, char *s) {
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

void updateReadings() {
  if (airSensor.dataAvailable()) { // check if update available
    // so i thought this was going to need to be atomic/async safe to avoid race conditions
    // but it turns out trying to do that causes way more problems lol
    lastCo2 = airSensor.getCO2();
    lastTemp = airSensor.getTemperature();
    lastHumidity = airSensor.getHumidity();
  } else {
    if (lastCo2 == 0) {
      Serial.println("A call to updateReadings() was made before the senor had populated the lastReading struct...");
    }
  }
}

int getJSONChunk(char *buffer, int maxLen, size_t index) {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that has been already sent
  //You will be asked for more data until 0 is returned
  int sizeBigJSON = measureJson(json);
  size_t max = (ESP.getFreeHeap() / 3) & 0xFFE0;
  // Get the chunk based on the index and maxLen
  size_t len = sizeBigJSON - index;
  if (len > maxLen) len = maxLen;
  if (len > max) len = max;
  if (len > 0) {
    if (DEBUG) { Serial.printf("Adding %i bytes to buffer\n", len); }
    memcpy(buffer, bigJSON + index, len);
  }
  if (len == 0) {
    if (DEBUG) { Serial.println("Complete buffer sent."); }
  }
  return len; // Return the actual length of the chunk (0 for end of file)
};

/*
This function doesn't work because we can't (currently) control the backlight of the TFT screen
on the BSides badge :( This function turns the whole screen white during the "off" period.

It will require more investigation to determine whether we can utilise D0 to control the backlight.
*/
bool tftState = true; // true = on
void tftSleep() {
  time_t currTime;
	currTime = time(NULL);
	tm *tmLocal = localtime(&currTime);

  if (tmLocal->tm_hour >= 0 && tmLocal->tm_hour <= 8) {
    if (tftState) {
      if (DEBUG) { Serial.printf("Hour is %i, sleep TFT...\n", tmLocal->tm_hour); }
      tft.writecommand(0x10); delay(5); // if time within boundary, turn TFT off
      tftState = false;
    }
  } else {
    if (!tftState) {
      if (DEBUG) { Serial.printf("Hour is %i, wake TFT...\n", tmLocal->tm_hour); }
      tft.writecommand(0x11); delay(120); // otherwise, turn TFT on
      tftState = true;
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("");
  pinMode(LED_PIN, OUTPUT); // prep D8 LED

  // start filesystem
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield();
  }

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
  // draw labels
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // 2k
  tft.setCursor(4, 41);
  tft.println("2k");
  tft.drawLine(GRAPH_BEG_X-2, GRAPH_BEG_Y, GRAPH_BEG_X, GRAPH_BEG_Y, TFT_LIGHTGREY);
  // 1k
  tft.setCursor(4, 66);
  tft.println("1k");
  tft.drawLine(GRAPH_BEG_X-2, 73, GRAPH_BEG_X, 73, TFT_LIGHTGREY);
  // 0
  tft.setCursor(12, 92);
  tft.println("0");
  tft.drawLine(GRAPH_BEG_X-2, GRAPH_END_Y, GRAPH_BEG_X, GRAPH_END_Y, TFT_LIGHTGREY);

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
  if (ENABLE_WIFI && MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("MDNS responder started: http://%s\n", HOSTNAME);
  }

  // define HTTP routes
  SPIFFS.begin();
  server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html").setCacheControl("no-cache");

  server.on("/co2", HTTP_GET, [](AsyncWebServerRequest *request) {
    String co2String = String(lastCo2);
    request->send(200, "text/plain", co2String);
  });

  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request) {
    String tempString = String(lastTemp);
    request->send(200, "text/plain", tempString);
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    String humidString = String(lastHumidity);
    request->send(200, "text/plain", humidString);
  });

  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument json(128);
    json["heap"] = ESP.getFreeHeap();
    json["co2"] = lastCo2;
    json["temp"] = lastTemp;
    json["humidity"] = lastHumidity;
    serializeJson(json, *response);
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.on("/table", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      return getJSONChunk((char *)buffer, (int)maxLen, index);
    });
    response->addHeader("Cache-Control", "max-age=60, must-revalidate");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument json(512);
    json["data"][0][0] = "WIFI_SSID";
    json["data"][0][1] = WIFI_SSID;

    json["data"][1][0] = "WIFI_PW";
    json["data"][1][1] = "********";

    json["data"][2][0] = "TIMEZONE";
    json["data"][2][1] = TIMEZONE;

    json["data"][3][0] = "DEBUG";
    json["data"][3][1] = DEBUG;

    json["data"][4][0] = "FAKE_SENSOR";
    json["data"][4][1] = FAKE_SENSOR;

    json["data"][5][0] = "ALTITUDE_ABOVE_SEA";
    json["data"][5][1] = ALTITUDE_ABOVE_SEA;

    json["data"][6][0] = "PPM_YELLOW";
    json["data"][6][1] = PPM_YELLOW;

    json["data"][7][0] = "PPM_ORANGE";
    json["data"][7][1] = PPM_ORANGE;

    json["data"][8][0] = "PPM_RED";
    json["data"][8][1] = PPM_RED;

    json["data"][9][0] = "LED_ALARM";
    json["data"][9][1] = LED_ALARM;

    serializeJson(json, *response);
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.printf("Redirecting to admin.html page");
    request->redirect("/admin.html");
  });

  server.on("/admin", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("PPM", true)) {
      uint16_t ppmCalibrate = request->getParam("PPM", true)->value().toInt();
      Serial.printf("Calibrating with Co2 PPM: %i\n", ppmCalibrate);

      if (airSensor.setForcedRecalibrationFactor(ppmCalibrate)) {
        request->redirect("/admin.html?msg=success");
      } else {
        request->redirect("/admin.html?msg=error");
      }
    }
    request->redirect("/admin.html?msg=invalid");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  // start HTTP server
  server.begin();

  // set timezone & configure NTP
  if (ENABLE_WIFI) {
    configTime(0, 0, NTP_SERVER);
    setTimezone(TIMEZONE);
    Serial.printf("Timezone set to: %s\n", TIMEZONE);
  }

  // start SCD30 sensor (it likes being last...)
  Wire.begin(SCD30_SDA, SCD30_SCL);
  if (!airSensor.begin()) {
    Serial.println("SCD30 not detected. Please check wiring. Freezing...");
    while (1);
  }

  airSensor.setAltitudeCompensation(ALTITUDE_ABOVE_SEA); // tell SCD30 our altitude

  // print various sensor information
  Serial.print("Auto calibration set to: ");
  if (airSensor.getAutoSelfCalibration() == true) {
    Serial.println("true");
  } else {
    Serial.println("false");
  }

  int interval = airSensor.getMeasurementInterval();
  Serial.print("Measurement Interval: "); Serial.println(interval);

  unsigned int altitude = airSensor.getAltitudeCompensation();
  Serial.print("Current altitude: "); Serial.print(altitude); Serial.println("m");

  float offset = airSensor.getTemperatureOffset();
  Serial.print("Current temp offset: "); Serial.print(offset, 2); Serial.println(" C");
}

unsigned long timeRun = 0L; // for graph timer
#if !FAKE_SENSOR
unsigned long MinuteCounter = (60*1000L); // for graph timer
#else
unsigned long MinuteCounter = (5*1000L); // Also make time go faster
#endif
bool firstRead = true;
char co2StringBuffer[14];

void loop() {
  unsigned long currentMillis = millis();
  updateReadings(); // check if, and update when, new sensor values are available

  tft.loadFont(AA_FONT_LARGE); // Must load the font first
  tft.setTextDatum(TC_DATUM); // Top centre
  tft.setTextPadding(100);
  if (lastCo2 >= PPM_RED) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  }  else if (lastCo2 >= PPM_ORANGE) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else if (lastCo2 >= PPM_YELLOW) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  // convert uint into string with "," on >=1000
  tft.drawString(ultoa(lastCo2, co2StringBuffer), 65, 4); // x-axis: 65 (half of 130px), y-axis: 4 (any lower and the text padding crops the top of the "2k")
  tft.unloadFont();

  if (firstRead) { // to get the first graph/table plot without having to wait a minute
    updGraph(lastCo2);
    updTable(lastCo2, lastTemp, lastHumidity);
    firstRead = false;
  }

  tft.loadFont(AA_FONT_SMALL); // Must load the font first
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour AND the background colour so the anti-aliasing works
  tft.setTextDatum(TL_DATUM); // Top left
  tft.setTextPadding(20);
  // Temp
  tft.drawFloat(lastTemp, 1, 22, 112); tft.print(" °C");
  // Humidity
  tft.drawFloat(lastHumidity, 0, 92, 112);
  tft.unloadFont();

  // for serial plotter
  //Serial.println(lastCo2);

  if (currentMillis - timeRun >= MinuteCounter) { // update graph & table every 1 min
    timeRun += MinuteCounter;
    updGraph(lastCo2);
    updTable(lastCo2, lastTemp, lastHumidity);
    // tftSleep();
  }

  if (lastCo2 >= LED_ALARM) { // determine whether LED should be on/off
    if (!ledAlarm.IsRunning()) {
      ledAlarm.Reset();
    }
    ledAlarm.Update();
  } else {
    ledAlarm.Stop();
  }

  if (ENABLE_WIFI) { MDNS.update(); }
  delay(1000); // this is needed or SCD30 spits the dummy
}
