# BSides Perth 2018 Badge - Environment Monitor

A portable CO2, temperature, and humidity monitor with a WiFi-enabled dashboard, data logging & CSV export using the BSides Perth 2018 badge.

This code is compatible with any ESP8266 microcontroller but some modifications will be necessary to convert it to another badge or board.

![CO2 Monitor sitting outside](github_pics/outside.jpg)

## Features:
* Highly accurate measurements with the SCD30 sensor
* TFT screen showing the last 90 minutes of readings along with the most current CO2, temp, and humidity readings
* Visual alarm (via orange LED) when CO2 measurements exceed recommended levels
* An mDNS enabled web application with AJAX powered (auto-updating) graphs and data table
* The last 2 hours of readings are automatically logged and can be exported as CSV
* Uses primarily recycled goods!
* Total cost ~$50 AUD

What to buy:
1. [SCD30 sensor](https://www.aliexpress.com/item/1005001392172293.html) (this was the seller I went with)
2. [2x AA Battery holder w/ switch](https://www.jaycar.com.au/2aa-switched-battery-enclosure/p/PH9280)

It's assumed you have access to a small amount of copper wire, a soldering iron, and solder.

## Schematic:

This schematic shows only the connections necessary to convert the BSides badge. If you are going to use this project with a different badge/TFT screen, you may need to make modifications here. That is outside of this project's scope.

![Schematic](github_pics/schematic.png)

## Compiling:
1. Install [PlatformIO](https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation) for VSCode
2. Clone this repository
3. Open this project in VSCode
4. Check out `settings.h.tpl`, complete, and rename to `settings.h`
5. Build and upload the `Filesystem Image`
6. Build and upload the project.

## Other notes:
1. It works better on 5V so you might want to use a USB battery pack (like a portable phone charger) rather than running from AA batteries.
2. TODO: It's undoubtedly possible to improve the amount of data logged by improving the efficiency of the CircularBuffers. Some ways I can think of that might work:
    * Do not store the UNIX timestamp in the buffer. Store the UNIX time of the first reading as a global then use the size of the `CircularBuffer<uint16_t,120> co2Buffer;`, adding 60 seconds for each measurement.
    * Reuse the `CircularBuffer<uint16_t,120> co2Buffer;` for the TFT graph.
    * Store temp and humidity as two `uint8_t`'s (i.e. `uint8 + "." + uint8`) rather than 32bit floats. This reduces the precision from 6 decimal places to 3 but halves the RAM required.

## Battery Life

On typical Duracell LR6 AA batteries, I was able to get >2 hours of good usage with WiFi enabled & connected. As the battery ran low the TFT backlight began flickering, the CO2 readings began to be suspiciously low, and eventually after `4 hours 23 minutes` the SCD30 stopped collecting data.

## Other Pics:

![CO2 Monitor inside](github_pics/inside.jpg)

Necessary connections for SCD30 sensor:

![CO2 Monitor image of back](github_pics/back.jpg)

Web Application (it's mobile responsive too):

![CO2 Monitor web application](github_pics/dashboard.png)