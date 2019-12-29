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

#include "indiweather.h"

class WeatherRadio : public INDI::Weather
{
  public:
    WeatherRadio() = default;
    ~WeatherRadio() = default;

    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n);

    /** \brief perform handshake with device to check communication */
    virtual bool Handshake();

    /** \brief Called when setTimer() time is up */
    virtual void TimerHit();

protected:
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    bool readWeatherData(char *data);

    /** \brief find the matching raw sensor INDI property vector */
    INumberVectorProperty *findRawSensorProperty(char *name);
    std::vector<INumberVectorProperty> rawSensors;

private:
    const char *getDefaultName();
    virtual bool Connect();
    virtual bool Disconnect();
};
