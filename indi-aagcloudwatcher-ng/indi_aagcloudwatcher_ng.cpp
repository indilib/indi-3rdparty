/**
   This file is part of the AAG Cloud Watcher INDI Driver.
   A driver for the AAG Cloud Watcher (AAGware - http : //www.aagware.eu/)

   Copyright (C) 2012 - 2015 Sergio Alonso (zerjioi@ugr.es)
   Copyright (C) 2019 Adrián Pardini - Universidad Nacional de La Plata (github@tangopardo.com.ar)
   Copyright (C) 2021 Jasem Mutlaq

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

#include "indi_aagcloudwatcher_ng.h"

#include "config.h"

#include <cstring>
#include <cmath>
#include <memory>

#define ABS_ZERO 273.15

static std::unique_ptr<AAGCloudWatcher> cloudWatcher(new AAGCloudWatcher());

AAGCloudWatcher::AAGCloudWatcher()
{
    setVersion(AAG_VERSION_MAJOR, AAG_VERSION_MINOR);

    LOG_DEBUG("Initializing from AAG Cloud Watcher device...");

    cwc = new CloudWatcherController(true);

    lastReadPeriod = 3.0;

    heatingStatus = normal;

    pulseStartTime = -1;
    wetStartTime   = -1;

    desiredSensorTemperature = 0;
    globalRainSensorHeater   = -1;

}

AAGCloudWatcher::~AAGCloudWatcher()
{
    delete (cwc);
}

bool AAGCloudWatcher::Handshake()
{
    cwc->setPortFD(PortFD);

    // 2 sec delay added to support Pocket Cloudwatcher
    (void) sleep(2);

    auto check = cwc->checkCloudWatcher();

    if (check)
    {
        LOG_INFO("Connected to AAG Cloud Watcher (Lunatico Astro)");
        sendConstants();

        if (m_FirmwareVersion >= 5.6)
        {
            // add humidity parameter, if not already present
            if (!ParametersNP.findWidgetByName("WEATHER_HUMIDITY")) {
                addParameter("WEATHER_HUMIDITY", "Relative Humidity (%)", 0, 100, 10);
                setCriticalParameter("WEATHER_HUMIDITY");
            }
	    if (m_FirmwareVersion >= 5.89)
	    {
		// add SQM parameter, if not already present
		if (!ParametersNP.findWidgetByName("WEATHER_SQM")) {
		    addParameter("WEATHER_SQM", "SQM (mpsas)", 18.50, 28.50, 10);
		    setCriticalParameter("WEATHER_SQM");
		}
	    }
	    else
	    {
		// add pseudo-SQM parameter, if not already present
		if (!ParametersNP.findWidgetByName("WEATHER_SQM")) {
		    addParameter("WEATHER_SQM", "Ambient light brightness (K)", 2100, 1000000, 20);
		    setCriticalParameter("WEATHER_SQM");
		}
	    }
        }

        return true;
    }
    else
    {
        LOG_ERROR("Could not connect to AAG Cloud Watcher. Handshake failed. Check port or cable.");

        return false;
    }
}


/**********************************************************************
 ** Initialize all properties & set default values.
 **********************************************************************/
bool AAGCloudWatcher::initProperties()
{
    INDI::Weather::initProperties();
    buildSkeleton("indi_aagcloudwatcher_ng_sk.xml");

    addParameter("WEATHER_WIND_SPEED", "Wind speed (Km/H)", 0, 30, 50);
    addParameter("WEATHER_RAIN", "Rain (cycles)", 2000, 10000, 10);
    addParameter("WEATHER_CLOUD", "Cloud (corrected infrared sky temperature °C)", -40, 60, 20, true);

    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN");
    setCriticalParameter("WEATHER_CLOUD");


    addDebugControl();

    return true;
}

