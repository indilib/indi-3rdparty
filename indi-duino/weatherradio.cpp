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

    // define INDI config values for known sensors
    deviceConfig["BME280"]["Temp"]    = {"Temperature (°C)", TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["BME280"]["Pres"]    = {"Pressure (hPa)", PRESSURE_SENSOR, "%.1f", 500., 1100.0, 1.0};
    deviceConfig["BME280"]["Hum"]    = {"Humidity (%)", HUMIDITY_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["MLX90614"]["T amb"] = {"Ambient Temp. (°C)", TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["MLX90614"]["T obj"] = {"Sky Temp. (°C)", OBJECT_TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["TSL2591"]["Lux"] = {"Luminance (Lux)", LUMINOSITY_SENSOR, "%.1f", 0.0, 1000.0, 1.0};

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool WeatherRadio::updateProperties()
{
    if (! INDI::Weather::updateProperties()) return false;
    if (isConnected())
    {
        for (size_t i = 0; i < rawSensors.size(); i++)
            defineNumber(&rawSensors[i]);

        addWeatherProperty(&temperatureSensorSP, sensorRegistry.temperature, "TEMPERATURE_SENSOR", "Temperature Sensor");
        addWeatherProperty(&pressureSensorSP, sensorRegistry.pressure, "PRESSURE_SENSOR", "Pressure Sensor");
        addWeatherProperty(&humiditySensorSP, sensorRegistry.humidity, "HUMIDITY_SENSOR", "Humidity Sensor");
        addWeatherProperty(&luminositySensorSP, sensorRegistry.luminosity, "LUMINOSITY_SENSOR", "Luminosity Sensor");
        addWeatherProperty(&ambientTemperatureSensorSP, sensorRegistry.temperature, "AMBIENT_TEMP_SENSOR", "Ambient Temp. Sensor");
        addWeatherProperty(&objectTemperatureSensorSP, sensorRegistry.temp_object, "OBJECT_TEMP_SENSOR", "Object Temp. Sensor");
    }
    else
    {
        for (size_t i = 0; i < rawSensors.size(); i++)
            deleteProperty(rawSensors[i].name);

        deleteProperty(temperatureSensorSP.name);
        deleteProperty(pressureSensorSP.name);
        deleteProperty(humiditySensorSP.name);
        deleteProperty(luminositySensorSP.name);
        deleteProperty(ambientTemperatureSensorSP.name);
        deleteProperty(objectTemperatureSensorSP.name);
    }
    return INDI::Weather::updateProperties();
}

/**************************************************************************************
**
***************************************************************************************/
void WeatherRadio::addWeatherProperty(ISwitchVectorProperty *sensor, std::vector<sensor_name> sensors, const char* name, const char* label)
{
    ISwitch *switches {new ISwitch[sensors.size()]};
    for (size_t i = 0; i < sensors.size(); i++)
    {
        std::string name = canonicalName(sensors[i]).c_str();
        IUFillSwitch(&switches[i], name.c_str(), name.c_str(), ISS_OFF);
    }
    IUFillSwitchVector(sensor, switches, static_cast<int>(sensors.size()), getDeviceName(), name, label, "Options", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineSwitch(sensor);
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
WeatherRadio::sensor_name WeatherRadio::updateSensorConfig(ISwitchVectorProperty *weatherParameter, const char *selected)
{
    sensor_name sensor;
    if (selected != nullptr && parseCanonicalName(&sensor, selected))
    {
        weatherParameter->s = IPS_OK;
    }
    else
        weatherParameter->s = IPS_IDLE;

    IDSetSwitch(weatherParameter, nullptr);
    return sensor;
}

bool WeatherRadio::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (strcmp(name, temperatureSensorSP.name) == 0)
        {
            // temperature sensor selected
            IUUpdateSwitch(&temperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorConfig(&temperatureSensorSP, selected);
            currentSensors.temperature = sensor;

            return (temperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, pressureSensorSP.name) == 0)
        {
            // pressure sensor selected
            IUUpdateSwitch(&pressureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorConfig(&pressureSensorSP, selected);
            currentSensors.pressure = sensor;

            return (pressureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, humiditySensorSP.name) == 0)
        {
            // humidity sensor selected
            IUUpdateSwitch(&humiditySensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorConfig(&humiditySensorSP, selected);
            currentSensors.humidity = sensor;

            return (humiditySensorSP.s == IPS_OK);
        }
        else if (strcmp(name, luminositySensorSP.name) == 0)
        {
            // luminosity sensor selected
            IUUpdateSwitch(&luminositySensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorConfig(&luminositySensorSP, selected);
            currentSensors.luminosity = sensor;

            return (luminositySensorSP.s == IPS_OK);
        }
        else if (strcmp(name, ambientTemperatureSensorSP.name) == 0)
        {
            // ambient temperature sensor selected
            IUUpdateSwitch(&ambientTemperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorConfig(&ambientTemperatureSensorSP, selected);
            currentSensors.temp_ambient = sensor;

            return (ambientTemperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, objectTemperatureSensorSP.name) == 0)
        {
            // object temperature sensor selected
            IUUpdateSwitch(&objectTemperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorConfig(&objectTemperatureSensorSP, selected);
            currentSensors.temp_object = sensor;

            return (objectTemperatureSensorSP.s == IPS_OK);
        }
    }
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

    return loadConfig() || result;
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
        char *srcBuffer{new char[n_bytes+1] {0}};
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
            char *name {new char[strlen(deviceIter->key)+1] {0}};
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
                    sensorsConfigType devConfig = deviceConfig[name];
                    if (devConfig.count(sensorData[i].first) > 0)
                    {
                        sensor_name sensor = {name, sensorData[i].first};
                        sensor_config config = devConfig[sensorData[i].first];
                        IUFillNumber(&sensors[i], sensor.sensor.c_str(), config.label.c_str(), config.format.c_str(), config.min, config.max, config.steps, sensorData[i].second);
                        registerSensor(sensor, config.type);
                    }
                    else
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
                    if (sensor != nullptr && sensorIter->value.isDouble())
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
void WeatherRadio::registerSensor(WeatherRadio::sensor_name sensor, SENSOR_TYPE type)
{
    switch (type) {
    case TEMPERATURE_SENSOR:
        sensorRegistry.temperature.push_back(sensor);
        break;
    case PRESSURE_SENSOR:
        sensorRegistry.pressure.push_back(sensor);
        break;
    case HUMIDITY_SENSOR:
        sensorRegistry.humidity.push_back(sensor);
        break;
    case LUMINOSITY_SENSOR:
        sensorRegistry.luminosity.push_back(sensor);
        break;
    case OBJECT_TEMPERATURE_SENSOR:
        sensorRegistry.temp_object.push_back(sensor);
        break;
    }
}


/*********************************************************************************
 * config file
 *********************************************************************************/

bool WeatherRadio::saveConfigItems(FILE *fp)
{
    LOG_DEBUG(__FUNCTION__);
    IUSaveConfigSwitch(fp, &temperatureSensorSP);
    IUSaveConfigSwitch(fp, &pressureSensorSP);
    IUSaveConfigSwitch(fp, &humiditySensorSP);
    IUSaveConfigSwitch(fp, &luminositySensorSP);
    IUSaveConfigSwitch(fp, &ambientTemperatureSensorSP);
    IUSaveConfigSwitch(fp, &objectTemperatureSensorSP);

    return INDI::Weather::saveConfigItems(fp);
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
bool WeatherRadio::parseCanonicalName(sensor_name *sensor, std::string name)
{
    // find " (" and ")"
    size_t found_open = name.find(" (");
    size_t found_close = name.find(")");
    if (found_open == std::string::npos || found_close == std::string::npos)
        return false;

    // OK, limiters found
    sensor->device = name.substr(0, found_open);
    sensor->sensor = name.substr(found_open+2, found_close-found_open-2);
    return true;
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
