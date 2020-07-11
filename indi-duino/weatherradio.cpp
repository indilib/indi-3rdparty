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
#include "weathercalculator.h"

#include <cstdlib>
#include <cstring>
#include <map>

#include <sys/stat.h>

#include "indistandardproperty.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include "gason/gason.h"
#include "curl/curl.h"

#include "config.h"

const char *CALIBRATION_TAB = "Calibration";

/* Our weather station auto pointer */
std::unique_ptr<WeatherRadio> station_ptr(new WeatherRadio());

#define MAX_WEATHERBUFFER 512

#define ARDUINO_SETTLING_TIME 5

#define WIFI_DEVICE "WiFi"

#define WEATHER_TEMPERATURE     "WEATHER_TEMPERATURE"
#define WEATHER_PRESSURE        "WEATHER_PRESSURE"
#define WEATHER_HUMIDITY        "WEATHER_HUMIDITY"
#define WEATHER_CLOUD_COVER     "WEATHER_CLOUD_COVER"
#define WEATHER_SQM             "WEATHER_SQM"
#define WEATHER_DEWPOINT        "WEATHER_DEWPOINT"
#define WEATHER_SKY_TEMPERATURE "WEATHER_SKY_TEMPERATURE"
#define WEATHER_WIND_GUST       "WEATHER_WIND_GUST"
#define WEATHER_WIND_SPEED      "WEATHER_WIND_SPEED"
#define WEATHER_WIND_DIRECTION  "WEATHER_WIND_DIRECTION"

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
**
***************************************************************************************/
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    strcpy(static_cast<char *>(userp), static_cast<char *>(contents));
    return size * nmemb;
}

/**************************************************************************************
** Constructor
***************************************************************************************/
WeatherRadio::WeatherRadio()
{
    setVersion(WEATHERRADIO_VERSION_MAJOR, WEATHERRADIO_VERSION_MINOR);
    weatherCalculator = new WeatherCalculator();
}