IPState AAGCloudWatcher::updateWeather()
{
    cwc->setElevation(LocationN[INDI::Weather::LOCATION_ELEVATION].value); // in case elevation updated as GPS gets a better fix

    if (!sendData())
    {
        LOG_ERROR("Can not get data from device");
        return IPS_ALERT;
    }

    heatingAlgorithm();

    return IPS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool AAGCloudWatcher::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI::Weather::ISNewNumber(dev, name, values, names, n);

    // Ignore if not ours
    if (strcmp(dev, getDefaultName()))
    {
        return false;
    }

    auto nvp = getNumber(name);

    if (!nvp)
    {
        return false;
    }

    if (nvp.isNameMatch("heaterParameters"))
    {
        for (int i = 0; i < 8; i++)
        {
            if ((strcmp(names[i], "tempLow") == 0) || (strcmp(names[i], "tempHigh") == 0))
            {
                if (values[i] < -50)
                {
                    values[i] = -50;
                }
                else if (values[i] > 100)
                {
                    values[i] = 100;
                }
            }

            if ((strcmp(names[i], "deltaHigh") == 0) || (strcmp(names[i], "deltaLow") == 0))
            {
                if (values[i] < 0)
                {
                    values[i] = 0;
                }
                else if (values[i] > 50)
                {
                    values[i] = 50;
                }
            }

            if ((strcmp(names[i], "min") == 0))
            {
                if (values[i] < 10)
                {
                    values[i] = 10;
                }
                else if (values[i] > 20)
                {
                    values[i] = 20;
                }
            }

            if ((strcmp(names[i], "heatImpulseTemp") == 0))
            {
                if (values[i] < 1)
                {
                    values[i] = 1;
                }
                else if (values[i] > 30)
                {
                    values[i] = 30;
                }
            }

            if ((strcmp(names[i], "heatImpulseDuration") == 0))
            {
                if (values[i] < 0)
                {
                    values[i] = 0;
                }
                else if (values[i] > 600)
                {
                    values[i] = 600;
                }
            }

            if ((strcmp(names[i], "heatImpulseCycle") == 0))
            {
                if (values[i] < 60)
                {
                    values[i] = 60;
                }
                else if (values[i] > 1000)
                {
                    values[i] = 1000;
                }
            }
        }

        nvp.update(values, names, n);
        nvp.setState(IPS_OK);
        nvp.apply();

        return true;
    }

    if (nvp.isNameMatch("skyCorrection"))
    {
        for (int i = 0; i < 5; i++)
        {
            if (values[i] < -999)
            {
                values[i] = -999;
            }
            if (values[i] > 999)
            {
                values[i] = 999;
            }
        }

        nvp.update(values, names, n);
        nvp.setState(IPS_OK);
        nvp.apply();

        return true;
    }

    return false;
}

bool AAGCloudWatcher::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // ignore if not ours
    if (strcmp(dev, getDefaultName()))
    {
        return false;
    }

    if (INDI::Weather::ISNewSwitch(dev, name, states, names, n) == true)
    {
        return true;
    }

    auto svp = getSwitch(name);

    if (!svp)
    {
        return false;
    }

    int error = 0;
    if (svp.isNameMatch("deviceSwitch"))
    {
        char *namesSw[2];
        ISState statesSw[2];
        statesSw[0] = ISS_ON;
        statesSw[1] = ISS_OFF;
        namesSw[0]  = const_cast<char *>("open");
        namesSw[1]  = const_cast<char *>("close");

        ISState openState;

        if (strcmp(names[0], "open") == 0)
        {
            openState = states[0];
        }
        else
        {
            openState = states[1];
        }

        if (openState == ISS_ON)
        {
            if (isConnected())
            {
                bool r = cwc->openSwitch();

                if (!r)
                {
                    statesSw[0] = ISS_OFF;
                    statesSw[1] = ISS_ON;
                }
            }
            else
            {
                statesSw[0] = ISS_ON;
                statesSw[1] = ISS_OFF;
                error       = 1;
            }
        }
        else
        {
            if (isConnected())
            {
                bool r = cwc->closeSwitch();

                if (r)
                {
                    statesSw[0] = ISS_OFF;
                    statesSw[1] = ISS_ON;
                }
            }
            else
            {
                statesSw[0] = ISS_ON;
                statesSw[1] = ISS_OFF;
                error       = 1;
            }
        }

        svp.update(statesSw, namesSw, 2);
        if (error)
        {
            svp.setState(IPS_IDLE);
        }
        else
        {
            svp.setState(IPS_OK);
        }
        svp.apply();

        return true;
    }

    if (svp.isNameMatch("anemometerType"))
    {
        svp.update(states, names, 2);
        svp.setState(IPS_OK);

        auto sp = svp.findWidgetByName("BLACK");
        if (sp->getState() == ISS_ON)
        {
            cwc->setAnemometerType(BLACK);
        }
        else
        {
            cwc->setAnemometerType(GRAY);
        }
    }

    return false;
}


