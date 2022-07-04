/*******************************************************************************
  Copyright(c) 2022 Rick Bassham. All rights reserved.

  INDI WeeWx JSON Weather Driver

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "indi_weewx_json.h"
#include "config.h"

#include <curl/curl.h>

#include <memory>
#include <cstring>
#include <string>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    strcpy(static_cast<char *>(userp), static_cast<char *>(contents));
    return size * nmemb;
}

// We declare an auto pointer to WeewxJSON.
std::unique_ptr<WeewxJSON> weewx_json(new WeewxJSON());

WeewxJSON::WeewxJSON()
{
    setVersion(WEEWX_VERSION_MAJOR, WEEWX_VERSION_MINOR);

    setWeatherConnection(CONNECTION_NONE);
}

WeewxJSON::~WeewxJSON() {}

const char *WeewxJSON::getDefaultName()
{
    return (const char *)"WeewxJSON";
}

bool WeewxJSON::Connect()
{
    return true;
}

bool WeewxJSON::Disconnect()
{
    return true;
}

bool WeewxJSON::initProperties()
{
    INDI::Weather::initProperties();

    weewxJsonUrl[WEEWX_URL].fill("WEEWX_URL", "Weewx JSON URL", nullptr);

    weewxJsonUrl.fill(getDeviceName(), "WEEWX_URL", "Weewx", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_DEW_POINT", "Dew Point (C)", -20, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_HEAT_INDEX", "Heat Index (C)", -20, 35, 15);
    addParameter("WEATHER_BAROMETER", "Barometer (mbar)", 20, 32.5, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_GUST", "Wind Gust (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_DIRECTION", "Wind Direction", 0, 360, 15);
    addParameter("WEATHER_WIND_CHILL", "Wind Chill (C)", -20, 35, 15);
    addParameter("WEATHER_RAIN_RATE", "Rain (mm/h)", 0, 0, 15);

    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN_RATE");

    addDebugControl();

    return true;
}

void WeewxJSON::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    static bool once = true;
    if (once)
    {
        once = false;
        defineProperty(&weewxJsonUrl);
        loadConfig(true, weewxJsonUrl.getName());
    }
}

bool WeewxJSON::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(&weewxJsonUrl);
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(weewxJsonUrl.getName());
    }

    return true;
}

bool WeewxJSON::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (weewxJsonUrl.isNameMatch(name))
        {
            weewxJsonUrl.update(texts, names, n);
            weewxJsonUrl.setState(IPS_OK);
            weewxJsonUrl.apply();
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

void WeewxJSON::handleTemperatureData(JsonValue value, std::string key)
{
    double temperatureValue = 0.0;
    bool isFarhenheit       = false;

    JsonIterator iter;
    for (iter = begin(value); iter != end(value); ++iter)
    {
        if (strcmp(iter->key, "value") == 0)
        {
            temperatureValue = iter->value.toNumber();
        }
        else if (strcmp(iter->key, "units") == 0)
        {
            isFarhenheit = strcmp(iter->value.toString(), "°F") == 0;
        }
    }

    if (isFarhenheit)
    {
        temperatureValue = (temperatureValue - 32.0) * 5.0 / 9.0;
    }

    setParameterValue(key, temperatureValue);
}

void WeewxJSON::handleRawData(JsonValue value, std::string key)
{
    double rawValue = 0.0;

    JsonIterator iter;
    for (iter = begin(value); iter != end(value); ++iter)
    {
        if (strcmp(iter->key, "value") == 0)
        {
            rawValue = iter->value.toNumber();
        }
    }

    setParameterValue(key, rawValue);
}

void WeewxJSON::handleBarometerData(JsonValue value)
{
    double pressureValue = 0.0;
    bool isINHG          = false;

    JsonIterator iter;
    for (iter = begin(value); iter != end(value); ++iter)
    {
        if (strcmp(iter->key, "value") == 0)
        {
            pressureValue = iter->value.toNumber();
        }
        else if (strcmp(iter->key, "units") == 0)
        {
            isINHG = strcmp(iter->value.toString(), "inHg") == 0;
        }
    }

    if (isINHG)
    {
        pressureValue = pressureValue * 33.864;
    }

    setParameterValue("WEATHER_BAROMETER", pressureValue);
}

void WeewxJSON::handleWindSpeedData(JsonValue value, std::string key)
{
    double speedValue = 0.0;
    bool isMPH        = false;

    JsonIterator iter;
    for (iter = begin(value); iter != end(value); ++iter)
    {
        if (strcmp(iter->key, "value") == 0)
        {
            speedValue = iter->value.toNumber();
        }
        else if (strcmp(iter->key, "units") == 0)
        {
            isMPH = strcmp(iter->value.toString(), "mph") == 0;
        }
    }

    if (isMPH)
    {
        speedValue = speedValue * 1.609;
    }

    setParameterValue(key, speedValue);
}

void WeewxJSON::handleRainRateData(JsonValue value)
{
    double rainRate  = 0.0;
    bool isInPerHour = false;

    JsonIterator iter;
    for (iter = begin(value); iter != end(value); ++iter)
    {
        if (strcmp(iter->key, "value") == 0)
        {
            rainRate = iter->value.toNumber();
        }
        else if (strcmp(iter->key, "units") == 0)
        {
            isInPerHour = strcmp(iter->value.toString(), "in/h") == 0;
        }
    }

    if (isInPerHour)
    {
        rainRate = rainRate * 25.4;
    }

    setParameterValue("WEATHER_RAIN_RATE", rainRate);
}

void WeewxJSON::handleWeatherData(JsonValue value)
{
    JsonIterator sensorIter;
    for (sensorIter = begin(value); sensorIter != end(value); ++sensorIter)
    {
        if (strcmp(sensorIter->key, "temperature") == 0)
        {
            handleTemperatureData(sensorIter->value, "WEATHER_TEMPERATURE");
        }
        else if (strcmp(sensorIter->key, "dewpoint") == 0)
        {
            handleTemperatureData(sensorIter->value, "WEATHER_DEW_POINT");
        }
        else if (strcmp(sensorIter->key, "humidity") == 0)
        {
            handleRawData(sensorIter->value, "WEATHER_HUMIDITY");
        }
        else if (strcmp(sensorIter->key, "heat index") == 0)
        {
            handleTemperatureData(sensorIter->value, "WEATHER_HEAT_INDEX");
        }
        else if (strcmp(sensorIter->key, "barometer") == 0)
        {
            handleBarometerData(sensorIter->value);
        }
        else if (strcmp(sensorIter->key, "wind speed") == 0)
        {
            handleWindSpeedData(sensorIter->value, "WEATHER_WIND_SPEED");
        }
        else if (strcmp(sensorIter->key, "wind gust") == 0)
        {
            handleWindSpeedData(sensorIter->value, "WEATHER_WIND_GUST");
        }
        else if (strcmp(sensorIter->key, "wind direction") == 0)
        {
            handleRawData(sensorIter->value, "WEATHER_WIND_DIRECTION");
        }
        else if (strcmp(sensorIter->key, "wind chill") == 0)
        {
            handleTemperatureData(sensorIter->value, "WEATHER_WIND_CHILL");
        }
        else if (strcmp(sensorIter->key, "rain rate") == 0)
        {
            handleRainRateData(sensorIter->value);
        }
    }
}

IPState WeewxJSON::updateWeather()
{
    if (isDebug())
        IDLog("%s: updateWeather()\n", getDeviceName());

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        char response[20480] = { 0 };
        curl_easy_setopt(curl, CURLOPT_URL, weewxJsonUrl[WEEWX_URL].text);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLcode::CURLE_OK)
        {
            char *endptr;
            JsonValue value;
            JsonAllocator allocator;
            int status = jsonParse(response, &endptr, &value, allocator);
            if (status != JSON_OK)
            {
                LOGF_ERROR("Parsing error %s at %zd", jsonStrError(status), endptr - response);
                return IPS_ALERT;
            }

            JsonIterator typeIter;
            for (typeIter = begin(value); typeIter != end(value); ++typeIter)
            {
                if (strcmp(typeIter->key, "current") == 0)
                {
                    handleWeatherData(typeIter->value);
                }
            }

            return IPS_OK;
        }
        else
        {
            LOGF_ERROR("HTTP request to %s failed.", weewxJsonUrl[WEEWX_URL].text);
            return IPS_ALERT;
        }
    }
    else
    {
        LOGF_ERROR("Cannot initialize CURL, connection to HTTP server %s failed.", weewxJsonUrl[WEEWX_URL].text);
        return IPS_ALERT;
    }

    return IPS_OK;
}

bool WeewxJSON::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    IUSaveConfigText(fp, &weewxJsonUrl);

    return true;
}