/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool WeatherRadio::initProperties()
{
    INDI::Weather::initProperties();

    addConfigurationControl();

    IUFillNumber(&ttyTimeoutN[0], "TIMEOUT", "Timeout (s)", "%.f", 0, 60, 1, getTTYTimeout());
    IUFillNumberVector(&ttyTimeoutNP, ttyTimeoutN, 1, getDeviceName(), "TTY_TIMEOUT", "TTY timeout", CONNECTION_TAB, IP_RW, 0, IPS_OK);

    // Firmware version
    IUFillText(&FirmwareInfoT[0], "FIRMWARE_INFO", "Firmware Version", "<unknown version>");
    IUFillTextVector(&FirmwareInfoTP, FirmwareInfoT, 1, getDeviceName(), "FIRMWARE", "Firmware", INFO_TAB, IP_RO, 60, IPS_OK);

    // Reset Arduino
    IUFillSwitch(&resetArduinoS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&resetArduinoSP, resetArduinoS, 1, getDeviceName(), "RESET_ARDUINO", "Arduino", INFO_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // refresh firmware configuration
    IUFillSwitch(&refreshConfigS[0], "REFRESH", "Refresh", ISS_OFF);
    IUFillSwitchVector(&refreshConfigSP, refreshConfigS, 1, getDeviceName(), "REFRESH_CONFIG", "Refresh", INFO_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // connect/disconnect WiFi
    IUFillSwitch(&wifiConnectionS[0], "DISCONNECT", "Disconnect", ISS_OFF);
    IUFillSwitch(&wifiConnectionS[1], "CONNECT", "Connect", ISS_OFF);
    IUFillSwitchVector(&wifiConnectionSP, wifiConnectionS, 2, getDeviceName(), "WIFI", "WiFi", INFO_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // calibration parameters
    IUFillNumber(&skyTemperatureCalibrationN[0], "K1", "K1", "%.2f", 0, 100, 1, weatherCalculator->skyTemperatureCoefficients.k1);
    IUFillNumber(&skyTemperatureCalibrationN[1], "K2", "K2", "%.2f", -50, 50, 1, weatherCalculator->skyTemperatureCoefficients.k2);
    IUFillNumber(&skyTemperatureCalibrationN[2], "K3", "K3", "%.2f", 0, 100, 1, weatherCalculator->skyTemperatureCoefficients.k3);
    IUFillNumber(&skyTemperatureCalibrationN[3], "K4", "K4", "%.2f", 0, 100, 1, weatherCalculator->skyTemperatureCoefficients.k4);
    IUFillNumber(&skyTemperatureCalibrationN[4], "K5", "K5", "%.2f", 0, 100, 1, weatherCalculator->skyTemperatureCoefficients.k5);
    IUFillNumber(&skyTemperatureCalibrationN[5], "T_CLEAR", "clear sky (°C)", "%.2f", -20, 20, 1, weatherCalculator->skyTemperatureCoefficients.t_clear);
    IUFillNumber(&skyTemperatureCalibrationN[6], "T_OVERCAST", "overcast sky (°C)", "%.2f", -20, 20, 1, weatherCalculator->skyTemperatureCoefficients.t_overcast);
    IUFillNumberVector(&skyTemperatureCalibrationNP, skyTemperatureCalibrationN, 7, getDeviceName(), "SKY_TEMP_CALIBRATION", "Sky Temp calibr.", CALIBRATION_TAB, IP_RW, 0, IPS_OK);

    // calibration parameters
    IUFillNumber(&humidityCalibrationN[0], "FACTOR", "Factor", "%.3f", 0, 10, 0.1, 1.0);
    IUFillNumber(&humidityCalibrationN[1], "SHIFT", "Shift", "%.3f", -100, 100, 1, 0.0);
    IUFillNumberVector(&humidityCalibrationNP, humidityCalibrationN, 2, getDeviceName(), "HUMIDITY_CALIBRATION", "Humidity calibr.", CALIBRATION_TAB, IP_RW, 0, IPS_OK);

    IUFillNumber(&temperatureCalibrationN[0], "FACTOR", "Factor", "%.3f", 0, 10, 0.1, 1.0);
    IUFillNumber(&temperatureCalibrationN[1], "SHIFT", "Shift", "%.3f", -100, 100, 1, 0.0);
    IUFillNumberVector(&temperatureCalibrationNP, temperatureCalibrationN, 2, getDeviceName(), "TEMPERATURE_CALIBRATION", "Temperature calibr.", CALIBRATION_TAB, IP_RW, 0, IPS_OK);

    IUFillNumber(&sqmCalibrationN[0], "FACTOR", "Factor", "%.3f", 0, 10, 0.1, 1.0);
    IUFillNumber(&sqmCalibrationN[1], "SHIFT", "Shift", "%.3f", -100, 100, 1, 0.0);
    IUFillNumberVector(&sqmCalibrationNP, sqmCalibrationN, 2, getDeviceName(), "SQM_CALIBRATION", "SQM calibr.", CALIBRATION_TAB, IP_RW, 0, IPS_OK);

    IUFillNumber(&windDirectionCalibrationN[0], "OFFSET", "Offset", "%.3f", 0, 360, 1, 0.0);
    IUFillNumberVector(&windDirectionCalibrationNP, windDirectionCalibrationN, 1, getDeviceName(), "WIND_DIRECTION_CALIBRATION", "Wind direction", CALIBRATION_TAB, IP_RW, 0, IPS_OK);

    addDebugControl();
    setWeatherConnection(CONNECTION_SERIAL);

    // define INDI config values for known sensors
    deviceConfig["BME280"]["Temp"]      = {"Temperature (°C)", TEMPERATURE_SENSOR, "%.1f", -100.0, 100.0, 1.0};
    deviceConfig["BME280"]["Pres"]      = {"Pressure (hPa)", PRESSURE_SENSOR, "%.1f", 500., 1100.0, 1.0};
    deviceConfig["BME280"]["Hum"]       = {"Humidity (%)", HUMIDITY_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["DHT"]["Temp"]         = {"Temperature (°C)", TEMPERATURE_SENSOR, "%.1f", -100.0, 100.0, 1.0};
    deviceConfig["DHT"]["Hum"]          = {"Humidity (%)", HUMIDITY_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["MLX90614"]["T amb"]   = {"Ambient Temp. (°C)", TEMPERATURE_SENSOR, "%.1f", -100.0, 100.0, 1.0};
    deviceConfig["MLX90614"]["T obj"]   = {"Sky Temp. (°C)", OBJECT_TEMPERATURE_SENSOR, "%.1f", -100.0, 100.0, 1.0};
    deviceConfig["TSL237"]["Frequency"] = {"Frequency", INTERNAL_SENSOR, "%.0f", 0.0, 100000.0, 1.0};
    deviceConfig["TSL237"]["SQM"]       = {"SQM", SQM_SENSOR, "%.1f", 0.0, 25.0, 1.0};
    deviceConfig["TSL2591"]["Lux"]      = {"Luminance (Lux)", LUMINOSITY_SENSOR, "%.3f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["Visible"]  = {"Lightness (Vis)", INTERNAL_SENSOR, "%.1f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["IR"]       = {"Lightness (IR)", INTERNAL_SENSOR, "%.1f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["Gain"]     = {"Gain", INTERNAL_SENSOR, "%.0f", 0.0, 1000.0, 1.0};
    deviceConfig["TSL2591"]["Timing"]   = {"Timing", INTERNAL_SENSOR, "%.0f", 0.0, 1000.0, 1.0};
    deviceConfig["Davis Anemometer"]["avg speed"] = {"Wind speed (avg, m/s)", WIND_SPEED_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["Davis Anemometer"]["min speed"] = {"Wind speed (min, m/s)", INTERNAL_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["Davis Anemometer"]["max speed"] = {"Wind speed (max, m/s)", WIND_GUST_SENSOR, "%.1f", 0., 100.0, 1.0};
    deviceConfig["Davis Anemometer"]["direction"] = {"Wind direction (deg)", WIND_DIRECTION_SENSOR, "%.0f", 0., 360.0, 1.0};
    deviceConfig["Davis Anemometer"]["rotations"] = {"Wind wheel rotations", INTERNAL_SENSOR, "%.0f", 0., 360.0, 1.0};

    LOG_DEBUG("Properties initialization finished successfully.");
    return true;
}

/**************************************************************************************
** Update the INDI properties as a reaction on connect or disconnect.
***************************************************************************************/
bool WeatherRadio::updateProperties()
{
    bool result = true;

    // dynamically add weather parameters
    if (isConnected())
    {
        if (sensorRegistry.temperature.size() > 0)
        {
            addParameter(WEATHER_TEMPERATURE, "Temperature (°C)", -10, 30, 15);
            setCriticalParameter(WEATHER_TEMPERATURE);
            addSensorSelection(&temperatureSensorSP, sensorRegistry.temperature, "TEMPERATURE_SENSOR", "Temperature Sensor");
            addSensorSelection(&ambientTemperatureSensorSP, sensorRegistry.temperature, "AMBIENT_TEMP_SENSOR", "Ambient Temp. Sensor");

            defineNumber(&temperatureCalibrationNP);
            defineNumber(&skyTemperatureCalibrationNP);
            LOG_INFO("Temperature sensor selections added.");
        }
        if (sensorRegistry.pressure.size() > 0)
        {
            addParameter(WEATHER_PRESSURE, "Pressure (hPa)", 950, 1070, 15);
            setCriticalParameter(WEATHER_PRESSURE);
            addSensorSelection(&pressureSensorSP, sensorRegistry.pressure, "PRESSURE_SENSOR", "Pressure Sensor");
            LOG_INFO("Pressure sensor selections added.");
        }
        if (sensorRegistry.humidity.size() > 0)
        {
            addParameter(WEATHER_HUMIDITY, "Humidity (%)", 0, 100, 15);
            addParameter(WEATHER_DEWPOINT, "Dewpoint (°C)", -10, 30, 15);
            setCriticalParameter(WEATHER_HUMIDITY);
            addSensorSelection(&humiditySensorSP, sensorRegistry.humidity, "HUMIDITY_SENSOR", "Humidity Sensor");

            defineNumber(&humidityCalibrationNP);
            LOG_INFO("Humidity sensor selections added.");
        }
        if (sensorRegistry.luminosity.size() > 0 || sensorRegistry.sqm.size() > 0)
        {
            addParameter(WEATHER_SQM, "SQM", 10, 30, 15);
            setCriticalParameter(WEATHER_SQM);
            if (sensorRegistry.luminosity.size() > 0)
            {
                addSensorSelection(&luminositySensorSP, sensorRegistry.luminosity, "LUMINOSITY_SENSOR", "Luminosity Sensor");
                LOG_INFO("Luminosity sensor selections added.");
            }
            if (sensorRegistry.sqm.size() > 0)
            {
                addSensorSelection(&sqmSensorSP, sensorRegistry.sqm, "SQM_SENSOR", "SQM Sensor");
                defineNumber(&sqmCalibrationNP);
                LOG_INFO("SQM sensor selections added.");
            }
        }
        if (sensorRegistry.temp_object.size() > 0)
        {
            addParameter(WEATHER_CLOUD_COVER, "Clouds (%)", 0, 100, 50);
            addParameter(WEATHER_SKY_TEMPERATURE, "Sky Temp (corr, °C)", -30, 20, 0);
            setCriticalParameter(WEATHER_CLOUD_COVER);
            addSensorSelection(&objectTemperatureSensorSP, sensorRegistry.temp_object, "OBJECT_TEMP_SENSOR", "Object Temp. Sensor");
            LOG_INFO("Sky temperature sensor selections added.");
        }
        if (sensorRegistry.wind_gust.size() > 0)
        {
            addParameter(WEATHER_WIND_GUST, "Wind gust (m/s)", 0, 15, 50);
            setCriticalParameter(WEATHER_WIND_GUST);
            addSensorSelection(&windGustSensorSP, sensorRegistry.wind_gust, "WIND_GUST_SENSOR", "Wind Gust Sensor");
            LOG_INFO("Wind gust sensor selections added.");
        }
        if (sensorRegistry.wind_speed.size() > 0)
        {
            addParameter(WEATHER_WIND_SPEED, "Wind speed (m/s)", 0, 10, 50);
            setCriticalParameter(WEATHER_WIND_SPEED);
            addSensorSelection(&windSpeedSensorSP, sensorRegistry.wind_speed, "WIND_SPEED_SENSOR", "Wind Speed Sensor");
            LOG_INFO("Wind speed sensor selections added.");
        }
        if (sensorRegistry.wind_direction.size() > 0)
        {
            addParameter(WEATHER_WIND_DIRECTION, "Wind direction (deg)", 0, 360, 10);
            addSensorSelection(&windDirectionSensorSP, sensorRegistry.wind_direction, "WIND_DIRECTION_SENSOR", "Wind Direction Sensor");

            defineNumber(&windDirectionCalibrationNP);
            LOG_INFO("Wind direction sensor selections added.");
        }
        for (size_t i = 0; i < rawDevices.size(); i++)
            defineNumber(&rawDevices[i]);
        LOG_INFO("Raw sensors added.");

        getBasicData();
        // update the weather parameters to avoid sending dummy weather values
        updateWeather();

        result = INDI::Weather::updateProperties();
        // Load the configuration if everything was fine
        if (result == true)
            loadConfig();

        defineSwitch(&resetArduinoSP);
    }
    else
    {

        for (size_t i = 0; i < rawDevices.size(); i++)
            deleteProperty(rawDevices[i].name);

        deleteProperty(resetArduinoSP.name);
        deleteProperty(windDirectionCalibrationNP.name);
        deleteProperty(sqmCalibrationNP.name);
        deleteProperty(temperatureCalibrationNP.name);
        deleteProperty(humidityCalibrationNP.name);
        deleteProperty(skyTemperatureCalibrationNP.name);
        deleteProperty(temperatureSensorSP.name);
        deleteProperty(pressureSensorSP.name);
        deleteProperty(humiditySensorSP.name);
        deleteProperty(luminositySensorSP.name);
        deleteProperty(sqmSensorSP.name);
        deleteProperty(ambientTemperatureSensorSP.name);
        deleteProperty(objectTemperatureSensorSP.name);
        deleteProperty(windGustSensorSP.name);
        deleteProperty(windSpeedSensorSP.name);
        deleteProperty(windDirectionSensorSP.name);
        deleteProperty(refreshConfigSP.name);
        deleteProperty(wifiConnectionSP.name);
        deleteProperty(FirmwareInfoTP.name);
        deleteProperty(FirmwareConfigTP.name);

        result = INDI::Weather::updateProperties();

        // clean up weather interface parameters to avoid doubling when reconnecting
        for (int i = 0; i < WeatherInterface::ParametersNP.nnp; i++)
        {
            free(WeatherInterface::ParametersN[i].aux0);
            free(WeatherInterface::ParametersN[i].aux1);
            free(WeatherInterface::ParametersRangeNP[i].np);
        }

        free(WeatherInterface::ParametersN);
        WeatherInterface::ParametersN = nullptr;
        WeatherInterface::ParametersNP.nnp = 0;
        if (WeatherInterface::ParametersRangeNP != nullptr)
        {
            WeatherInterface::ParametersRangeNP->nnp = 0;
            free(WeatherInterface::ParametersRangeNP);
            WeatherInterface::ParametersRangeNP = nullptr;
        }
        WeatherInterface::nRanges = 0;

        free(WeatherInterface::critialParametersL);
        WeatherInterface::critialParametersL = nullptr;
        WeatherInterface::critialParametersLP.nlp = 0;

        LOG_DEBUG("Weather Radio properties removal completed.");

    }

    return result;
}



/**************************************************************************************
** Retrieve basic data after a successful connect.
***************************************************************************************/
IPState WeatherRadio::getBasicData()
{

    FirmwareInfoT[0].text = new char[64];
    FirmwareInfoTP.s = getFirmwareVersion(FirmwareInfoT[0].text);
    if (FirmwareInfoTP.s != IPS_OK)
    {
        LOG_ERROR("Failed to get firmware from device.");
        return FirmwareInfoTP.s;
    }
    else
        LOGF_INFO("Firmware version: %s", FirmwareInfoT[0].text);

    defineText(&FirmwareInfoTP);
    IDSetText(&FirmwareInfoTP, nullptr);

    FirmwareConfig config;
    IPState result = readFirmwareConfig(&config);
    if (result != IPS_OK)
    {
        LOG_ERROR("Failed to get firmware configuration from device.");
        return result;
    }

    FirmwareConfigT = new IText[config.size()];
    std::map<std::string, std::string>::iterator it;
    size_t pos = 0;

    for (it = config.begin(); it != config.end(); ++it)
    {
        FirmwareConfigT[pos].text = nullptr; // seems like a bug in IUFillText that this is necessary
        IUFillText(&FirmwareConfigT[pos++], it->first.c_str(), it->first.c_str(), it->second.c_str());
        LOGF_INFO("Firmware config: %s = %s", it->first.c_str(), it->second.c_str());
    }

    IUFillTextVector(&FirmwareConfigTP, FirmwareConfigT, static_cast<int>(config.size()), getDeviceName(), "FIRMWARE_CONFIGS", "Firmware config", INFO_TAB, IP_RO, 60, IPS_OK);
    defineText(&FirmwareConfigTP);

    // refresh button
    defineSwitch(&refreshConfigSP);

    if (hasWiFi)
        defineSwitch(&wifiConnectionSP);

    return IPS_OK;
}

/**************************************************************************************
** Update firmware configuration data
***************************************************************************************/
void WeatherRadio::updateConfigData()
{
    FirmwareInfoTP.s = getFirmwareVersion(FirmwareInfoT[0].text);
    if (FirmwareInfoTP.s != IPS_OK)
        LOG_ERROR("Failed to get firmware from device.");

    FirmwareConfig config;
    readFirmwareConfig(&config);
    std::map<std::string, std::string>::iterator it;

    for (it = config.begin(); it != config.end(); ++it)
    {
        // find the matching text property
        for (int i = 0; i < FirmwareConfigTP.ntp; i++)
            if (strcmp(FirmwareConfigT[i].name, it->first.c_str()) == 0)
            {
                IUSaveText(&FirmwareConfigT[i], it->second.c_str());
                LOGF_INFO("Firmware config: %s = %s", it->first.c_str(), it->second.c_str());
            }
    }
    FirmwareConfigTP.s = IPS_OK;
    IDSetText(&FirmwareInfoTP, nullptr);
    IDSetText(&FirmwareConfigTP, nullptr);
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
        LOG_DEBUG("Firmware retrieved successfully.");
    }
    LOG_DEBUG("Request for firmware version failed!");
    return IPS_ALERT;
}

/**************************************************************************************
** Read the configuration parameters from the firmware
***************************************************************************************/
IPState WeatherRadio::readFirmwareConfig(FirmwareConfig *config)
{
    char data[MAX_WEATHERBUFFER] = {0};
    int n_bytes = 0;
    bool result = sendQuery("c", data, &n_bytes);

    if (result)
    {
        // LOGF_DEBUG("Firmware configuration response: %s", data);
        char *source = data;
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
            char *device {new char[strlen(deviceIter->key)+1] {0}};
            strncpy(device, deviceIter->key, static_cast<size_t>(strlen(deviceIter->key)));

            if (strcmp(device, WIFI_DEVICE) == 0)
                hasWiFi = true;

            JsonIterator configIter;

            // read settings for the single device
            for (configIter = begin(deviceIter->value); configIter != end(deviceIter->value); ++configIter)
            {
                char *name {new char[strlen(configIter->key)+1] {0}};

                // copy single setting
                strncpy(name, configIter->key, static_cast<size_t>(strlen(configIter->key)));
                std::string value;
                double number;

                switch (configIter->value.getTag()) {
                case JSON_NUMBER:
                    number = configIter->value.toNumber();
                    if (trunc(number) == number)
                        value = std::to_string(int(number));
                    else
                        value = std::to_string(number);
                    break;
                case JSON_TRUE:
                    value = "true";
                    break;
                case JSON_FALSE:
                    value = "false";
                    break;
                default:
                    value = strdup(configIter->value.toString());
                    break;
                }
                // add it to the configuration
                (*config)[std::string(device) + "::" + std::string(name)] = value;
            }

        }
        // update WiFi status
        if (hasWiFi)
        {
            FirmwareConfig::iterator configIt = config->find(std::string(WIFI_DEVICE) + "::" + "connected");
            bool connected = (configIt != config->end() && strcmp(configIt->second.c_str(), "true") == 0);

            LOG_DEBUG("WiFi device detected.");
            updateWiFiStatus(connected);
        }

        LOG_DEBUG("Firmware parsed successfully.");
        return IPS_OK;

    }
    else
    {
        LOG_WARN("Retrieving firmware config failed.");
        return IPS_ALERT;
    }
}

/**************************************************************************************
** Connect / disconnect the Arduino to WiFi.
***************************************************************************************/
bool WeatherRadio::connectWiFi(bool connect)
{
    bool result;
    if (connect)
    {
        result = transmitSerial("s\n");
        LOGF_INFO("Connecting WiFi %", result ? "succeeded." : "failed!");
    }
    else
    {
        result = transmitSerial("d\n");
        LOGF_INFO("Disonnecting WiFi %", result ? "succeeded." : "failed!");
    }
    return result;
}

void WeatherRadio::updateWiFiStatus(bool connected)
{
    wifiConnectionS[0].s = connected ? ISS_OFF : ISS_ON;
    wifiConnectionS[1].s = connected ? ISS_ON : ISS_OFF;
    wifiConnectionSP.s = IPS_OK;

    IDSetSwitch(&wifiConnectionSP, nullptr);
    LOGF_INFO("WiFi %s.", connected ? "connected" : "disconnected");
}

/**************************************************************************************
** Reset the Arduino.
***************************************************************************************/
bool WeatherRadio::resetArduino()
{
    bool result = transmitSerial("r\n");
    LOGF_INFO("Resetting Arduino %", result ? "succeeded." : "failed!");
    return result;
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
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // grab host name and port for HTTP connection
        if (strcmp(name, INDI::SP::DEVICE_ADDRESS) == 0)
        {
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], "ADDRESS") == 0)
                    strcpy(hostname, texts[i]);
                else if (strcmp(names[i], "PORT") == 0)
                    strcpy(port, texts[i]);
            }
            // fall through to pass the text vector to the base class
        }
    }
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process Number properties
***************************************************************************************/
bool WeatherRadio::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, ttyTimeoutNP.name) == 0)
        {
            IUUpdateNumber(&ttyTimeoutNP, values, names, n);
            ttyTimeout = int(values[0]);
            ttyTimeoutNP.s = IPS_OK;
            IDSetNumber(&ttyTimeoutNP, nullptr);
            return ttyTimeoutNP.s;
        }
        else if (strcmp(name, skyTemperatureCalibrationNP.name) == 0)
        {
            IUUpdateNumber(&skyTemperatureCalibrationNP, values, names, n);
            if (n > 0) weatherCalculator->skyTemperatureCoefficients.k1 = values[0];
            if (n > 1) weatherCalculator->skyTemperatureCoefficients.k2 = values[1];
            if (n > 2) weatherCalculator->skyTemperatureCoefficients.k3 = values[2];
            if (n > 3) weatherCalculator->skyTemperatureCoefficients.k4 = values[3];
            if (n > 4) weatherCalculator->skyTemperatureCoefficients.k5 = values[4];
            if (n > 5) weatherCalculator->skyTemperatureCoefficients.t_clear = values[5];
            if (n > 6) weatherCalculator->skyTemperatureCoefficients.t_overcast = values[6];
            skyTemperatureCalibrationNP.s = IPS_OK;
            IDSetNumber(&skyTemperatureCalibrationNP, nullptr);
            LOG_DEBUG("Cloud coverage value calibration updated.");
            return skyTemperatureCalibrationNP.s;
        }
        else if (strcmp(name, humidityCalibrationNP.name) == 0)
        {
            IUUpdateNumber(&humidityCalibrationNP, values, names, n);
            weatherCalculator->humidityCalibration.factor = values[0];
            weatherCalculator->humidityCalibration.shift  = values[1];
            humidityCalibrationNP.s = IPS_OK;
            IDSetNumber(&humidityCalibrationNP, nullptr);
            LOG_DEBUG("Humidity value calibration updated.");
            return humidityCalibrationNP.s;
        }
        else if (strcmp(name, temperatureCalibrationNP.name) == 0)
        {
            IUUpdateNumber(&temperatureCalibrationNP, values, names, n);
            weatherCalculator->temperatureCalibration.factor = values[0];
            weatherCalculator->temperatureCalibration.shift  = values[1];
            temperatureCalibrationNP.s = IPS_OK;
            IDSetNumber(&temperatureCalibrationNP, nullptr);
            LOG_DEBUG("Temperature value calibration updated.");
            return temperatureCalibrationNP.s;
        }
        else if (strcmp(name, sqmCalibrationNP.name) == 0)
        {
            IUUpdateNumber(&sqmCalibrationNP, values, names, n);
            weatherCalculator->sqmCalibration.factor = values[0];
            weatherCalculator->sqmCalibration.shift  = values[1];
            sqmCalibrationNP.s = IPS_OK;
            IDSetNumber(&sqmCalibrationNP, nullptr);
            return sqmCalibrationNP.s;
            LOG_DEBUG("SQM value calibration updated.");
        }
        else if (strcmp(name, windDirectionCalibrationNP.name) == 0)
        {
            IUUpdateNumber(&windDirectionCalibrationNP, values, names, n);
            weatherCalculator->windDirectionOffset = values[0];
            windDirectionCalibrationNP.s = IPS_OK;
            IDSetNumber(&windDirectionCalibrationNP, nullptr);
            return windDirectionCalibrationNP.s;
            LOG_DEBUG("Wind direction value calibration updated.");
        }
    }
    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Process Switch properties