float AAGCloudWatcher::getLastReadPeriod()
{
    return lastReadPeriod;
}

bool AAGCloudWatcher::isWetRain()
{
    return (checkParameterState("WEATHER_RAIN") != IPS_OK);
}

bool AAGCloudWatcher::heatingAlgorithm()
{
    auto heaterParameters = getNumber("heaterParameters");
    float tempLow                           = getNumberValueFromVector(heaterParameters, "tempLow");
    float tempHigh                          = getNumberValueFromVector(heaterParameters, "tempHigh");
    float deltaLow                          = getNumberValueFromVector(heaterParameters, "deltaLow");
    float deltaHigh                         = getNumberValueFromVector(heaterParameters, "deltaHigh");
    float heatImpulseTemp                   = getNumberValueFromVector(heaterParameters, "heatImpulseTemp");
    float heatImpulseDuration               = getNumberValueFromVector(heaterParameters, "heatImpulseDuration");
    float heatImpulseCycle                  = getNumberValueFromVector(heaterParameters, "heatImpulseCycle");
    float min                               = getNumberValueFromVector(heaterParameters, "min");

    auto sensors = getNumber("sensors");
    float ambient                  = getNumberValueFromVector(sensors, "ambientTemperatureSensor");
    float rainSensorTemperature    = getNumberValueFromVector(sensors, "rainSensorTemperature");

    // XXX FIXME: when the automatic refresh is disabled the refresh period is set to 0, however we can be called in a manual fashion.
    // this is needed as we divide by refresh later...
    if (WI::UpdatePeriodNP[0].getValue() < 3)
    {
        WI::UpdatePeriodNP[0].setValue(3.0);
    }

    if (globalRainSensorHeater == -1)
    {
        // If not already setted
        globalRainSensorHeater = getNumberValueFromVector(sensors, "rainSensorHeater");
    }

    time_t currentTime = time(nullptr);

    if ((isWetRain()) && (heatingStatus == normal))
    {
        // We check if sensor is wet.
        if (wetStartTime == -1)
        {
            // First moment wet
            wetStartTime = time(nullptr);
        }
        else
        {
            // We have been wet for a while

            if (currentTime - wetStartTime >= heatImpulseCycle)
            {
                // We have been a cycle wet. Apply pulse
                wetStartTime   = -1;
                heatingStatus  = increasingToPulse;
                pulseStartTime = -1;
            }
        }
    }
    else
    {
        // is not wet
        wetStartTime = -1;
    }

    if (heatingStatus == pulse)
    {
        if (currentTime - pulseStartTime > heatImpulseDuration)
        {
            // Pulse ends
            heatingStatus  = normal;
            wetStartTime   = -1;
            pulseStartTime = -1;
        }
    }

    if (heatingStatus == normal)
    {
        // Compute desired temperature

        if (ambient < tempLow)
        {
            desiredSensorTemperature = deltaLow;
        }
        else if (ambient > tempHigh)
        {
            desiredSensorTemperature = ambient + deltaHigh;
        }
        else
        {
            // Between tempLow and tempHigh
            float delt = ((((ambient - tempLow) / (tempHigh - tempLow)) * (deltaHigh - deltaLow)) + deltaLow);

            desiredSensorTemperature = ambient + delt;

            if (desiredSensorTemperature < tempLow)
            {
                desiredSensorTemperature = deltaLow;
            }
        }
    }
    else
    {
        desiredSensorTemperature = ambient + heatImpulseTemp;
    }

    if (heatingStatus == increasingToPulse)
    {
        if (rainSensorTemperature < desiredSensorTemperature)
        {
            globalRainSensorHeater = 100.0;
        }
        else
        {
            // the pulse starts
            pulseStartTime = time(nullptr);
            heatingStatus  = pulse;
        }
    }

    if ((heatingStatus == normal) || (heatingStatus == pulse))
    {
        // Check desired temperature and act accordingly
        // Obtain the difference in temperature and modifier
        float dif             = fabs(desiredSensorTemperature - rainSensorTemperature);
        float refreshModifier = sqrt(WI::UpdatePeriodNP[0].getValue() / 10.0);
        float modifier        = 1;

        if (dif > 8)
        {
            modifier = (1.4 / refreshModifier);
        }
        else if (dif > 4)
        {
            modifier = (1.2 / refreshModifier);
        }
        else if (dif > 3)
        {
            modifier = (1.1 / refreshModifier);
        }
        else if (dif > 2)
        {
            modifier = (1.06 / refreshModifier);
        }
        else if (dif > 1)
        {
            modifier = (1.04 / refreshModifier);
        }
        else if (dif > 0.5)
        {
            modifier = (1.02 / refreshModifier);
        }
        else if (dif > 0.3)
        {
            modifier = (1.01 / refreshModifier);
        }

        if (rainSensorTemperature > desiredSensorTemperature)
        {
            // Lower heating
            //   IDLog("Temp: %f, Desired: %f, Lowering: %f %f -> %f\n", rainSensorTemperature, desiredSensorTemperature, modifier, globalRainSensorHeater, globalRainSensorHeater / modifier);
            globalRainSensorHeater /= modifier;
        }
        else
        {
            // increase heating
            //   IDLog("Temp: %f, Desired: %f, Increasing: %f %f -> %f\n", rainSensorTemperature, desiredSensorTemperature, modifier, globalRainSensorHeater, globalRainSensorHeater * modifier);
            globalRainSensorHeater *= modifier;
        }
    }

    if (globalRainSensorHeater < min)
    {
        globalRainSensorHeater = min;
    }
    if (globalRainSensorHeater > 100.0)
    {
        globalRainSensorHeater = 100.0;
    }

    int rawSensorHeater = int(globalRainSensorHeater * 1023.0 / 100.0);

    cwc->setPWMDutyCycle(rawSensorHeater);

    // Sending heater status to clients
    char *namesSw[3];
    ISState statesSw[3];
    statesSw[0] = ISS_OFF;
    statesSw[1] = ISS_OFF;
    statesSw[2] = ISS_OFF;
    namesSw[0]  = const_cast<char *>("normal");
    namesSw[1]  = const_cast<char *>("increasing");
    namesSw[2]  = const_cast<char *>("pulse");

    if (heatingStatus == normal)
    {
        statesSw[0] = ISS_ON;
    }
    else if (heatingStatus == increasingToPulse)
    {
        statesSw[1] = ISS_ON;
    }
    else if (heatingStatus == pulse)
    {
        statesSw[2] = ISS_ON;
    }

    auto svp = getSwitch("heaterStatus");
    svp.update(statesSw, namesSw, 3);
    svp.setState(IPS_OK);
    svp.apply();
    return true;
}

