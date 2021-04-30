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
    int check = cwc->checkCloudWatcher();

    if (check)
    {
        LOG_INFO("Connected to AAG Cloud Watcher");

        sendConstants();

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

    addParameter("WEATHER_BRIGHTNESS", "Ambient light brightness (K)", 2100, 1000000, 10);
    addParameter("WEATHER_WIND_SPEED", "Wind speed (Km/H)", 0, 30, 10);
    addParameter("WEATHER_RAIN", "Rain (cycles)", 2000, 10000, 10);
    addParameter("WEATHER_CLOUD", "Cloud (corrected infrared sky temperature °C)", -5, 10, 10);

    setCriticalParameter("WEATHER_BRIGHTNESS");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN");
    setCriticalParameter("WEATHER_CLOUD");

    addDebugControl();

    return true;
}

IPState AAGCloudWatcher::updateWeather()
{
    if (!sendData())
    {
        LOG_ERROR("Can not get data from device");
        return IPS_ALERT;
    }

    heatingAlgorithm();

    return IPS_OK;
}

/*************************************************************************
** Define Basic properties to clients.
*************************************************************************/
void AAGCloudWatcher::ISGetProperties(const char *dev)
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

/*****************************************************************************
** Process Text properties
*****************************************************************************/
bool AAGCloudWatcher::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool AAGCloudWatcher::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI::Weather::ISNewNumber(dev, name, values, names, n);

    // Ignore if not ours
    if (strcmp(dev, getDefaultName()))
    {
        return false;
    }

    INumberVectorProperty *nvp = getNumber(name);

    if (!nvp)
    {
        return false;
    }

    if (!strcmp(nvp->name, "heaterParameters"))
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

        IUUpdateNumber(nvp, values, names, n);
        nvp->s = IPS_OK;
        IDSetNumber(nvp, nullptr);

        return true;
    }

    if (!strcmp(nvp->name, "skyCorrection"))
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

        IUUpdateNumber(nvp, values, names, n);
        nvp->s = IPS_OK;
        IDSetNumber(nvp, nullptr);

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

    ISwitchVectorProperty *svp = getSwitch(name);

    if (!svp)
    {
        return false;
    }

    int error = 0;
    if (!strcmp(svp->name, "deviceSwitch"))
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

        IUUpdateSwitch(svp, statesSw, namesSw, 2);
        if (error)
        {
            svp->s = IPS_IDLE;
        }
        else
        {
            svp->s = IPS_OK;
        }
        IDSetSwitch(svp, nullptr);

        return true;
    }

    if (!strcmp(svp->name, "anemometerType"))
    {
        IUUpdateSwitch(svp, states, names, 2);
        svp->s = IPS_OK;

        ISwitch *sp = IUFindSwitch(svp, "BLACK");
        if (sp->s == ISS_ON)
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

float AAGCloudWatcher::getRefreshPeriod()
{
    // XXX: The WEATHER_UPDATE property is defined / deleted when connection status changes, so we just retrieve it from here.
    return UpdatePeriodN[0].value;
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
    INumberVectorProperty *heaterParameters = getNumber("heaterParameters");
    float tempLow                           = getNumberValueFromVector(heaterParameters, "tempLow");
    float tempHigh                          = getNumberValueFromVector(heaterParameters, "tempHigh");
    float deltaLow                          = getNumberValueFromVector(heaterParameters, "deltaLow");
    float deltaHigh                         = getNumberValueFromVector(heaterParameters, "deltaHigh");
    float heatImpulseTemp                   = getNumberValueFromVector(heaterParameters, "heatImpulseTemp");
    float heatImpulseDuration               = getNumberValueFromVector(heaterParameters, "heatImpulseDuration");
    float heatImpulseCycle                  = getNumberValueFromVector(heaterParameters, "heatImpulseCycle");
    float min                               = getNumberValueFromVector(heaterParameters, "min");

    INumberVectorProperty *sensors = getNumber("sensors");
    float ambient                  = getNumberValueFromVector(sensors, "ambientTemperatureSensor");
    float rainSensorTemperature    = getNumberValueFromVector(sensors, "rainSensorTemperature");

    float refresh = getRefreshPeriod();

    // XXX FIXME: when the automatic refresh is disabled the refresh period is set to 0, however we can be called in a manual fashion.
    // this is needed as we divide by refresh later...
    if (refresh < 3)
    {
        refresh = 3;
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
        float refreshModifier = sqrt(refresh / 10.0);
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

    ISwitchVectorProperty *svp = getSwitch("heaterStatus");
    IUUpdateSwitch(svp, statesSw, namesSw, 3);
    svp->s = IPS_OK;
    IDSetSwitch(svp, nullptr);
    return true;
}

bool AAGCloudWatcher::sendData()
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
    values[0] = data.supply;

    names[1]  = const_cast<char *>("sky");
    values[1] = data.sky;

    names[2]  = const_cast<char *>("sensor");
    values[2] = data.sensor;

    names[3]  = const_cast<char *>("ambient");
    values[3] = data.ambient;

    names[4]  = const_cast<char *>("rain");
    values[4] = data.rain;

    names[5]  = const_cast<char *>("rainHeater");
    values[5] = data.rainHeater;

    names[6]  = const_cast<char *>("rainTemp");
    values[6] = data.rainTemperature;

    names[7]  = const_cast<char *>("LDR");
    values[7] = data.ldr;

    names[8]       = const_cast<char *>("readCycle");
    values[8]      = data.readCycle;
    lastReadPeriod = data.readCycle;

    names[9]  = const_cast<char *>("windSpeed");
    values[9] = data.windSpeed;

    names[10]  = const_cast<char *>("totalReadings");
    values[10] = data.totalReadings;

    INumberVectorProperty *nvp = getNumber("readings");
    IUUpdateNumber(nvp, values, names, N_DATA);
    nvp->s = IPS_OK;
    IDSetNumber(nvp, nullptr);

    const int N_ERRORS = 5;
    double valuesE[N_ERRORS];
    char *namesE[N_ERRORS];

    namesE[0]  = const_cast<char *>("internalErrors");
    valuesE[0] = data.internalErrors;

    namesE[1]  = const_cast<char *>("firstAddressByteErrors");
    valuesE[1] = data.firstByteErrors;

    namesE[2]  = const_cast<char *>("commandByteErrors");
    valuesE[2] = data.commandByteErrors;

    namesE[3]  = const_cast<char *>("secondAddressByteErrors");
    valuesE[3] = data.secondByteErrors;

    namesE[4]  = const_cast<char *>("pecByteErrors");
    valuesE[4] = data.pecByteErrors;

    INumberVectorProperty *nvpE = getNumber("unitErrors");
    IUUpdateNumber(nvpE, valuesE, namesE, N_ERRORS);
    nvpE->s = IPS_OK;
    IDSetNumber(nvpE, nullptr);

    const int N_SENS = 9;
    double valuesS[N_SENS];
    char *namesS[N_SENS];

    float skyTemperature = float(data.sky) / 100.0;
    namesS[0]            = const_cast<char *>("infraredSky");
    valuesS[0]           = skyTemperature;

    namesS[1]  = const_cast<char *>("infraredSensor");
    valuesS[1] = float(data.sensor) / 100.0;

    namesS[2]  = const_cast<char *>("rainSensor");
    valuesS[2] = data.rain;

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

    namesS[3]  = const_cast<char *>("rainSensorTemperature");
    valuesS[3] = rainSensorTemperature;

    float rainSensorHeater = data.rainHeater;
    rainSensorHeater       = 100.0 * rainSensorHeater / 1023.0;
    namesS[4]              = const_cast<char *>("rainSensorHeater");
    valuesS[4]             = rainSensorHeater;

    float ambientLight = float(data.ldr);
    if (ambientLight > 1022.0)
    {
        ambientLight = 1022.0;
    }
    if (ambientLight < 1)
    {
        ambientLight = 1.0;
    }
    ambientLight = constants.ldrPullUpResistance / ((1023.0 / ambientLight) - 1.0);
    namesS[5]    = const_cast<char *>("brightnessSensor");
    valuesS[5]   = ambientLight;

    setParameterValue("WEATHER_BRIGHTNESS", ambientLight);

    float ambientTemperature = data.ambient;

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

    namesS[6]  = const_cast<char *>("ambientTemperatureSensor");
    valuesS[6] = ambientTemperature;

    INumberVectorProperty *nvpSky = getNumber("skyCorrection");
    float k1                      = getNumberValueFromVector(nvpSky, "k1");
    float k2                      = getNumberValueFromVector(nvpSky, "k2");
    float k3                      = getNumberValueFromVector(nvpSky, "k3");
    float k4                      = getNumberValueFromVector(nvpSky, "k4");
    float k5                      = getNumberValueFromVector(nvpSky, "k5");

    float correctedTemperature =
        skyTemperature - ((k1 / 100.0) * (ambientTemperature - k2 / 10.0) +
                          (k3 / 100.0) * pow(exp(k4 / 1000 * ambientTemperature), (k5 / 100.0)));

    namesS[7]  = const_cast<char *>("correctedInfraredSky");
    valuesS[7] = correctedTemperature;

    namesS[8]  = const_cast<char *>("windSpeed");
    valuesS[8] = data.windSpeed;

    INumberVectorProperty *nvpS = getNumber("sensors");
    IUUpdateNumber(nvpS, valuesS, namesS, N_SENS);
    nvpS->s = IPS_OK;
    IDSetNumber(nvpS, nullptr);

    ISState states[2];
    char *namesSw[2];
    namesSw[0] = const_cast<char *>("open");
    namesSw[1] = const_cast<char *>("close");
    //IDLog("%d\n", data.switchStatus);
    if (data.switchStatus == 1)
    {
        states[0] = ISS_OFF;
        states[1] = ISS_ON;
    }
    else
    {
        states[0] = ISS_ON;
        states[1] = ISS_OFF;
    }

    ISwitchVectorProperty *svpSw = getSwitch("deviceSwitch");
    IUUpdateSwitch(svpSw, states, namesSw, 2);
    svpSw->s = IPS_OK;
    IDSetSwitch(svpSw, nullptr);

    //IDLog("%d\n", data.switchStatus);

    setParameterValue("WEATHER_CLOUD", correctedTemperature);
    setParameterValue("WEATHER_RAIN", data.rain);

    INumberVectorProperty *consts = getNumber("constants");
    int anemometerStatus          = getNumberValueFromVector(consts, "anemometerStatus");

    //IDLog("%d\n", data.switchStatus);

    if (anemometerStatus)
    {
        setParameterValue("WEATHER_WIND_SPEED", data.windSpeed);
    }
    else
    {
        setParameterValue("WEATHER_WIND_SPEED", 0);
    }
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

    INumberVectorProperty *nvp = getNumber("readings");
    IUUpdateNumber(nvp, values, names, N_DATA);
    nvp->s = IPS_IDLE;
    IDSetNumber(nvp, nullptr);

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

    INumberVectorProperty *nvpE = getNumber("unitErrors");
    IUUpdateNumber(nvpE, valuesE, namesE, N_ERRORS);
    nvpE->s = IPS_IDLE;
    IDSetNumber(nvpE, nullptr);

    const int N_SENS = 9;
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
    setParameterValue("WEATHER_BRIGHTNESS", 0);

    namesS[6]  = const_cast<char *>("ambientTemperatureSensor");
    valuesS[6] = 0.0;

    namesS[7]  = const_cast<char *>("correctedInfraredSky");
    valuesS[7] = 0.0;

    namesS[6]  = const_cast<char *>("windSpeed");
    valuesS[6] = 0.0;
    setParameterValue("WEATHER_WIND_SPEED", 0);

    INumberVectorProperty *nvpS = getNumber("sensors");
    IUUpdateNumber(nvpS, valuesS, namesS, N_SENS);
    nvpS->s = IPS_IDLE;
    IDSetNumber(nvpS, nullptr);

    ISwitchVectorProperty *svpSw = getSwitch("deviceSwitch");
    svpSw->s                     = IPS_IDLE;
    IDSetSwitch(svpSw, nullptr);

    ISwitchVectorProperty *svpRC = getSwitch("rainConditions");
    svpRC->s                     = IPS_IDLE;
    IDSetSwitch(svpRC, nullptr);

    ISwitchVectorProperty *svp = getSwitch("heaterStatus");
    svp->s                     = IPS_IDLE;
    IDSetSwitch(svp, nullptr);
    return true;
}

bool AAGCloudWatcher::sendConstants()
{
    INumberVectorProperty *nvp = getNumber("constants");

    ITextVectorProperty *tvp = getText("FW");

    int r = cwc->getConstants(&constants);

    if (!r)
    {
        return false;
    }

    const int N_CONSTANTS = 11;
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

    IUUpdateNumber(nvp, values, names, N_CONSTANTS);
    nvp->s = IPS_OK;
    IDSetNumber(nvp, nullptr);

    char *valuesT[1];
    char *namesT[1];

    namesT[0]  = const_cast<char *>("firmwareVersion");
    valuesT[0] = constants.firmwareVersion;

    IUUpdateText(tvp, valuesT, namesT, 1);
    tvp->s = IPS_OK;
    IDSetText(tvp, nullptr);
    return true;
}

bool AAGCloudWatcher::resetConstants()
{
    INumberVectorProperty *nvp = getNumber("constants");

    ITextVectorProperty *tvp = getText("FW");

    CloudWatcherConstants constants;

    int r = cwc->getConstants(&constants);

    if (!r)
    {
        return false;
    }

    const int N_CONSTANTS = 11;
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

    IUUpdateNumber(nvp, values, names, N_CONSTANTS);
    nvp->s = IPS_IDLE;
    IDSetNumber(nvp, nullptr);

    char *valuesT[1];
    char *namesT[1];

    namesT[0]  = const_cast<char *>("firmwareVersion");
    valuesT[0] = const_cast<char *>("-");

    IUUpdateText(tvp, valuesT, namesT, 1);
    tvp->s = IPS_IDLE;
    IDSetText(tvp, nullptr);
    return true;
}

const char *AAGCloudWatcher::getDefaultName()
{
    return "AAG Cloud Watcher NG";
}