***************************************************************************************/
bool WeatherRadio::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (strcmp(name, refreshConfigSP.name) == 0)
        {
            // refresh config button pressed
            IUUpdateSwitch(&refreshConfigSP, states, names, n);
            updateConfigData();

            refreshConfigSP.s = IPS_OK;
            refreshConfigS[0].s = ISS_OFF;
            IDSetSwitch(&refreshConfigSP, nullptr);

            LOG_INFO("Firmware configuration data updated.");
            return (refreshConfigSP.s == IPS_OK);
        }
        else if (strcmp(name, wifiConnectionSP.name) == 0)
        {
            // reconnect config button pressed
            IUUpdateSwitch(&wifiConnectionSP, states, names, n);
            int pressed = IUFindOnSwitchIndex(&wifiConnectionSP);

            wifiConnectionSP.s = connectWiFi(pressed == 1) ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&wifiConnectionSP, nullptr);

            LOGF_INFO("%s WiFi. Press \"Refresh\" to update the status.", pressed == 1? "Connecting" :"Disconnecting");
            return (wifiConnectionSP.s == IPS_OK);
        }
        else if (strcmp(name, resetArduinoSP.name) == 0)
        {
            // reset Arduino button pressed
            IUUpdateSwitch(&resetArduinoSP, states, names, n);

            if (resetArduino())
                resetArduinoSP.s = IPS_OK;
            else
                resetArduinoSP.s = IPS_ALERT;
            resetArduinoS->s = ISS_OFF;
            IDSetSwitch(&resetArduinoSP, nullptr);

            LOG_INFO("Resetting Arduino.  Press \"Refresh\" to update the status");
            return (resetArduinoSP.s == IPS_OK);
        }
        else if (strcmp(name, temperatureSensorSP.name) == 0)
        {
            // temperature sensor selected
            IUUpdateSwitch(&temperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&temperatureSensorSP, selected);
            currentSensors.temperature = sensor;

            LOGF_DEBUG("Temperature sensor selected: %s", selected);
            return (temperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, pressureSensorSP.name) == 0)
        {
            // pressure sensor selected
            IUUpdateSwitch(&pressureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&pressureSensorSP, selected);
            currentSensors.pressure = sensor;

            LOGF_DEBUG("Pressure sensor selected: %s", selected);
            return (pressureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, humiditySensorSP.name) == 0)
        {
            // humidity sensor selected
            IUUpdateSwitch(&humiditySensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&humiditySensorSP, selected);
            currentSensors.humidity = sensor;

            LOGF_DEBUG("Humidity sensor selected: %s", selected);
            return (humiditySensorSP.s == IPS_OK);
        }
        else if (strcmp(name, luminositySensorSP.name) == 0)
        {
            // luminosity sensor selected
            IUUpdateSwitch(&luminositySensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&luminositySensorSP, selected);
            currentSensors.luminosity = sensor;

            LOGF_DEBUG("Luminosity sensor selected: %s", selected);
            return (luminositySensorSP.s == IPS_OK);
        }
        else if (strcmp(name, sqmSensorSP.name) == 0)
        {
            // SQM sensor selected
            IUUpdateSwitch(&sqmSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&sqmSensorSP, selected);
            currentSensors.sqm = sensor;

            LOGF_DEBUG("SQM sensor selected: %s", selected);
            return (sqmSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, ambientTemperatureSensorSP.name) == 0)
        {
            // ambient temperature sensor selected
            IUUpdateSwitch(&ambientTemperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&ambientTemperatureSensorSP, selected);
            currentSensors.temp_ambient = sensor;

            LOGF_DEBUG("Ambient temperature sensor selected: %s", selected);
            return (ambientTemperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, objectTemperatureSensorSP.name) == 0)
        {
            // object temperature sensor selected
            IUUpdateSwitch(&objectTemperatureSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&objectTemperatureSensorSP, selected);
            currentSensors.temp_object = sensor;

            LOGF_DEBUG("Object temperature sensor selected: %s", selected);
            return (objectTemperatureSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, windGustSensorSP.name) == 0)
        {
            // wind gust sensor selected
            IUUpdateSwitch(&windGustSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&windGustSensorSP, selected);
            currentSensors.wind_gust = sensor;

            LOGF_DEBUG("Wind gust sensor selected: %s", selected);
            return (windGustSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, windSpeedSensorSP.name) == 0)
        {
            // wind speed sensor selected
            IUUpdateSwitch(&windSpeedSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&windSpeedSensorSP, selected);
            currentSensors.wind_speed = sensor;

            LOGF_DEBUG("Wind speed sensor selected: %s", selected);
            return (windSpeedSensorSP.s == IPS_OK);
        }
        else if (strcmp(name, windDirectionSensorSP.name) == 0)
        {
            // wind direction sensor selected
            IUUpdateSwitch(&windDirectionSensorSP, states, names, n);

            const char *selected = IUFindOnSwitchName(states, names, n);
            sensor_name sensor = updateSensorSelection(&windDirectionSensorSP, selected);
            currentSensors.wind_direction = sensor;

            LOGF_DEBUG("Wind direction sensor selected: %s", selected);
            return (windDirectionSensorSP.s == IPS_OK);
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
    // Sleep for 5 seconds so that the serial connection of the Arduino has settled
    // This seems to be necessary for some Arduinos, otherwise they run into a timeout
    struct timespec request_delay = {ARDUINO_SETTLING_TIME, 0L};
    DEBUGFDEVICE(getDeviceName(), INDI::Logger::DBG_SESSION, "Waiting for %d seconds the communication to Arduino to settle.", ARDUINO_SETTLING_TIME);

    nanosleep(&request_delay, nullptr);

    // read the weather parameters for the first time so that #updateProperties() knows all sensors
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

    if (result == false)
        return IPS_ALERT;

    char *src{new char[n_bytes+1] {0}};
    // duplicate the buffer since the parser will modify it
    strncpy(src, data, static_cast<size_t>(n_bytes));
    result = parseWeatherData(src);

    // result recieved
    LOGF_DEBUG("Reading weather data from Arduino %s", result ? "succeeded." : "failed!");
    return result == true ? IPS_OK : IPS_ALERT;
}

