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

    Cloud coverage calculation taken from the INDI driver for the AAG Cloud Watcher
    (Lunatico https://www.lunatico.es), implemented by Sergio Alonso (zerjioi@ugr.es)
*/

#include "weatherradio.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>
#include <math.h>

#include <sys/stat.h>

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include "gason/gason.h"

#include "config.h"

/* Our weather station auto pointer */
std::unique_ptr<WeatherRadio> station_ptr(new WeatherRadio());

#define MAX_WEATHERBUFFER 256
#define MAX_WAIT 2

#define WEATHER_TEMPERATURE "WEATHER_TEMPERATURE"
#define WEATHER_PRESSURE    "WEATHER_PRESSURE"
#define WEATHER_HUMIDITY    "WEATHER_HUMIDITY"
#define WEATHER_CLOUD_COVER "WEATHER_CLOUD_COVER"
#define WEATHER_SQM         "WEATHER_SQM"
#define WEATHER_DEWPOINT    "WEATHER_DEWPOINT"

//Clear sky corrected temperature (temp below means 0% clouds)
#define CLOUD_TEMP_CLEAR  -8
//Totally cover sky corrected temperature (temp above means 100% clouds)
#define CLOUD_TEMP_OVERCAST  0


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
** Constructor
***************************************************************************************/
WeatherRadio::WeatherRadio()
{
    setVersion(WEATHERRADIO_VERSION_MAJOR, WEATHERRADIO_VERSION_MINOR);
}

/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool WeatherRadio::initProperties()
{
    INDI::Weather::initProperties();

    addConfigurationControl();

    IUFillText(&FirmwareInfoT[0], "FIRMWARE_INFO", "Firmware Version", "<unknown version>");
    IUFillTextVector(&FirmwareInfoTP, FirmwareInfoT, 1, getDeviceName(), "FIRMWARE", "Firmware", INFO_TAB, IP_RO, 60, IPS_OK);

    addParameter(WEATHER_TEMPERATURE, "Temperature (°C)", -10, 30, 15);
    addParameter(WEATHER_PRESSURE, "Pressure (hPa)", 950, 1070, 15);
    addParameter(WEATHER_HUMIDITY, "Humidity (%)", 0, 100, 15);
    addParameter(WEATHER_CLOUD_COVER, "Clouds (%)", 0, 100, 50);
    addParameter(WEATHER_SQM, "SQM", 10, 30, 15);
    addParameter(WEATHER_DEWPOINT, "Dewpoint (°C)", -10, 30, 15);

    setCriticalParameter(WEATHER_TEMPERATURE);
    setCriticalParameter(WEATHER_PRESSURE);
    setCriticalParameter(WEATHER_HUMIDITY);
    setCriticalParameter(WEATHER_CLOUD_COVER);
    setCriticalParameter(WEATHER_SQM);

    addDebugControl();
    setWeatherConnection(CONNECTION_SERIAL);

    // define INDI config values for known sensors
    deviceConfig["BME280"]["Temp"]     = {"Temperature (°C)", TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["BME280"]["Pres"]     = {"Pressure (hPa)", PRESSURE_SENSOR, "%.1f", 500., 1100.0, 1.0};
    deviceConfig["BME280"]["Hum"]      = {"Humidity (%)", HUMIDITY_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["DHT"]["Temp"]        = {"Temperature (°C)", TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["DHT"]["Hum"]         = {"Humidity (%)", HUMIDITY_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["MLX90614"]["T amb"]  = {"Ambient Temp. (°C)", TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["MLX90614"]["T obj"]  = {"Sky Temp. (°C)", OBJECT_TEMPERATURE_SENSOR, "%.2f", -100.0, 100.0, 1.0};
    deviceConfig["TSL2591"]["Lux"]     = {"Luminance (Lux)", LUMINOSITY_SENSOR, "%.1f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["Visible"] = {"Lightness (Vis)", INTERNAL_SENSOR, "%.1f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["IR"]      = {"Lightness (IR)", INTERNAL_SENSOR, "%.1f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["Gain"]    = {"Gain", INTERNAL_SENSOR, "%.0f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["Timing"]  = {"Timing", INTERNAL_SENSOR, "%.0f", 0.0, 1000.0, 1.0};

    return true;
}

/**************************************************************************************
** Update the INDI properties as a reaction on connect or disconnect.
***************************************************************************************/
bool WeatherRadio::updateProperties()
{
    if (! INDI::Weather::updateProperties()) return false;
    if (isConnected())
    {

        for (size_t i = 0; i < rawDevices.size(); i++)
            defineNumber(&rawDevices[i]);

        addSensorSelection(&temperatureSensorSP, sensorRegistry.temperature, "TEMPERATURE_SENSOR", "Temperature Sensor");
        addSensorSelection(&pressureSensorSP, sensorRegistry.pressure, "PRESSURE_SENSOR", "Pressure Sensor");
        addSensorSelection(&humiditySensorSP, sensorRegistry.humidity, "HUMIDITY_SENSOR", "Humidity Sensor");
        addSensorSelection(&luminositySensorSP, sensorRegistry.luminosity, "LUMINOSITY_SENSOR", "Luminosity Sensor");
        addSensorSelection(&ambientTemperatureSensorSP, sensorRegistry.temperature, "AMBIENT_TEMP_SENSOR", "Ambient Temp. Sensor");
        addSensorSelection(&objectTemperatureSensorSP, sensorRegistry.temp_object, "OBJECT_TEMP_SENSOR", "Object Temp. Sensor");

        getBasicData();
    }
    else
    {
        for (size_t i = 0; i < rawDevices.size(); i++)
            deleteProperty(rawDevices[i].name);

        deleteProperty(temperatureSensorSP.name);
        deleteProperty(pressureSensorSP.name);
        deleteProperty(humiditySensorSP.name);
        deleteProperty(luminositySensorSP.name);
        deleteProperty(ambientTemperatureSensorSP.name);
        deleteProperty(objectTemperatureSensorSP.name);
        deleteProperty(FirmwareInfoTP.name);
    }

    // Load the configuration.
    loadConfig();

    return true;
}

/**************************************************************************************
** Retrieve basic data after a successful connect.
***************************************************************************************/
void WeatherRadio::getBasicData()
{
    FirmwareInfoT[0].text = new char[64];
    FirmwareInfoTP.s = getFirmwareVersion(FirmwareInfoT[0].text);
    if (FirmwareInfoTP.s != IPS_OK)
        LOG_ERROR("Failed to get firmware from device.");

    defineText(&FirmwareInfoTP);
    IDSetText(&FirmwareInfoTP, nullptr);
}

/**************************************************************************************
** Version of the Arduino firmware
***************************************************************************************/
IPState WeatherRadio::getFirmwareVersion(char *versionInfo)
{
    char data[MAX_WEATHERBUFFER] = {0};
    int n_bytes = 0;
    bool result = sendQuery("v", data, &n_bytes);

    if (result == true)
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
            return IPS_ALERT;
        }

        JsonIterator docIter;
        for (docIter = begin(value); docIter != end(value); ++docIter)
        {
            if (strcmp(docIter->key, "version") == 0)
                strcpy(versionInfo, docIter->value.toString());
        }
        return IPS_OK;
    }
    return IPS_ALERT;
}

/**************************************************************************************
** Create a selection of sensors for a certain weather property.
***************************************************************************************/
void WeatherRadio::addSensorSelection(ISwitchVectorProperty *sensor, std::vector<sensor_name> sensors, const char* name, const char* label)
{
    ISwitch *switches {new ISwitch[sensors.size()]};
    for (size_t i = 0; i < sensors.size(); i++)
    {
        std::string name = canonicalName(sensors[i]).c_str();
        IUFillSwitch(&switches[i], name.c_str(), name.c_str(), ISS_OFF);
    }
    IUFillSwitchVector(sensor, switches, static_cast<int>(sensors.size()), getDeviceName(), name, label, OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineSwitch(sensor);
}

/**************************************************************************************
** Define Basic properties to clients.
***************************************************************************************/

void WeatherRadio::ISGetProperties(const char *dev)
{

    // Ask the default driver first to send properties.
    INDI::Weather::ISGetProperties(dev);
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool WeatherRadio::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process Number properties
***************************************************************************************/
bool WeatherRadio::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Process Switch properties
***************************************************************************************/
bool WeatherRadio::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (strcmp(name, temperatureSensorSP.name) == 0)
        {
            // temperature sensor selected
            IUUpdateSwitch(&temperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&temperatureSensorSP, selected);
            currentSensors.temperature = sensor;

            return (temperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, pressureSensorSP.name) == 0)
        {
            // pressure sensor selected
            IUUpdateSwitch(&pressureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&pressureSensorSP, selected);
            currentSensors.pressure = sensor;

            return (pressureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, humiditySensorSP.name) == 0)
        {
            // humidity sensor selected
            IUUpdateSwitch(&humiditySensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&humiditySensorSP, selected);
            currentSensors.humidity = sensor;

            return (humiditySensorSP.s == IPS_OK);
        }
        else if (strcmp(name, luminositySensorSP.name) == 0)
        {
            // luminosity sensor selected
            IUUpdateSwitch(&luminositySensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&luminositySensorSP, selected);
            currentSensors.luminosity = sensor;

            return (luminositySensorSP.s == IPS_OK);
        }
        else if (strcmp(name, ambientTemperatureSensorSP.name) == 0)
        {
            // ambient temperature sensor selected
            IUUpdateSwitch(&ambientTemperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&ambientTemperatureSensorSP, selected);
            currentSensors.temp_ambient = sensor;

            return (ambientTemperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, objectTemperatureSensorSP.name) == 0)
        {
            // object temperature sensor selected
            IUUpdateSwitch(&objectTemperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&objectTemperatureSensorSP, selected);
            currentSensors.temp_object = sensor;

            return (objectTemperatureSensorSP.s == IPS_OK);
        }
    }
    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Manage BLOBs.
***************************************************************************************/
bool WeatherRadio::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n)
{
    return INDI::Weather::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Initialization when the driver gets connected.
***************************************************************************************/
bool WeatherRadio::Handshake()
{
    IPState result = updateWeather();

    return result == IPS_OK;
}


/**************************************************************************************
** Read all weather sensor values.
***************************************************************************************/
IPState WeatherRadio::updateWeather()
{
    char data[MAX_WEATHERBUFFER] = {0};
    int n_bytes = 0;
    bool result = sendQuery("w", data, &n_bytes);

    if (result == true)
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
            return IPS_ALERT;
        }

        JsonIterator deviceIter;
        for (deviceIter = begin(value); deviceIter != end(value); ++deviceIter)
        {
            char *name {new char[strlen(deviceIter->key)+1] {0}};
            strncpy(name, deviceIter->key, static_cast<size_t>(strlen(deviceIter->key)));

            JsonIterator sensorIter;
            INumberVectorProperty *deviceProp = findRawDeviceProperty(name);

            if (deviceProp == nullptr)
            {
                // new device found
                std::vector<std::pair<char*, double>> sensorData;
                // read all sensor data
                bool initialized = false;
                for (sensorIter = begin(deviceIter->value); sensorIter != end(deviceIter->value); ++sensorIter)
                {
                    std::pair<char*, double> entry;
                    entry.first = sensorIter->key;

                    // special case: get the information whether the device has been initialized
                    if (strcmp(entry.first, "init") == 0)
                        initialized = (sensorIter->value.getTag() == JSON_TRUE);

                    if (sensorIter->value.isDouble())
                    {
                        entry.second = sensorIter->value.toNumber();
                        sensorData.push_back(entry);
                    }
                }
                if (initialized)
                {
                    // fill the sensor data if the sensor has been initialized
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
                    rawDevices.push_back(*deviceProp);
                }
            }
            else {
                // read all sensor data
                for (sensorIter = begin(deviceIter->value); sensorIter != end(deviceIter->value); ++sensorIter)
                {
                    if (strcmp(sensorIter->key, "init") == 0 || sensorIter->value.getTag() != JSON_NUMBER)
                        continue;
                    INumber *sensor = IUFindNumber(deviceProp, sensorIter->key);
                    if (sensor != nullptr && sensorIter->value.isDouble())
                    {
                        sensor->value = sensorIter->value.toNumber();
                        // update the weather parameter {name, sensorIter->key} to sensorIter->value.toNumber()
                        updateWeatherParameter({name, sensorIter->key}, sensorIter->value.toNumber());
                    }
                }
                // update device values
                IDSetNumber(deviceProp, nullptr);
            }

        }
        return IPS_OK;
    }
    else
        return IPS_ALERT;
}

/**************************************************************************************
** Sensor selection changed
***************************************************************************************/
WeatherRadio::sensor_name WeatherRadio::updateSensorSelection(ISwitchVectorProperty *weatherParameter, const char *selected)
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

/**************************************************************************************
** Update the value of the WEATHER_... from its sensor value
***************************************************************************************/
void WeatherRadio::updateWeatherParameter(WeatherRadio::sensor_name sensor, double value)
{
    if (currentSensors.temperature == sensor)
        setParameterValue(WEATHER_TEMPERATURE, value);
    else if (currentSensors.pressure == sensor)
    {
        double elevation = LocationN[LOCATION_ELEVATION].value;

        const double temp_gradient = 0.0065;
        double temp = 15.0;
        INumber *temperatureParameter = getWeatherParameter(WEATHER_TEMPERATURE);
        if (temperatureParameter != nullptr)
            temp = temperatureParameter->value;
        else
            temp = 15.0; // default value

        // barometric height formula
        double pressure_normalized = value / pow(1-temp_gradient*elevation/(temp+elevation*temp_gradient+273.15), 0.03416/temp_gradient);

        setParameterValue(WEATHER_PRESSURE, pressure_normalized);
    }
    else if (currentSensors.humidity == sensor)
    {
        setParameterValue(WEATHER_HUMIDITY, value);
        INumber *temperatureParameter = getWeatherParameter(WEATHER_TEMPERATURE);
        if (temperatureParameter != nullptr)
        {
            double dp =  dewPoint(value, temperatureParameter->value);
            setParameterValue(WEATHER_DEWPOINT, dp);
        }
     }
    else if (currentSensors.temp_ambient == sensor)
    {
        // obtain the current object temperature
        INumber *objProp = findRawSensorProperty(currentSensors.temp_object);
        if (objProp != nullptr)
        {
            double objectTemperature = objProp->value;
            setParameterValue(WEATHER_CLOUD_COVER, cloudCoverage(value, objectTemperature));
        }
    }
    else if (currentSensors.temp_object == sensor)
    {
        // obtain the current ambient temperature
        INumber *ambientProp = findRawSensorProperty(currentSensors.temp_ambient);
        if (ambientProp != nullptr)
        {
            double ambientTemperature = ambientProp->value;
            setParameterValue(WEATHER_CLOUD_COVER, cloudCoverage(ambientTemperature, value));
        }
    }
    else if (currentSensors.luminosity == sensor)
        setParameterValue(WEATHER_SQM, sqmValue(value));
}

/**************************************************************************************
** Calculate the cloud coverage from the difference between ambient and sky temperature
** The original formula stems from the AAG cloud watcher http://lunatico.es/aagcw/enhelp/
** and the INDI driver indi_aagcloudwatcher.cpp.
***************************************************************************************/
double WeatherRadio::cloudCoverage(double ambientTemperature, double skyTemperature)
{
    // FIXME: make the parameters k1 .. k5 configurable
    double k1 = 33.0, k2 = 0.0,  k3 = 4.0, k4 = 100.0, k5 = 100.0;

    double correctedTemperature =
        skyTemperature - ((k1 / 100.0) * (ambientTemperature - k2 / 10.0) +
                          (k3 / 100.0) * pow(exp(k4 / 1000. * ambientTemperature), (k5 / 100.0)));


    if (correctedTemperature < CLOUD_TEMP_CLEAR) correctedTemperature = CLOUD_TEMP_CLEAR;
    if (correctedTemperature > CLOUD_TEMP_OVERCAST) correctedTemperature = CLOUD_TEMP_OVERCAST;

    return (correctedTemperature - CLOUD_TEMP_CLEAR) * 100 / (CLOUD_TEMP_OVERCAST - CLOUD_TEMP_CLEAR);
}

/**************************************************************************************
**
***************************************************************************************/
double WeatherRadio::sqmValue(double lux)
{
    double mag_arcsec2 = log10(lux/108000)/-0.4;
    return mag_arcsec2;
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
    case INTERNAL_SENSOR:
        // do nothing
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
    IUSaveConfigNumber(fp, ParametersRangeNP);

    return INDI::Weather::saveConfigItems(fp);
}

/**************************************************************************************
** access to device and sensor INDI properties
***************************************************************************************/
INumberVectorProperty *WeatherRadio::findRawDeviceProperty(const char *name)
{
    for (size_t i = 0; i < rawDevices.size(); i++)
    {
        if (strcmp(name, rawDevices[i].name) == 0)
            return &rawDevices[i];
    }

    // not found
    return nullptr;
}

INumber *WeatherRadio::findRawSensorProperty(WeatherRadio::sensor_name sensor)
{
    INumberVectorProperty *deviceProp = findRawDeviceProperty(sensor.device.c_str());

    INumber *sensorProp = nullptr;
    if (deviceProp != nullptr)
        sensorProp = IUFindNumber(deviceProp, sensor.sensor.c_str());

    return sensorProp;
}

INumber *WeatherRadio::getWeatherParameter(std::string name)
{
    for (int i = 0; i < ParametersNP.nnp; i++)
    {
        if (!strcmp(ParametersN[i].name, name.c_str()))
            return &ParametersN[i];
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
** Helper functions for serial communication
***************************************************************************************/
bool WeatherRadio::receive(char* buffer, int* bytes, char end, int wait)
{
    int timeout = wait;
    int returnCode = tty_read_section(PortFD, buffer, end, timeout, bytes);
    if (returnCode != TTY_OK)
    {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        if(returnCode == TTY_TIME_OUT && wait <= 0) return false;
        LOGF_WARN("Failed to receive full response: %s. (Return code: %d)", errorString, returnCode);
        return false;
    }

    return true;
}


bool WeatherRadio::transmit(const char* buffer)
{
    int bytesWritten = 0;
    int returnCode = tty_write_string(PortFD, buffer, &bytesWritten);

    if (returnCode != TTY_OK)
    {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        LOGF_WARN("Failed to transmit %s. Wrote %d bytes and got error %s.", buffer, bytesWritten, errorString);
        return false;
    }
    return true;
}

bool WeatherRadio::sendQuery(const char* cmd, char* response, int *length)
{
    if(!transmit(cmd))
    {
        LOGF_ERROR("Command <%s> failed.", cmd);
        return false;
    }
    return receive(response, length, '\n', MAX_WAIT);;
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
