/**
This file is part of the AAG Cloud Watcher INDI Driver.
A driver for the AAG Cloud Watcher (AAGware - http : //www.aagware.eu/)

Copyright (C) 2012 - 2015 Sergio Alonso (zerjioi@ugr.es)
Copyright (C) 2019 Adrián Pardini - Universidad Nacional de La Plata (github@tangopardo.com.ar)

AAG Cloud Watcher INDI Driver is free software : you can redistribute it
and / or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

AAG Cloud Watcher INDI Driver is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AAG Cloud Watcher INDI Driver.  If not, see
< http : //www.gnu.org/licenses/>.

Anemometer code contributed by Joao Bento.
*/

#pragma once

#include "CloudWatcherController_ng.h"

#include "indiweather.h"
#include "connectionplugins/connectionserial.h"

enum HeatingAlgorithmStatus
{
    normal,
    increasingToPulse,
    pulse
};

class AAGCloudWatcher : public INDI::Weather
{
    public:
        AAGCloudWatcher();
        virtual ~AAGCloudWatcher() override;

        virtual bool initProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        virtual const char *getDefaultName() override;
        bool sendData();
        float getRefreshPeriod();
        float getLastReadPeriod();
        bool heatingAlgorithm();

    protected:
        virtual bool Handshake() override;
        virtual IPState updateWeather() override;

    private:
        float lastReadPeriod {0};
        CloudWatcherConstants constants;
        CloudWatcherController *cwc {nullptr};


        bool sendConstants();
        bool resetConstants();
        bool resetData();
        double getNumberValueFromVector(INumberVectorProperty *nvp, const char *name);
        bool isWetRain();

        HeatingAlgorithmStatus heatingStatus;

        time_t pulseStartTime;
        time_t wetStartTime;

        float desiredSensorTemperature;
        float globalRainSensorHeater;

        double m_FirmwareVersion {5};

        enum
        {
            RAW_SENSOR_SUPPLY,
            RAW_SENSOR_SKY,
            RAW_SENSOR_SENSOR,
            RAW_SENSOR_AMBIENT,
            RAW_SENSOR_RAIN,
            RAW_SENSOR_RAIN_HEATER,
            RAW_SENSOR_RAIN_TEMPERATURE,
            RAW_SENSOR_LDR,
            RAW_SENSOR_READ_CYCLES,
            RAW_SENSOR_WIND_SPEED,
            RAW_SENSOR_RELATIVE_HUMIDITY,
            RAW_SENSOR_PRESSURE,
            RAW_SENSOR_TOTAL_READINGS
        };

         enum
        {
            SENSOR_INFRARED_SKY, //skyTemperature
            SENSOR_CORRECTED_INFRARED_SKY, //correctedTemperature
            SENSOR_INFRARED_SENSOR,
            SENSOR_RAIN_SENSOR, //data.sensor
            SENSOR_RAIN_SENSOR_TEMPERATURE, //rainSensorTemperature
            SENSOR_RAIN_SENSOR_HEATER, //rainSensorHeater
            SENSOR_BRIGHTNESS_SENSOR, //ambientLight
            SENSOR_AMBIENT_TEMPERATURE_SENSOR, //ambientTemperature
            SENSOR_WIND_SPEED, //data.windSpeed;
            SENSOR_HUMIDITY, //data.humidity;
            SENSOR_PRESSURE //data.pressure;
        };

};
