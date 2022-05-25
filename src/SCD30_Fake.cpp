#include "SCD30_Fake.h"

SCD30_Fake::SCD30_Fake(void)
{
  // Constructor
}

bool SCD30_Fake::begin(TwoWire &wirePort, bool autoCalibrate, bool measBegin)
{
  return (true);
}

// Returns true if device responds to a firmware request
bool SCD30_Fake::isConnected()
{
  return (true);
}

// Returns the latest available CO2 level
// If the current level has already been reported, trigger a new read
uint16_t SCD30_Fake::getCO2(void)
{
  if (co2HasBeenReported == true) // Trigger a new read
  {
    if (readMeasurement() == false) // Pull in new co2, humidity, and temp into global vars
      co2 = 0;                      // Failed to read sensor
  }

  co2HasBeenReported = true;

  return (uint16_t)co2; // Cut off decimal as co2 is 0 to 10,000
}

// Returns the latest available humidity
// If the current level has already been reported, trigger a new read
float SCD30_Fake::getHumidity(void)
{
  if (humidityHasBeenReported == true) // Trigger a new read
    if (readMeasurement() == false)    // Pull in new co2, humidity, and temp into global vars
      humidity = 0;                    // Failed to read sensor

  humidityHasBeenReported = true;

  return humidity;
}

// Returns the latest available temperature
// If the current level has already been reported, trigger a new read
float SCD30_Fake::getTemperature(void)
{
  if (temperatureHasBeenReported == true) // Trigger a new read
    if (readMeasurement() == false)       // Pull in new co2, humidity, and temp into global vars
      temperature = 0;                    // Failed to read sensor

  temperatureHasBeenReported = true;

  return temperature;
}

// Enables or disables the ASC
bool SCD30_Fake::setAutoSelfCalibration(bool enable)
{
  if (enable)
    return true; // Activate continuous ASC
  else
    return false; // Deactivate continuous ASC
}

// Get the temperature offset. See 1.3.8.
float SCD30_Fake::getTemperatureOffset(void)
{
  return 100.0;
}

// Set the temperature offset to remove module heating from temp reading
bool SCD30_Fake::setTemperatureOffset(float tempOffset)
{
  return true;
}

// Get the altitude compenstation. See 1.3.9.
uint16_t SCD30_Fake::getAltitudeCompensation(void)
{
  return 123;
}

// Set the altitude compenstation. See 1.3.9.
bool SCD30_Fake::setAltitudeCompensation(uint16_t altitude)
{
  return true;
}

// Set the pressure compenstation. This is passed during measurement startup.
// mbar can be 700 to 1200
bool SCD30_Fake::setAmbientPressure(uint16_t pressure_mbar)
{
  return true;
}

// SCD30_Fake soft reset
void SCD30_Fake::reset()
{
  //
}

// Get the current ASC setting
bool SCD30_Fake::getAutoSelfCalibration()
{
  return true;
}

// Begins continuous measurements
// Continuous measurement status is saved in non-volatile memory. When the sensor
// is powered down while continuous measurement mode is active SCD30_Fake will measure
// continuously after repowering without sending the measurement command.
// Returns true if successful
bool SCD30_Fake::beginMeasuring(uint16_t pressureOffset)
{
  return true;
}

// Overload - no pressureOffset
bool SCD30_Fake::beginMeasuring(void)
{
  return true;
}

// Stop continuous measurement
bool SCD30_Fake::StopMeasurement(void)
{
  return true;
}

// Sets interval between measurements
// 2 seconds to 1800 seconds (30 minutes)
bool SCD30_Fake::setMeasurementInterval(uint16_t interval)
{
  return true;
}

// Gets interval between measurements
// 2 seconds to 1800 seconds (30 minutes)
uint16_t SCD30_Fake::getMeasurementInterval(void)
{
  return (2);
}

// Returns true when data is available
unsigned long timeRun2 = 0;
unsigned long counter2 = 2500; // we make our fake sensor a little slower
bool SCD30_Fake::dataAvailable()
{
  if (millis() - timeRun2 >= counter2) {
    timeRun2 += counter2;
    // it's been time!
    return (true);
  }
  return (false);
}

// Get 18 bytes from SCD30_Fake
// Updates global variables with floats
// Returns true if success
int lastCo2F = 1000;
float lastTempF = 20.00;
float lastHumidF = 50.00;

bool SCD30_Fake::readMeasurement()
{
  // Verify we have data from the sensor
  // mock this because we uber cheated getting dataAvailable() working
  // if (dataAvailable() == false) {
  //   return (false);
  // }
    
  // make up data!!
  co2 = random(lastCo2F-50,lastCo2F+50);
  temperature = (float)random(lastTempF-1,lastTempF+1)+(float)random(1,99)/100.0;
  humidity = (float)random(lastHumidF-1,lastHumidF+1)+(float)random(1,99)/100.0;

  // Mark our global variables as fresh
  co2HasBeenReported = false;
  humidityHasBeenReported = false;
  temperatureHasBeenReported = false;

  return (true); // Success! New data available in globals.
}

// Gets a setting by reading the appropriate register.
// Returns true if the CRC is valid.
bool SCD30_Fake::getSettingValue(uint16_t registerAddress, uint16_t *val)
{
  return true;
}

// Gets two bytes from SCD30_Fake
uint16_t SCD30_Fake::readRegister(uint16_t registerAddress)
{
  //
  return 1234;
}

// Sends just a command, no arguments, no CRC
bool SCD30_Fake::sendCommand(uint16_t command)
{
  return (true);
}