bool AAGCloudWatcher::sendData()
{
    CloudWatcherData data;

    if (cwc->getAllData(&data) == 0)
        return false;

    auto nvp = getNumber("readings");
    nvp[RAW_SENSOR_SUPPLY].setValue(data.supply);
    nvp[RAW_SENSOR_SKY].setValue(data.sky);
    nvp[RAW_SENSOR_SENSOR].setValue(data.sensor);
    nvp[RAW_SENSOR_TEMP_EST].setValue(data.tempEst);
    nvp[RAW_SENSOR_TEMP_ACT].setValue(data.tempAct);
    nvp[RAW_SENSOR_RAIN].setValue(data.rain);
    nvp[RAW_SENSOR_RAIN_HEATER].setValue(data.rainHeater);
    nvp[RAW_SENSOR_RAIN_TEMPERATURE].setValue(data.rainTemperature);
    nvp[RAW_SENSOR_LDR].setValue(data.ldr);
    nvp[RAW_SENSOR_LIGHT_FREQ].setValue(data.lightFreq);
    nvp[RAW_SENSOR_READ_CYCLES].setValue(data.readCycle);
    lastReadPeriod = data.readCycle;
    nvp[RAW_SENSOR_WIND_SPEED].setValue(data.windSpeed);
    nvp[RAW_SENSOR_RELATIVE_HUMIDITY].setValue(data.humidity);
    nvp[RAW_SENSOR_PRESSURE].setValue(data.pressure);
    nvp[RAW_SENSOR_TOTAL_READINGS].setValue(data.totalReadings);
    nvp.setState(IPS_OK);
    nvp.apply();

    auto nvpE = getNumber("unitErrors");
    nvpE[0].setValue(data.internalErrors);
    nvpE[1].setValue(data.firstByteErrors);
    nvpE[2].setValue(data.commandByteErrors);
    nvpE[3].setValue(data.secondByteErrors);
    nvpE[4].setValue(data.pecByteErrors);
    nvpE.setState(IPS_OK);
    nvpE.apply();

    auto nvpS = getNumber("sensors");

    float skyTemperature = float(data.sky) / 100.0;
    nvpS[SENSOR_INFRARED_SKY].setValue(skyTemperature);
    nvpS[SENSOR_INFRARED_SENSOR].setValue(float(data.sensor) / 100.0);

    nvpS[SENSOR_RAIN_SENSOR].setValue(data.rain);

    float rainSensorTemperature = data.rainTemperature;
    if (rainSensorTemperature > 1022)
    {
        rainSensorTemperature = 1022;
    }
    if (rainSensorTemperature < 1)
    {
        rainSensorTemperature = 1;
    }
    rainSensorTemperature = constants.rainPullUpResistance / ((1023.0 / rainSensorTemperature) - 1.0);
    rainSensorTemperature = log(rainSensorTemperature / constants.rainResistanceAt25);
    rainSensorTemperature =
        1.0 / (rainSensorTemperature / constants.rainBetaFactor + 1.0 / (ABS_ZERO + 25.0)) - ABS_ZERO;

    nvpS[SENSOR_RAIN_SENSOR_TEMPERATURE].setValue(rainSensorTemperature);

    float rainSensorHeater = data.rainHeater;
    rainSensorHeater       = 100.0 * rainSensorHeater / 1023.0;
    nvpS[SENSOR_RAIN_SENSOR_HEATER].setValue(rainSensorHeater);

/*************
    if (constants.sqmStatus < 0) {
	LOG_DEBUG("send correct sqmStatus constant");

        if (m_FirmwareVersion >= 5.89 && data.lightFreq > 0)
	{
	    constants.sqmStatus = 1;
	}
	else
	{
	    constants.sqmStatus = 0;
	}
        sendConstants();
    }
*************/


    float ambientTemperature = data.tempEst;
    float ambientLight = data.ldr;

    nvpS[SENSOR_BRIGHTNESS_SENSOR].setValue(ambientLight);

    if( data.lightFreq > 0 )
    {
        double sqm = ( 250000.0 / double(data.lightFreq) );

	auto nvpSqmLimit = getNumber("sqmLimit");
	float sqmLimit   = getNumberValueFromVector(nvpSqmLimit, "sqmLimit");

        sqm = sqmLimit - 2.5 * log10( sqm );

        ambientLight = float( sqm );
    }
    else
    {
        ambientLight = float(data.ldr);
        if (ambientLight > 1022.0)
        {
            ambientLight = 1022.0;
        }
        if (ambientLight < 1)
        {
            ambientLight = 1.0;
        }
        ambientLight = constants.ldrPullUpResistance / ((1023.0 / ambientLight) - 1.0);
    }
    nvpS[SENSOR_BRIGHTNESS_SQM].setValue(ambientLight);

    if (ambientTemperature == -10000)
    {
        ambientTemperature = float(data.sensor) / 100.0;
    }
    else
    {
        if (ambientTemperature > 1022)
        {
            ambientTemperature = 1022;
        }
        if (ambientTemperature < 1)
        {
            ambientTemperature = 1;
        }
        ambientTemperature = constants.ambientPullUpResistance / ((1023.0 / ambientTemperature) - 1.0);
        ambientTemperature = log(ambientTemperature / constants.ambientResistanceAt25);
        ambientTemperature =
            1.0 / (ambientTemperature / constants.ambientBetaFactor + 1.0 / (ABS_ZERO + 25.0)) - ABS_ZERO;
    }

    nvpS[SENSOR_AMBIENT_TEMPERATURE_SENSOR].setValue(ambientTemperature);

    auto nvpSky = getNumber("skyCorrection");
    float k1    = getNumberValueFromVector(nvpSky, "k1");
    float k2    = getNumberValueFromVector(nvpSky, "k2");
    float k3    = getNumberValueFromVector(nvpSky, "k3");
    float k4    = getNumberValueFromVector(nvpSky, "k4");
    float k5    = getNumberValueFromVector(nvpSky, "k5");

    float correctedTemperature =
        skyTemperature - ((k1 / 100.0) * (ambientTemperature - k2 / 10.0) +
                          (k3 / 100.0) * pow(exp(k4 / 1000 * ambientTemperature), (k5 / 100.0)));

    nvpS[SENSOR_CORRECTED_INFRARED_SKY].setValue(correctedTemperature);
    nvpS[SENSOR_WIND_SPEED].setValue(data.windSpeed);
    nvpS[SENSOR_RELATIVE_HUMIDITY].setValue(data.humidity);
    nvpS[SENSOR_PRESSURE].setValue(data.abspress);
    nvpS[SENSOR_RELATIVE_PRESSURE].setValue(data.relpress);
    nvpS.setState(IPS_OK);
    nvpS.apply();

    auto svpSw = getSwitch("deviceSwitch");
    svpSw[0].setState((data.switchStatus == 1) ? ISS_OFF : ISS_ON);
    svpSw[1].setState((data.switchStatus == 1) ? ISS_ON : ISS_OFF);
    svpSw.setState(IPS_OK);
    svpSw.apply();

    setParameterValue("WEATHER_CLOUD", correctedTemperature);
    setParameterValue("WEATHER_RAIN", data.rain);

    if (constants.anemometerStatus)
    {
        setParameterValue("WEATHER_WIND_SPEED", data.windSpeed);
    }
    else
    {
        setParameterValue("WEATHER_WIND_SPEED", 0);
    }

    if (data.humidity > 0)
        setParameterValue("WEATHER_HUMIDITY", data.humidity);
    if (ambientLight > 0)
	setParameterValue("WEATHER_SQM", ambientLight);

    return true;
}

