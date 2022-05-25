/*

A quick and dirty mock-up of the SCD30 SparkFun library.
It does not fully implement any functions but returns success responses and (where applicable) mock data.
This can be used to seed "random" data for the gadget to perform dry-runs without the sensor attached or test UI elements.

Activate this library wiith the "FAKE_SENSOR" setting.

*/
#include "Arduino.h"
#include <Wire.h>

class SCD30_Fake
{
public:
	SCD30_Fake(void);

	bool begin(bool autoCalibrate) { return begin(Wire, autoCalibrate); }

	bool begin(TwoWire &wirePort = Wire, bool autoCalibrate = false, bool measBegin = true); // By default use Wire port

	bool isConnected();
	void enableDebugging(Stream &debugPort = Serial); // Turn on debug printing. If user doesn't specify then Serial will be used.

	bool beginMeasuring(uint16_t pressureOffset);
	bool beginMeasuring(void);
	bool StopMeasurement(void); // paulvha

	bool setAmbientPressure(uint16_t pressure_mbar);

	bool getSettingValue(uint16_t registerAddress, uint16_t *val);
	bool getFirmwareVersion(uint16_t *val) { return (getSettingValue(0, val)); }
	uint16_t getCO2(void);
	float getHumidity(void);
	float getTemperature(void);

	uint16_t getMeasurementInterval(void);
	bool getMeasurementInterval(uint16_t *val) { return (getSettingValue(1, val)); }
	bool setMeasurementInterval(uint16_t interval);

	uint16_t getAltitudeCompensation(void);
	bool getAltitudeCompensation(uint16_t *val) { return (getSettingValue(2, val)); }
	bool setAltitudeCompensation(uint16_t altitude);

	bool getAutoSelfCalibration(void);
	bool setAutoSelfCalibration(bool enable);

	bool getForcedRecalibration(uint16_t *val) { return (getSettingValue(3, val)); }
	bool setForcedRecalibrationFactor(uint16_t concentration);

	float getTemperatureOffset(void);
	bool getTemperatureOffset(uint16_t *val) { return (getSettingValue(4, val)); }
	bool setTemperatureOffset(float tempOffset);

	bool dataAvailable();
	bool readMeasurement();

	void reset();

	bool sendCommand(uint16_t command, uint16_t arguments);
	bool sendCommand(uint16_t command);

	uint16_t readRegister(uint16_t registerAddress);

	uint8_t computeCRC8(uint8_t data[], uint8_t len);

private:
    // Global main datums
    float co2 = 0;
    float temperature = 0;
    float humidity = 0;

    // These track the staleness of the current data
    // This allows us to avoid calling readMeasurement() every time individual datums are requested
    bool co2HasBeenReported = true;
    bool humidityHasBeenReported = true;
    bool temperatureHasBeenReported = true;
};