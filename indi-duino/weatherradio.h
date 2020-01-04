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

#include "indiweather.h"

class WeatherRadio : public INDI::Weather
{
  public:
    WeatherRadio();
    ~WeatherRadio() = default;

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;

    /** \brief perform handshake with device to check communication */
    virtual bool Handshake() override;

protected:
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    ISwitchVectorProperty temperatureSensorSP, ambientTemperatureSensorSP, objectTemperatureSensorSP, pressureSensorSP, humiditySensorSP, luminositySensorSP;

    /**
     * @brief Read the weather data from the JSON document
     * @return parse success
     */
    IPState updateWeather() override;

    /**
     * @brief Calculate the cloud coverage from the ambient and cloud temperature.
     */
    double cloudCoverage(double ambientTemperature, double skyTemperature);

    /**
     * @brief Calculate the Sky quality SQM from the measured illuminance.
     */
    double sqmValue(double lux);

    /**
      * Device specific configurations
      */
    enum SENSOR_TYPE {TEMPERATURE_SENSOR, OBJECT_TEMPERATURE_SENSOR, PRESSURE_SENSOR, HUMIDITY_SENSOR, LUMINOSITY_SENSOR};

    struct sensor_config
    {
        std::string label;
        SENSOR_TYPE type;
        std::string format;
        double min;
        double max;
        double steps;
    };

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
        sensor_name temp_ambient;
        sensor_name temp_object;
    } currentSensors;

    struct
    {
        std::vector<sensor_name> temperature;
        std::vector<sensor_name> pressure;
        std::vector<sensor_name> humidity;
        std::vector<sensor_name> luminosity;
        std::vector<sensor_name> temp_object;
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
     * @brief Send a string to the serial device
     */
    // helper functions
    bool receive(char* buffer, int* bytes, char end, int wait);
    bool transmit(const char* buffer);
    bool sendQuery(const char* cmd, char* response, int *length);

    // override default INDI methods
    const char *getDefaultName() override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool saveConfigItems(FILE *fp) override;

};
