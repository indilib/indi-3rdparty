/*
    Weather Radio - a universal driver for weather stations that
    transmit their sensor data as JSON documents.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include <map>
#include <math.h>
#include <memory>

#include "indiweather.h"
#include "weathercalculator.h"

extern const char *CALIBRATION_TAB;
extern const char *TOKEN;

class WeatherRadio : public INDI::Weather
{
  public:
    WeatherRadio();
    ~WeatherRadio() override = default;

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;

    /** \brief perform handshake with device to check communication */
    virtual bool Handshake() override;

protected:
    WeatherCalculator *weatherCalculator;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    // Initial function to get data after connection is successful
    IPState getBasicData();

    // host name and port for HTTP connections
    char hostname[MAXINDILABEL];
    char port[MAXINDILABEL];

    // Read the firmware configuration
    void updateConfigData();

    ISwitchVectorProperty temperatureSensorSP, ambientTemperatureSensorSP, objectTemperatureSensorSP, pressureSensorSP,
        humiditySensorSP, luminositySensorSP, sqmSensorSP, windSpeedSensorSP, windGustSensorSP, windDirectionSensorSP,
        rainIntensitySensorSP, rainVolumeSensorSP, wetnessSensorSP;

    /**
     * @brief get the interface version from the Arduino device.
     */
    IPState getFirmwareVersion(char *versionInfo);
    // firmware info
    ITextVectorProperty FirmwareInfoTP;
    IText FirmwareInfoT[1] = {};
    // firmware configuration (dynamically created)
    IText *FirmwareConfigT;
    ITextVectorProperty FirmwareConfigTP;

    /**
     * @brief Read the weather data from the JSON document
     * @return parse success
     */
    IPState updateWeather() override;

    bool parseWeatherData(char *data);

    /**
      * Device specific configurations
      */
    enum SENSOR_TYPE {TEMPERATURE_SENSOR, OBJECT_TEMPERATURE_SENSOR, PRESSURE_SENSOR, HUMIDITY_SENSOR,
                      LUMINOSITY_SENSOR, SQM_SENSOR, WIND_SPEED_SENSOR, WIND_GUST_SENSOR, WIND_DIRECTION_SENSOR,
                      RAIN_INTENSITY_SENSOR, RAIN_VOLUME_SENSOR, WETNESS_SENSOR, INTERNAL_SENSOR};

    struct sensor_config
    {
        std::string label;
        SENSOR_TYPE type;
        std::string format;
        double min;
        double max;
        double steps;
    };

    typedef std::map<std::string, std::string> FirmwareConfig;

    typedef std::map<std::string, sensor_config> sensorsConfigType;
    typedef std::map<std::string, sensorsConfigType> deviceConfigType;

    deviceConfigType deviceConfig;

    /**
     * Sensor type specifics
     */
    struct sensor_name
    {
        std::string device;
        std::string sensor;

        inline bool operator==(sensor_name other)
        {
            return (other.device == device) && (other.sensor == sensor);
        }
    };

    std::vector<INumberVectorProperty> rawDevices;
    /**
     * \brief Find the matching raw device INDI property vector.
    */
    INumberVectorProperty *findRawDeviceProperty(const char *name);
    /**
     * @brief find the matching sensor INDI property
     */
    INumber *findRawSensorProperty(const sensor_name sensor);

    /**
     * @brief Find a given weather parameter.
     */
    INumber *getWeatherParameter(std::string name);

    /**
     * @brief TTY interface timeout
     */
    int getTTYTimeout() { return ttyTimeout; }
    // HINT: TSL2591 needs at night more than 1 sec for measuring, hence
    //       we set the TTY timeout to 5sec
    int ttyTimeout = 5;
    INumber ttyTimeoutN[1] = {};
    INumberVectorProperty ttyTimeoutNP;

    ISwitch refreshConfigS[1] = {};
    ISwitchVectorProperty refreshConfigSP;

    ISwitch wifiConnectionS[2] = {};
    ISwitchVectorProperty wifiConnectionSP;
    bool hasWiFi = false;

    ISwitch resetArduinoS[1] = {};
    ISwitchVectorProperty resetArduinoSP;

    // calibration parameters to calculate the corrected sky temperature
    INumberVectorProperty skyTemperatureCalibrationNP;
    INumber skyTemperatureCalibrationN[7];

    // calibration parameters for the humidity
    INumberVectorProperty humidityCalibrationNP;
    INumber humidityCalibrationN[2];

    // calibration parameters for the temperature
    INumberVectorProperty temperatureCalibrationNP;
    INumber temperatureCalibrationN[2];

    // calibration for wind direction
    INumberVectorProperty windDirectionCalibrationNP;
    INumber windDirectionCalibrationN[1];

    // calibration for SQM measurement
    INumberVectorProperty sqmCalibrationNP;
    INumber sqmCalibrationN[2];

    // calibration for wetness measurement
    INumberVectorProperty wetnessCalibrationNP;
    INumber wetnessCalibrationN[2];

    /**
     * @brief Create a canonical name as <device> (<sensor>)
     * @param sensor weather sensor
     */
    std::string canonicalName(sensor_name sensor) {return sensor.device + " (" + sensor.sensor + ")";}

    /**
     * @brief Inverse function of canonicalName(..)
     */
    bool parseCanonicalName(sensor_name *sensor, std::string name);

    struct
    {
        sensor_name temperature;
        sensor_name pressure;
        sensor_name humidity;
        sensor_name luminosity;
        sensor_name sqm;
        sensor_name temp_ambient;
        sensor_name temp_object;
        sensor_name wind_speed;
        sensor_name wind_gust;
        sensor_name wind_direction;
        sensor_name rain_intensity;
        sensor_name rain_volume;
        sensor_name wetness;
    } currentSensors;

    struct
    {
        std::vector<sensor_name> temperature;
        std::vector<sensor_name> pressure;
        std::vector<sensor_name> humidity;
        std::vector<sensor_name> luminosity;
        std::vector<sensor_name> sqm;
        std::vector<sensor_name> temp_object;
        std::vector<sensor_name> wind_speed;
        std::vector<sensor_name> wind_gust;
        std::vector<sensor_name> wind_direction;
        std::vector<sensor_name> rain_intensity;
        std::vector<sensor_name> rain_volume;
        std::vector<sensor_name> wetness;
    } sensorRegistry;

    /**
     * @brief Add a sensor to the sensor registry
     * @param sensor device name and sensor name
     * @param type sensor type
     */
    void registerSensor(sensor_name sensor, SENSOR_TYPE type);

    /**
     * @brief Add a INDI switch property for a specific weather parameter to the Settings panel
     * @param sensor property vector to be filled
     * @param sensors vector of sensor names that record the same weather parameter (temperature, pressure, etc.)
     * @param name name of the INDI property
     * @param label label of the INDI property
     */
    void addSensorSelection(ISwitchVectorProperty *sensor, std::vector<sensor_name> sensors, const char *name, const char *label);

    /**
     * @brief Update the selected sensor indicating a weather property
     */
    sensor_name updateSensorSelection(ISwitchVectorProperty *weatherParameter, const char *selected);

    /**
     * @brief Select the weather parameter from the sensor registry and update it.
     */
    void updateWeatherParameter(sensor_name sensor, double value);

    /**
     * @brief Read the firmware configuration
     * @param config configuration to be updated
     */
    IPState readFirmwareConfig(FirmwareConfig *config);

    /**
     * @brief Connect to WiFi
     * @param connect true iff connect, false iff disconnect
     */
    bool connectWiFi(bool connect);

    /**
     * @brief updateWiFiStatus
     * @param connected true iff WiFi is connected
     */
    void updateWiFiStatus(bool connected);

    /**
     * @brief Send the Arduino a reset command
     * @return true iff successful
     */
    bool resetArduino();

    // Serial communication
    bool receiveSerial(char* buffer, int* bytes, char end, int wait);
    bool transmitSerial(const char* buffer);
    bool sendQuery(const char* cmd, char* response, int *length);

    // override default INDI methods
    const char *getDefaultName() override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool saveConfigItems(FILE *fp) override;

};

extern std::unique_ptr<WeatherRadio> station_ptr;
