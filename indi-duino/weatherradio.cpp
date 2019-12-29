/*
    Weather Radio - a universal driver for weather stations that
    transmit their raw sensor data as JSON documents.

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

#include "weatherradio.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>

#include <sys/stat.h>

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include "gason/gason.h"

/* Our weather station auto pointer */
std::unique_ptr<WeatherRadio> station_ptr(new WeatherRadio());

#define MAX_WEATHERBUFFER 256
#define MAX_WAIT 2

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties(const char *dev)
{
    station_ptr->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    station_ptr->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    station_ptr->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    station_ptr->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    station_ptr->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}
/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice(XMLEle *root)
{
    INDI_UNUSED(root);
}

/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool WeatherRadio::initProperties()
{
    INDI::Weather::initProperties();

    addConfigurationControl();
    addPollPeriodControl();
    addDebugControl();
    setWeatherConnection(CONNECTION_SERIAL);

    return true;
}

bool WeatherRadio::updateProperties()
{
    if (! INDI::Weather::updateProperties()) return false;
    if (isConnected())
    {
        for (size_t i = 0; i < rawSensors.size(); i++)
            defineNumber(&rawSensors[i]);
    }
    else
    {
        for (size_t i = 0; i < rawSensors.size(); i++)
            deleteProperty(rawSensors[i].name);
    }
    return true;
}

/**************************************************************************************
** Define Basic properties to clients.
***************************************************************************************/
void WeatherRadio::ISGetProperties(const char *dev)
{
    static int configLoaded = 0;

    // Ask the default driver first to send properties.
    INDI::Weather::ISGetProperties(dev);

    // If no configuration is load before, then load it now.
    if (configLoaded == 0)
    {
        loadConfig();
        configLoaded = 1;
    }
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool WeatherRadio::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n)
{
    return INDI::Weather::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::Handshake()
{
    char data[MAX_WEATHERBUFFER] = {0};
    bool result = readWeatherData(data);

    return result;
}

/**************************************************************************************
**
***************************************************************************************/
void WeatherRadio::TimerHit()
{
    if (isConnected())
    {
        char data[MAX_WEATHERBUFFER] = {0};
        readWeatherData(data);

        SetTimer(POLLMS);
    }
    return;
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::readWeatherData(char *data)
{
    int n_bytes;
    int returnCode = tty_read_section(PortFD, data, '\n', MAX_WAIT, &n_bytes);

    if (returnCode == TTY_OK)
    {
        char *srcBuffer{new char[n_bytes]};
        // duplicate the buffer since the parser will modify it
        strncpy(srcBuffer, data, static_cast<size_t>(n_bytes));
        char *source = srcBuffer;
        char *endptr;
        JsonValue value;
        JsonAllocator allocator;
        int status = jsonParse(source, &endptr, &value, allocator);
        if (status != JSON_OK)
        {
            LOGF_ERROR("Parsing error %s at %zd", jsonStrError(status), endptr - source);
            return false;
        }

        JsonIterator deviceIter;
        for (deviceIter = begin(value); deviceIter != end(value); ++deviceIter)
        {
            char *name {new char[strlen(deviceIter->key)]};
            strncpy(name, deviceIter->key, static_cast<size_t>(strlen(deviceIter->key)));

            JsonIterator sensorIter;
            INumberVectorProperty *deviceProp = findRawSensorProperty(name);

            if (deviceProp == nullptr)
            {
                // new device found
                std::vector<std::pair<char*, double>> sensorData;
                // read all sensor data
                for (sensorIter = begin(deviceIter->value); sensorIter != end(deviceIter->value); ++sensorIter)
                {
                    std::pair<char*, double> entry;
                    entry.first = sensorIter->key;
                    entry.second = sensorIter->value.toNumber();
                    sensorData.push_back(entry);
                }
                // fill the sensor data
                INumber *sensors {new INumber[sensorData.size()]};
                for (size_t i = 0; i < sensorData.size(); i++)
                {
                    IUFillNumber(&sensors[i], sensorData[i].first, sensorData[i].first, "%.2f", -2000.0, 2000.0, 1., sensorData[i].second);
                }
                // create a new number vector for the device
                deviceProp = new INumberVectorProperty;
                IUFillNumberVector(deviceProp, sensors, static_cast<int>(sensorData.size()), getDeviceName(), name, name, "Raw Sensors", IP_RO, 60, IPS_OK);
                // make it visible
                if (isConnected())
                    defineNumber(deviceProp);
                rawSensors.push_back(*deviceProp);
            }
            else {
                // read all sensor data
                for (sensorIter = begin(deviceIter->value); sensorIter != end(deviceIter->value); ++sensorIter)
                {
                    INumber *sensor = IUFindNumber(deviceProp, sensorIter->key);
                    if (sensor != nullptr)
                        sensor->value = sensorIter->value.toNumber();
                }
                // update device values
                IDSetNumber(deviceProp, nullptr);
            }

        }
        return true;
    }
    else
    {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        LOGF_WARN("Failed to receive full response: %s. (Return code: %d)", errorString, returnCode);
        return false;
    }
}

/**************************************************************************************
**
***************************************************************************************/
INumberVectorProperty *WeatherRadio::findRawSensorProperty(char *name)
{
    for (size_t i = 0; i < rawSensors.size(); i++)
    {
        if (strcmp(name, rawSensors[i].name) == 0)
            return &rawSensors[i];
    }

    // not found
    return nullptr;
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::Connect()
{
    return INDI::Weather::Connect();
}

bool WeatherRadio::Disconnect()
{
    return INDI::Weather::Disconnect();
}

const char *WeatherRadio::getDefaultName()
{
    return "Weather Radio";
}