/**************************************************************************************
** Parse JSON weather document.
***************************************************************************************/
bool WeatherRadio::parseWeatherData(char *data)
{
    char *source = data;
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
    LOG_DEBUG("Parsing weather data succeeded.");
    return true;

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
        setParameterValue(WEATHER_TEMPERATURE, weatherCalculator->calibrate(weatherCalculator->temperatureCalibration, value));
    else if (currentSensors.pressure == sensor)
    {
        double elevation = LocationN[LOCATION_ELEVATION].value;

        double temp = 15.0; // default value
        INumber *temperatureParameter = getWeatherParameter(WEATHER_TEMPERATURE);
        if (temperatureParameter != nullptr)
            temp = temperatureParameter->value;

        double pressure_normalized = weatherCalculator->sealevelPressure(value, elevation, temp);
        setParameterValue(WEATHER_PRESSURE, pressure_normalized);
    }
    else if (currentSensors.humidity == sensor)
    {
        double humidity = weatherCalculator->calibrate(weatherCalculator->humidityCalibration, value);

        setParameterValue(WEATHER_HUMIDITY, humidity);
        INumber *temperatureParameter = getWeatherParameter(WEATHER_TEMPERATURE);
        if (temperatureParameter != nullptr)
        {
            double dp =  weatherCalculator->dewPoint(humidity, temperatureParameter->value);
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
            setParameterValue(WEATHER_CLOUD_COVER, weatherCalculator->cloudCoverage(value, objectTemperature));
            setParameterValue(WEATHER_SKY_TEMPERATURE, weatherCalculator->skyTemperatureCorr(value, objectTemperature));
        }
    }
    else if (currentSensors.temp_object == sensor)
    {
        // obtain the current ambient temperature
        INumber *ambientProp = findRawSensorProperty(currentSensors.temp_ambient);
        if (ambientProp != nullptr)
        {
            double ambientTemperature = ambientProp->value;
            setParameterValue(WEATHER_CLOUD_COVER, weatherCalculator->cloudCoverage(ambientTemperature, value));
            setParameterValue(WEATHER_SKY_TEMPERATURE, weatherCalculator->skyTemperatureCorr(ambientTemperature, value));
        }
    }
    else if (currentSensors.luminosity == sensor)
    {
        setParameterValue(WEATHER_SQM, weatherCalculator->calibrate(weatherCalculator->sqmCalibration,
                                                                    weatherCalculator->sqmValue(value)));
    }
    else if (currentSensors.sqm == sensor)
        setParameterValue(WEATHER_SQM, weatherCalculator->calibrate(weatherCalculator->sqmCalibration, value));
    else if (currentSensors.wind_gust == sensor)
        setParameterValue(WEATHER_WIND_GUST, value);
    else if (currentSensors.wind_speed == sensor)
        setParameterValue(WEATHER_WIND_SPEED, value);
    else if (currentSensors.wind_direction == sensor)
        setParameterValue(WEATHER_WIND_DIRECTION, weatherCalculator->calibratedWindDirection(value));
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
    case SQM_SENSOR:
        sensorRegistry.sqm.push_back(sensor);
        break;
    case OBJECT_TEMPERATURE_SENSOR:
        sensorRegistry.temp_object.push_back(sensor);
        break;
    case WIND_GUST_SENSOR:
        sensorRegistry.wind_gust.push_back(sensor);
        break;
    case WIND_SPEED_SENSOR:
        sensorRegistry.wind_speed.push_back(sensor);
        break;
    case WIND_DIRECTION_SENSOR:
        sensorRegistry.wind_direction.push_back(sensor);
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
    IUSaveConfigNumber(fp, &skyTemperatureCalibrationNP);
    IUSaveConfigNumber(fp, &temperatureCalibrationNP);
    IUSaveConfigNumber(fp, &humidityCalibrationNP);
    IUSaveConfigNumber(fp, &sqmCalibrationNP);
    IUSaveConfigNumber(fp, &windDirectionCalibrationNP);
    IUSaveConfigSwitch(fp, &temperatureSensorSP);
    IUSaveConfigSwitch(fp, &pressureSensorSP);
    IUSaveConfigSwitch(fp, &humiditySensorSP);
    IUSaveConfigSwitch(fp, &luminositySensorSP);
    IUSaveConfigSwitch(fp, &sqmSensorSP);
    IUSaveConfigSwitch(fp, &ambientTemperatureSensorSP);
    IUSaveConfigSwitch(fp, &objectTemperatureSensorSP);
    IUSaveConfigSwitch(fp, &windGustSensorSP);
    IUSaveConfigSwitch(fp, &windSpeedSensorSP);
    IUSaveConfigSwitch(fp, &windDirectionSensorSP);
    if (ParametersRangeNP != nullptr)
        IUSaveConfigNumber(fp, ParametersRangeNP);
    IUSaveConfigNumber(fp, &ttyTimeoutNP);


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
** Communicate with serial device or HTTP server
***************************************************************************************/
bool WeatherRadio::sendQuery(const char* cmd, char* response, int *length)
{
    // communication through a serial (USB) interface
    if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
    {
        char cmdstring[20] = {0};
        sprintf(cmdstring, "%s\n", cmd);
        LOGF_DEBUG("Sending query: %s", cmdstring);

        if(!transmitSerial(cmdstring))
        {
            LOGF_ERROR("Command <%s> failed.", cmdstring);
            return false;
        }
        return receiveSerial(response, length, '\n', getTTYTimeout());
    }
    // communication through HTTP, e.g. with a ESP8266 Arduino chip
    else if (getActiveConnection()->type() == Connection::Interface::CONNECTION_TCP)
    {
        CURL *curl;
        CURLcode res;
        char requestURL[MAXRBUF];

        snprintf(requestURL, MAXRBUF, "http://%s:%s/%s", hostname, port, cmd);

        curl = curl_easy_init();
        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, requestURL);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if (res == CURLcode::CURLE_OK)
            {
                *length = strlen(response);
                return true;
            }
            else
            {
                LOGF_ERROR("HTTP request to %s failed.", hostname);
                return false;
            }
        }
        else {
            LOGF_ERROR("Cannot initialize CURL, connection to HTTP server %s failed.", hostname);
            return false;
        }
    }
    // this should not happen
    LOGF_ERROR("Unsupported active connection type: %d", getActiveConnection()->type());
    return false;
}

/**************************************************************************************
** Helper functions for serial communication
***************************************************************************************/
bool WeatherRadio::receiveSerial(char* buffer, int* bytes, char end, int wait)
{
    int timeout = wait;
    int returnCode = TTY_PORT_BUSY;
    int retry = 0;

    while (returnCode != TTY_OK && retry < 3)
    {
        returnCode = tty_read_section(PortFD, buffer, end, timeout, bytes);
        if (returnCode != TTY_OK)
        {
            char errorString[MAXRBUF];
            tty_error_msg(returnCode, errorString, MAXRBUF);
            if(returnCode == TTY_TIME_OUT && wait <= 0) return false;
            if (retry++ < 3)
                LOGF_INFO("Failed to receive full response: %s. (Return code: %d). Retrying...", errorString, returnCode);
            else
            {
                LOGF_WARN("Failed to receive full response: %s. (Return code: %d). Giving up", errorString, returnCode);
                return false;
            }
        }
    }
    return true;
}


bool WeatherRadio::transmitSerial(const char* buffer)
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
