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
    WeatherRadio() = default;
    ~WeatherRadio() = default;

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;

    /** \brief perform handshake with device to check communication */
    virtual bool Handshake() override;

    /** \brief Called when setTimer() time is up */
    virtual void TimerHit() override;

protected:
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    ISwitchVectorProperty temperatureSensorSP, ambientTemperatureSensorSP, objectTemperatureSensorSP, pressureSensorSP, humiditySensorSP, luminositySensorSP;

    bool readWeatherData(char *data);

    /** \brief find the matching raw sensor INDI property vector */
    INumberVectorProperty *findRawSensorProperty(char *name);
    std::vector<INumberVectorProperty> rawSensors;

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
    };

    std::string canonicalName(sensor_name sensor) {return sensor.device + " - " + sensor.sensor;}

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
    } allSensors;

    void registerSensor(sensor_name sensor, SENSOR_TYPE type);
    void addWeatherProperty(ISwitchVectorProperty *sensor, std::vector<sensor_name> sensors, const char *name, const char *label);

    const char *getDefaultName() override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
};