double AAGCloudWatcher::getNumberValueFromVector(INumberVectorProperty *nvp, const char *name)
{
    for (int i = 0; i < nvp->nnp; i++)
    {
        if (strcmp(name, nvp->np[i].name) == 0)
        {
            return nvp->np[i].value;
        }
    }
    return 0;
}

double AAGCloudWatcher::getNumberValueFromVector(INDI::PropertyNumber nvp, const char *name)
{
    auto widget = nvp.findWidgetByName(name);
    return widget ? widget->getValue() : 0;
}

bool AAGCloudWatcher::resetData()
{
    CloudWatcherData data;

    int r = cwc->getAllData(&data);

    if (!r)
    {
        return false;
    }

    const int N_DATA = 11;
    double values[N_DATA];
    char *names[N_DATA];

    names[0]  = const_cast<char *>("supply");
    values[0] = 0;

    names[1]  = const_cast<char *>("sky");
    values[1] = 0;

    names[2]  = const_cast<char *>("sensor");
    values[2] = 0;

    names[3]  = const_cast<char *>("ambient");
    values[3] = 0;

    names[4]  = const_cast<char *>("rain");
    values[4] = 0;

    names[5]  = const_cast<char *>("rainHeater");
    values[5] = 0;

    names[6]  = const_cast<char *>("rainTemp");
    values[6] = 0;

    names[7]  = const_cast<char *>("LDR");
    values[7] = 0;

    names[8]  = const_cast<char *>("readCycle");
    values[8] = 0;

    names[9]  = const_cast<char *>("windSpeed");
    values[9] = 0;

    names[10]  = const_cast<char *>("totalReadings");
    values[10] = 0;

    auto nvp = getNumber("readings");
    nvp.update(values, names, N_DATA);
    nvp.setState(IPS_IDLE);
    nvp.apply();

    const int N_ERRORS = 5;
    double valuesE[N_ERRORS];
    char *namesE[N_ERRORS];

    namesE[0]  = const_cast<char *>("internalErrors");
    valuesE[0] = 0;

    namesE[1]  = const_cast<char *>("firstAddressByteErrors");
    valuesE[1] = 0;

    namesE[2]  = const_cast<char *>("commandByteErrors");
    valuesE[2] = 0;

    namesE[3]  = const_cast<char *>("secondAddressByteErrors");
    valuesE[3] = 0;

    namesE[4]  = const_cast<char *>("pecByteErrors");
    valuesE[4] = 0;

    auto nvpE = getNumber("unitErrors");
    nvpE.update(valuesE, namesE, N_ERRORS);
    nvpE.setState(IPS_IDLE);
    nvpE.apply();

    const int N_SENS = 10;
    double valuesS[N_SENS];
    char *namesS[N_SENS];

    namesS[0]  = const_cast<char *>("infraredSky");
    valuesS[0] = 0.0;

    namesS[1]  = const_cast<char *>("infraredSensor");
    valuesS[1] = 0.0;

    namesS[2]  = const_cast<char *>("rainSensor");
    valuesS[2] = 0.0;

    namesS[3]  = const_cast<char *>("rainSensorTemperature");
    valuesS[3] = 0.0;

    namesS[4]  = const_cast<char *>("rainSensorHeater");
    valuesS[4] = 0.0;

    namesS[5]  = const_cast<char *>("brightnessSensor");
    valuesS[5] = 0.0;

    setParameterValue("WEATHER_SQM", 0);

    namesS[6]  = const_cast<char *>("correctedInfraredSky");
    valuesS[6] = 0.0;

    namesS[7]  = const_cast<char *>("ambientTemperatureSensor");
    valuesS[7] = 0.0;

    namesS[8]  = const_cast<char *>("windSpeed");
    valuesS[8] = 0.0;

    namesS[9]  = const_cast<char *>("pressure");
    valuesS[9] = 0.0;

    setParameterValue("WEATHER_WIND_SPEED", 0);

    auto nvpS = getNumber("sensors");
    nvpS.update(valuesS, namesS, N_SENS);
    nvpS.setState(IPS_IDLE);
    nvpS.apply();

    auto svpSw = getSwitch("deviceSwitch");
    svpSw.setState(IPS_IDLE);
    svpSw.apply();

    auto svpRC = getSwitch("rainConditions");
    svpRC.setState(IPS_IDLE);
    svpRC.apply();

    auto svp = getSwitch("heaterStatus");
    svp.setState(IPS_IDLE);
    svp.apply();
    return true;
}

bool AAGCloudWatcher::sendConstants()
{
    auto nvp = getNumber("constants");
    auto tvp = getText("FW");

    int r = cwc->getConstants(&constants);

    if (!r)
    {
        return false;
    }

    m_FirmwareVersion = constants.firmwareVersion;

    const int N_CONSTANTS = 12;
    double values[N_CONSTANTS];
    char *names[N_CONSTANTS];

    names[0]  = const_cast<char *>("internalSerialNumber");
    values[0] = constants.internalSerialNumber;

    names[1]  = const_cast<char *>("zenerVoltage");
    values[1] = constants.zenerVoltage;

    names[2]  = const_cast<char *>("LDRMaxResistance");
    values[2] = constants.ldrMaxResistance;

    names[3]  = const_cast<char *>("LDRPullUpResistance");
    values[3] = constants.ldrPullUpResistance;

    names[4]  = const_cast<char *>("rainBetaFactor");
    values[4] = constants.rainBetaFactor;

    names[5]  = const_cast<char *>("rainResistanceAt25");
    values[5] = constants.rainResistanceAt25;

    names[6]  = const_cast<char *>("rainPullUpResistance");
    values[6] = constants.rainPullUpResistance;

    names[7]  = const_cast<char *>("ambientBetaFactor");
    values[7] = constants.ambientBetaFactor;

    names[8]  = const_cast<char *>("ambientResistanceAt25");
    values[8] = constants.ambientResistanceAt25;

    names[9]  = const_cast<char *>("ambientPullUpResistance");
    values[9] = constants.ambientPullUpResistance;

    names[10]  = const_cast<char *>("anemometerStatus");
    values[10] = constants.anemometerStatus;

    names[11]  = const_cast<char *>("sqmStatus");
    values[11] = constants.sqmStatus;

    nvp.update(values, names, N_CONSTANTS);
    nvp.setState(IPS_OK);
    nvp.apply();

    char version[8] = {0};
    snprintf(version, 8, "%.2f", m_FirmwareVersion);
    tvp[0].setText(version);
    tvp.setState(IPS_OK);
    tvp.apply();
    return true;
}

/*
  bool AAGCloudWatcher::resetConstants()
  {
  auto nvp = getNumber("constants");

  auto tvp = getText("FW");

  CloudWatcherConstants constants;

  int r = cwc->getConstants(&constants);

  if (!r)
  {
  return false;
  }

  const int N_CONSTANTS = 12;
  double values[N_CONSTANTS];
  char *names[N_CONSTANTS];

  names[0]  = const_cast<char *>("internalSerialNumber");
  values[0] = 0;

  names[1]  = const_cast<char *>("zenerVoltage");
  values[1] = 0;

  names[2]  = const_cast<char *>("LDRMaxResistance");
  values[2] = 0;

  names[3]  = const_cast<char *>("LDRPullUpResistance");
  values[3] = 0;

  names[4]  = const_cast<char *>("rainBetaFactor");
  values[4] = 0;

  names[5]  = const_cast<char *>("rainResistanceAt25");
  values[5] = 0;

  names[6]  = const_cast<char *>("rainPullUpResistance");
  values[6] = 0;

  names[7]  = const_cast<char *>("ambientBetaFactor");
  values[7] = 0;

  names[8]  = const_cast<char *>("ambientResistanceAt25");
  values[8] = 0;

  names[9]  = const_cast<char *>("ambientPullUpResistance");
  values[9] = 0;

  names[10]  = const_cast<char *>("anemometerStatus");
  values[10] = 0;

  names[11]  = const_cast<char *>("sqmStatus");
  values[11] = 0;

  nvp.update(values, names, N_CONSTANTS);
  nvp.setState(IPS_IDLE);
  nvp.apply();

  char *valuesT[1];
  char *namesT[1];

  namesT[0]  = const_cast<char *>("firmwareVersion");
  valuesT[0] = const_cast<char *>("-");

  tvp.update(valuesT, namesT, 1);
  tvp.setState(IPS_IDLE);
  tvp.apply();
  return true;
  }
*/

const char *AAGCloudWatcher::getDefaultName()
{
    return "AAG Cloud Watcher NG";
}
