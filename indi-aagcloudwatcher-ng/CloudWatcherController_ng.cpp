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

#include "CloudWatcherController_ng.h"

#include "indicom.h"
#include "indiweather.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>

#include <limits.h>


#define READ_TIMEOUT 5

/******************************************************************/
/* PUBLIC MEMBERS                                                */
/******************************************************************/

CloudWatcherController::CloudWatcherController()
{
}

CloudWatcherController::CloudWatcherController(bool verbose) : verbose(verbose)
{
}

const char *CloudWatcherController::getDeviceName()
{
    return "AAG Cloud Watcher NG";
}

void CloudWatcherController::setPortFD(int newPortFD)
{
    PortFD = newPortFD;
}

void CloudWatcherController::setAnemometerType(enum ANEMOMETER_TYPE type)
{
    anemometerType = type;
}

void CloudWatcherController::setElevation(float elevation)
{
    siteElevation = elevation;
}

bool CloudWatcherController::getAllData(CloudWatcherData *cwd)
{
    auto check = false;

    totalReadings++;

    timeval begin;
    gettimeofday(&begin, nullptr);

    int skyTemperature[NUMBER_OF_READS] = {0};
    int sensorTemperature[NUMBER_OF_READS] = {0};
    int rainFrequency[NUMBER_OF_READS] = {0};
    int internalSupplyVoltage[NUMBER_OF_READS] = {0};
    float tempEstimate[NUMBER_OF_READS] = {0};
    int ldrValue[NUMBER_OF_READS] = {0};
    int lightFreq[NUMBER_OF_READS] = {0};
    int rainSensorTemperature[NUMBER_OF_READS] = {0};
    float wind[NUMBER_OF_READS] = {0};
    float temperature[NUMBER_OF_READS] = {0};
    float humidity[NUMBER_OF_READS] = {0};
    float pressure[NUMBER_OF_READS] = {0};

    for (int i = 0; i < NUMBER_OF_READS; i++)
    {
        check = getIRSkyTemperature(skyTemperature[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getIRSkyTemperature" );
            return false;
        }

        check = getIRSensorTemperature(sensorTemperature[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getIRSensorTemperature" );
            return false;
        }

        check = getRainFrequency(rainFrequency[i]);
        if (!check)
        {
            LOG_ERROR( "ERROR in getIRSensorTemperature" );
            return false;
        }

        check = getValues(&internalSupplyVoltage[i], &tempEstimate[i], &ldrValue[i], &lightFreq[i],
                          &rainSensorTemperature[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getValues" );
            return false;
        }

        check = getWindSpeed(wind[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getWindSpeed" );
            return false;
        }

        check = getTemperature(temperature[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getTemperature" );
            return false;
        }

        check = getHumidity(humidity[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getHumidity" );
            return false;
        }

        check = getPressure(pressure[i]);

        if (!check)
        {
            LOG_ERROR( "ERROR in getPressure" );
            return false;
        }
    }

    cwd->sky             = aggregateInts(skyTemperature, NUMBER_OF_READS);
    cwd->sensor          = aggregateInts(sensorTemperature, NUMBER_OF_READS);
    cwd->rain            = aggregateInts(rainFrequency, NUMBER_OF_READS);
    cwd->supply          = aggregateInts(internalSupplyVoltage, NUMBER_OF_READS);
    cwd->tempEst         = aggregateFloats(tempEstimate, NUMBER_OF_READS); // not really present since firmware 3.x.x
    cwd->ldr             = aggregateInts(ldrValue, NUMBER_OF_READS);
    cwd->lightFreq       = aggregateInts(lightFreq, NUMBER_OF_READS);
    cwd->rainTemperature = aggregateInts(rainSensorTemperature, NUMBER_OF_READS);
    cwd->windSpeed       = aggregateFloats(wind, NUMBER_OF_READS);
    cwd->windSpeed       = aggregateFloats(wind, NUMBER_OF_READS);
    cwd->tempAct         = aggregateFloats(temperature, NUMBER_OF_READS);
    cwd->humidity        = aggregateFloats(humidity, NUMBER_OF_READS);
    cwd->pressure        = aggregateFloats(pressure, NUMBER_OF_READS);


    if (m_FirmwareVersion >= 5.8)
    {
        float press = cwd->pressure; // raw pressure

        if (press > 0)
        {
            press /= 16;

            cwd->abspress = press;

            float temp = cwd->tempAct; // in Celsius
            float elev = siteElevation;
            float relpress;

            if (temp < -999 || temp > 999)
            {
                relpress = 0;
            }
            else
            {
                elev *= 0.0065;
                relpress = press * powf((1 - (elev / (temp + elev + 273.15))), -5.275); // unit is hPa (hectopascal) or millbars
            }

            cwd->relpress = relpress;
        }
        else
        {
            cwd->abspress = 0;
            cwd->relpress = 0;
        }
    }
    else
    {
        cwd->abspress = 0;
        cwd->relpress = 0;
    }

    check = getIRErrors(&cwd->firstByteErrors, &cwd->commandByteErrors, &cwd->secondByteErrors, &cwd->pecByteErrors);

    if (!check)
    {
        LOG_DEBUG( "ERROR in getIRErrors" );
        return false;
    }

    cwd->internalErrors = cwd->firstByteErrors + cwd->commandByteErrors + cwd->secondByteErrors + cwd->pecByteErrors;

    check = getPWMDutyCycle(cwd->rainHeater);

    if (!check)
    {
        LOG_DEBUG( "ERROR in getPWMDutyCycle" );
        return false;
    }

    check = getSwitchStatus(&cwd->switchStatus);

    if (!check)
    {
        LOG_DEBUG( "ERROR in getSwitchStatus" );
        return false;
    }

    timeval end;
    gettimeofday(&end, nullptr);

    float rc = float(end.tv_sec - begin.tv_sec) + float(end.tv_usec - begin.tv_usec) / 1000000.0;

    cwd->readCycle = rc;

    cwd->totalReadings   = totalReadings;

    return true;
}

bool CloudWatcherController::getConstants(CloudWatcherConstants *cwc)
{
    bool r = getFirmwareVersion(m_FirmwareVersion);

    if (!r)
    {
        return false;
    }

    cwc->firmwareVersion = m_FirmwareVersion;
    LOGF_DEBUG( "firmware version is %f", cwc->firmwareVersion);

    r = getSerialNumber(cwc->internalSerialNumber);

    if (!r)
    {
        LOG_DEBUG( "could not get internal serial number");
        return false;
    }

    if (m_FirmwareVersion >= 3)
    {
        r = getElectricalConstants();

        if (!r)
        {
            LOG_DEBUG( "could not get electrical constants");
            return false;
        }
    }

    cwc->zenerVoltage            = zenerConstant;
    cwc->ldrMaxResistance        = LDRMaxResistance;
    cwc->ldrPullUpResistance     = LDRPullUpResistance;
    cwc->rainBetaFactor          = rainBeta;
    cwc->rainResistanceAt25      = rainResAt25;
    cwc->rainPullUpResistance    = rainPullUpResistance;
    cwc->ambientBetaFactor       = ambBeta;
    cwc->ambientResistanceAt25   = ambResAt25;
    cwc->ambientPullUpResistance = ambPullUpResistance;

    getAnemometerStatus(cwc->anemometerStatus);
    LOGF_DEBUG( "anemometer status = %i", cwc->anemometerStatus);

    getSqmStatus(cwc->sqmStatus);
    LOGF_DEBUG( "SQM status = %i", cwc->sqmStatus);

    return true;
}

/******************************************************************/
/* Start of CloudWatcher Command-Related Serial Port Functions    */
/******************************************************************/
// The order of the function definitions mirror the order in which they appear in the Lunatico Astro Comms docs, which are:
//   Rs232_Comms_v100.pdf
//   Rs232_Comms_v110.pdf
//   Rs232_Comms_v120.pdf
//   Rs232_Comms_v130.pdf
//   Rs232_Comms_v140.pdf
// Hopefully that makes it easier to confirm function implementations (stare and compare). Start of PDF is nothed.
//
// Since some are public and some are private, we deviate from how the functions appear in the header file so that
// we can better track the documentation. Initial comments clarify public and private. Most are private.

/******************************************************/
/* CW Cmd funtions from Rs232_Comms_v100.pdf Document */
/******************************************************/
bool CloudWatcherController::checkCloudWatcher() // CW Internal Name Cmd: A! (public)
{
    sendCloudwatcherCommand("A!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    std::string internalNameBlock = "!N CloudWatcher!";
    std::string pocketNameBlock = "!N PocketCW!";
    std::string detectedName = std::string(inputBuffer);
    LOGF_DEBUG( "Detected name is %s", detectedName.c_str());

    return (detectedName == internalNameBlock || detectedName == pocketNameBlock);
}

// N.B. Document Rs232_Comms_v130.pdf updates the information in Rs232_Comms_v100.pdf (code below reflects latest update)
bool CloudWatcherController::getFirmwareVersion(double &version) // CW Firmware Version Cmd: B! (private)
{
    if (m_FirmwareVersion == 0)
    {
        char fw[8] = {0};

        sendCloudwatcherCommand("B!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int res = sscanf(inputBuffer, "!V         %s!", fw);

        if (res != 1)
        {
            LOGF_DEBUG("firmware answer did not scan: '%s'", inputBuffer);
            return false;
        }

        try
        {
            m_FirmwareVersion = std::stod(fw);
            version = m_FirmwareVersion;
        }
        catch (...)
        {
            LOGF_DEBUG("firmware std::stod conversion error: '%s'", fw);
            return false;
        }
    }

    return true;
}

// N.B. Documents Rs232_Comms_v110.pdf and Rs232_Comms_v140.pdf update the information in Rs232_Comms_v100.pdf (code below reflects latest updates)
bool CloudWatcherController::getValues(int *internalSupplyVoltage, float *tempEstimate, int *ldrValue,
                                       int *lightFreq,
                                       int *rainSensorTemperature) // CW Get Values Cmd: C! (private)
{
    sendCloudwatcherCommand("C!");

    static const int MAX_GV_BLOCKS = 6; // as of firmware 5.89, answer can be up to 90 characters (6 blocks)

    int zenerV = 0;
    float estTemp = -10000;
    int ldrRes = 0;
    int rainSensTemp = 0;
    int ltFreq = 0;

    // C! special case because (1) variable number of responses; (2) order or responses can vary
    // thus processing is different than usual

retry:
    {
        char inputBuffer[BLOCK_SIZE * MAX_GV_BLOCKS] = {0};
        int  blocks = 0;

        int rc = -1;
        int n = 0;
        int nb = 0;

        for (int i = 0, j = 0;  j < MAX_GV_BLOCKS; i += BLOCK_SIZE, j++, nb = 0)
        {
            if ((rc = tty_read(PortFD, &inputBuffer[i], BLOCK_SIZE, READ_TIMEOUT, &nb)) != TTY_OK || nb != BLOCK_SIZE)
            {
                if (rc == TTY_OK)
                {
                    LOGF_ERROR("%s read error[%i]: byte count != block size (%i != %i)", __FUNCTION__, rc, nb, BLOCK_SIZE);
                }
                else
                {
                    char errstr[MAXRBUF];
                    tty_error_msg(rc, errstr, MAXRBUF);
                    LOGF_ERROR("%s read error[%i]: %s", __FUNCTION__, rc, errstr);
                }
                return false;
            }
            n += nb;
            if (checkValidMessage(&inputBuffer[i], 1, nb, false))
            {
                break;
            }
        }

        if( inputBuffer[0] == '!' && ( inputBuffer[1] == 'f' || inputBuffer[1] == 'd' ) )
        {
            LOGF_DEBUG( "skip answer %s %i", inputBuffer, n );
            goto retry;
        }

        // n is bytes read and in this case is always a multiple of BLOCK_SIZE (if valid)
        if (n > 0 && n % BLOCK_SIZE == 0)
        {
            blocks = n / BLOCK_SIZE;
            auto valid = checkValidMessage(inputBuffer, blocks, n, false);
            LOGF_DEBUG( "getValues: [%s,%i] = %s", inputBuffer, blocks, valid ? "valid" : "invalid" );

            if (!valid)
                return false;
        }
        else
        {
            LOGF_DEBUG( "getValues: [%s,%i] = incomplete", inputBuffer, n);

            return false;
        }

        for (int i = 0;  i < n; i += BLOCK_SIZE)
        {
            if (inputBuffer[i] == '!')
            {
                switch(inputBuffer[i + 1])
                {
                    case '3': // ambient temperature
                        estTemp = std::stof(&inputBuffer[i + 2]);
                        break;
                    case '4': // LDR (light-dependent resistor) voltage
                        ldrRes = std::stoi(&inputBuffer[i + 2]);
                        break;
                    case '5': // rain sensor temperature
                        rainSensTemp = std::stoi(&inputBuffer[i + 2]);
                        break;
                    case '6': // zener voltage
                        zenerV = std::stoi(&inputBuffer[i + 2]);
                        break;
                    case '8': // raw frequency obtained by light sensor
                        ltFreq = std::stoi(&inputBuffer[i + 2]);
                        break;
                    case '\x11': // handshake
                        inputBuffer[i + 1] = '\0'; // trimString equivalent
                        i = n;
                        break;
                    default: // unexpected character
                        LOGF_DEBUG( "getValues: [%X,%i] = syntax", inputBuffer[i + 1], i);

                        return false;
                }
            }
            else
            {
                LOGF_DEBUG( "getValues: [%s,%i] = format", &inputBuffer[i], i);
            }
        }
    }

    if (sqmSensorStatus == SQM_UNKNOWN)
    {
        // If SQM Light sensor is installed, then ltFreq is greater than zero
        sqmSensorStatus = (ltFreq > 0) ? SQM_DETECTED : SQM_UNDETECTED;
    }


    *internalSupplyVoltage = zenerV;
    *tempEstimate          = estTemp;
    *ldrValue              = ldrRes;
    *rainSensorTemperature = rainSensTemp;
    *lightFreq             = ltFreq;

    return true;
}

bool CloudWatcherController::getSqmStatus(int
        &sqmStatus) // reduced version of getValues just meant to check if SQM available
{
    sendCloudwatcherCommand("C!");

    static const int MAX_GV_BLOCKS = 6; // as of firmware 5.89, answer can be up to 90 characters (6 blocks)

    // C! special case because (1) variable number of responses; (2) order or responses can vary
    // thus processing is different than usual

    sqmStatus = 0;

retry:
    {
        char inputBuffer[BLOCK_SIZE * MAX_GV_BLOCKS] = {0};
        int  blocks = 0;

        int rc = -1;
        int n = 0;
        int nb = 0;

        for (int i = 0, j = 0;  j < MAX_GV_BLOCKS; i += BLOCK_SIZE, j++, nb = 0)
        {
            if ((rc = tty_read(PortFD, &inputBuffer[i], BLOCK_SIZE, READ_TIMEOUT, &nb)) != TTY_OK || nb != BLOCK_SIZE)
            {
                if (rc == TTY_OK)
                {
                    LOGF_ERROR("%s read error[%i] in getSqmStatus: byte count != block size (%i != %i)", __FUNCTION__, rc, nb, BLOCK_SIZE);
                }
                else
                {
                    char errstr[MAXRBUF];
                    tty_error_msg(rc, errstr, MAXRBUF);
                    LOGF_ERROR("%s read error[%i] in getSqmStatus: %s", __FUNCTION__, rc, errstr);
                }
                return false;
            }
            n += nb;
            if (checkValidMessage(&inputBuffer[i], 1, nb, false))
            {
                break;
            }
        }

        if( inputBuffer[0] == '!' && ( inputBuffer[1] == 'f' || inputBuffer[1] == 'd' ) )
        {
            LOGF_DEBUG( "skip answer %s %i", inputBuffer, n );
            goto retry;
        }

        // n is bytes read and in this case is always a multiple of BLOCK_SIZE (if valid)
        if (n > 0 && n % BLOCK_SIZE == 0)
        {
            blocks = n / BLOCK_SIZE;
            auto valid = checkValidMessage(inputBuffer, blocks, n, false);
            LOGF_DEBUG( "getSqmStatus: [%s,%i] = %s", inputBuffer, blocks, valid ? "valid" : "invalid" );

            if (!valid)
                return false;
        }
        else
        {
            LOGF_DEBUG( "getSqmStatus: [%s,%i] = incomplete", inputBuffer, n);

            return false;
        }

        for (int i = 0;  i < n; i += BLOCK_SIZE)
        {
            if (inputBuffer[i] == '!')
            {
                switch(inputBuffer[i + 1])
                {
                    case '3': // ambient temperature
                        break;
                    case '4': // LDR voltage
                        break;
                    case '5': // rain sensor temperature
                        break;
                    case '6': // zener voltage
                        break;
                    case '8': // raw period obtained by light sensor
                        sqmStatus = 1;
                        break;
                    case '\x11': // handshake
                        inputBuffer[i + 1] = '\0'; // trimString equivalent
                        i = n;
                        break;
                    default: // unexpected character
                        LOGF_DEBUG( "getSqmStatus: [%X,%i] = syntax", inputBuffer[i + 1], i);

                        return false;
                }
            }
            else
            {
                LOGF_DEBUG( "getSqmStatus: [%s,%i] = format", &inputBuffer[i], i);
            }
        }
    }

    return true;
}

bool CloudWatcherController::getIRErrors(int *firstAddressByteErrors, int *commandByteErrors,
        int *secondAddressByteErrors, int *pecByteErrors) // CW Cmd: D! (private)
{
    sendCloudwatcherCommand("D!");

    char inputBuffer[BLOCK_SIZE * 5] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 5);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!E1       %d!E2       %d!E3       %d!E4       %d!", firstAddressByteErrors,
                     commandByteErrors, secondAddressByteErrors, pecByteErrors);
    if (res != 4)
    {
        LOGF_DEBUG("internal errors answer did not scan: '%s'", inputBuffer);
        return false;
    }

    return true;
}

bool CloudWatcherController::getRainFrequency(int &rainFreq) // CW Get Rain Frequency Cmd: E! (private)
{
    sendCloudwatcherCommand("E!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    if (!getCloudWatcherAnswer(inputBuffer, 2))
        return false;

    return matchBlock(inputBuffer, "!R", rainFreq); // range is 0 to 6,000
}

bool CloudWatcherController::getSwitchStatus(int *switchStatus) // CW Get Switch Status Cmd: F! (public)
{
    sendCloudwatcherCommand("F!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] == 'X')
    {
        *switchStatus = 1;
    }
    else if (inputBuffer[1] == 'Y')
    {
        *switchStatus = 0;
    }
    else
    {
        LOGF_DEBUG("switch status not X or Y, was '%c'", inputBuffer[1]);
        return false;
    }

    return true;
}

bool CloudWatcherController::openSwitch() // CW Set Switch Open CMD: G! (public)
{
    sendCloudwatcherCommand("G!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] != 'X')
    {
        LOGF_WARN("switch open action not confirmed by response (%c)", inputBuffer[1]);
        return false;
    }

    return true;
}

bool CloudWatcherController::closeSwitch() // CW Set Switch Closed Cmd: H! (public)
{
    sendCloudwatcherCommand("H!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] != 'Y')
    {
        LOGF_WARN("switch close action not confirmed by response (%c)", inputBuffer[1]);
        return false;
    }

    return true;
}

bool CloudWatcherController::setPWMDutyCycle(int pwmDutyCycle) // CW Set PWM Cmd: Pxxxx! (public); xxxx is set value
{
    if (pwmDutyCycle < 0)
    {
        pwmDutyCycle = 0;
    }

    if (pwmDutyCycle > 1023)
    {
        pwmDutyCycle = 1023;
    }

    int newPWM = pwmDutyCycle;

    char message[7] = "Pxxxx!";

    int n      = newPWM / 1000;
    message[1] = n + '0';
    newPWM     = newPWM - n * 1000;

    n          = newPWM / 100;
    message[2] = n + '0';
    newPWM     = newPWM - n * 100;

    n          = newPWM / 10;
    message[3] = n + '0';
    newPWM     = newPWM - n * 10;

    message[4] = newPWM + '0';

    sendCloudwatcherCommand(message, 6);

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!Q         %d!", &newPWM);

    if (res != 1)
    {
        return false;
    }

    if (newPWM != pwmDutyCycle)
    {
        LOGF_WARN("PWM set value (%d) did not match request value (%d)", pwmDutyCycle, newPWM);
        return false;
    }

    return true;
}

bool CloudWatcherController::getPWMDutyCycle(int &pwmDutyCycle) // CW Get PWM Value Cmd: Q! (private)
{
    sendCloudwatcherCommand("Q!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    if (!getCloudWatcherAnswer(inputBuffer, 2))
        return false;

    return matchBlock(inputBuffer, "!Q", pwmDutyCycle);
}

bool CloudWatcherController::getIRSkyTemperature(int
        &temp) // CW Get IR Sky Temp Cmd: S! (private); response in hundredths of a degree Celsius
{
    sendCloudwatcherCommand("S!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    if (!getCloudWatcherAnswer(inputBuffer, 2))
        return false;

    return matchBlock(inputBuffer, "!1", temp);
}

bool CloudWatcherController::getIRSensorTemperature(int
        &temp) // CW Get IR Sensor Temp Cmd: T! (private); response in hundredths of a degree Celsius
{
    sendCloudwatcherCommand("T!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    if (!getCloudWatcherAnswer(inputBuffer, 2))
        return false;

    return matchBlock(inputBuffer, "!2", temp);
}

// Note: z! cmd (Reset RS232 buffer pointers) is unimplemented and unused by INDI.

/******************************************************/
/* CW Cmd funtions from Rs232_Comms_v110.pdf Document */
/******************************************************/
bool CloudWatcherController::getSerialNumber(int &serialNumber) // CW Get Serial Number Cmd: K! (private)
{
    if (m_FirmwareVersion >= 3)
    {
        sendCloudwatcherCommand("K!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};

        if (!getCloudWatcherAnswer(inputBuffer, 2))
            return false;

        return matchBlock(inputBuffer, "!K", serialNumber);
    }
    else
    {
        serialNumber = -1;
    }

    return true;
}

bool CloudWatcherController::getElectricalConstants() // CW Get Electrical Constants Cmd: M! (private)
{
    sendCloudwatcherCommand("M!");

    char inputBuffer[BLOCK_SIZE * 2] = {0};

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] != 'M' || inputBuffer[BLOCK_SIZE] != '!')
    {
        LOGF_DEBUG("syntax problem in electrical: %c%c[%i][%i][%i][%i][%i][%i][%i][%i][%i][%i][%i][%i] %c",
                   inputBuffer[0],
                   inputBuffer[1],
                   (int)inputBuffer[2],
                   (int)inputBuffer[3],
                   (int)inputBuffer[4],
                   (int)inputBuffer[5],
                   (int)inputBuffer[6],
                   (int)inputBuffer[7],
                   (int)inputBuffer[8],
                   (int)inputBuffer[9],
                   (int)inputBuffer[10],
                   (int)inputBuffer[11],
                   (int)inputBuffer[12],
                   (int)inputBuffer[13],
                   inputBuffer[15]
                  );
        return false;
    }

    zenerConstant        = float(256 * (int)inputBuffer[2] + (int)inputBuffer[3]) / 100.0;
    LDRMaxResistance     = float(256 * (int)inputBuffer[4] + (int)inputBuffer[5]) / 1.0;
    LDRPullUpResistance  = float(256 * (int)inputBuffer[6] + (int)inputBuffer[7]) / 10.0;
    rainBeta             = float(256 * (int)inputBuffer[8] + (int)inputBuffer[9]) / 1.0;
    rainResAt25          = float(256 * (int)inputBuffer[10] + (int)inputBuffer[11]) / 10.0;
    rainPullUpResistance = float(256 * (int)inputBuffer[12] + (int)inputBuffer[13]) / 10.0;

    return true;
}

/******************************************************/
/* CW Cmd funtions from Rs232_Comms_v120.pdf Document */
/******************************************************/
bool CloudWatcherController::getAnemometerStatus(int &anemometerStatus) // CW Check for Anemometer Cmd: v! (private)
{
    m_AnemometerStatus = 0; // used in getWindSpeed

    if (m_FirmwareVersion >= 5)
    {
        LOG_DEBUG("sending anemometer check cmd");
        sendCloudwatcherCommand("v!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};

        if (!getCloudWatcherAnswer(inputBuffer, 2))
            return false;

        m_AnemometerStatus = matchBlock(inputBuffer, "!v", anemometerStatus);
        LOGF_DEBUG("anemometer status is %i", m_AnemometerStatus);
    }

    anemometerStatus = m_AnemometerStatus;

    return true;
}

bool CloudWatcherController::getWindSpeed(float &windSpeed) // CW Get Wind Speed Cmd: V! (private)
{

    if (m_FirmwareVersion >= 5 && m_AnemometerStatus)
    {
        sendCloudwatcherCommand("V!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};

        if (!getCloudWatcherAnswer(inputBuffer, 2))
            return false;

        int   ispeed = 0;

        if (!matchBlock(inputBuffer, "!w", ispeed))
            return false;

        float speed = ispeed;

        LOGF_DEBUG( "raw wind speed is %i for anemometer type %s", speed, (anemometerType == BLACK ? "black" : "grey"));

        switch (anemometerType)
        {
            case BLACK:
                if (speed != 0)
                {
                    speed = speed * 0.84 + 3;
                }
                break;

            case GRAY:
            default:
                break;
        }

        windSpeed = speed;
    }
    else
    {
        windSpeed = 0;
    }
    LOGF_DEBUG( "processed wind speed is %i for anemometer type %s", windSpeed, (anemometerType == BLACK ? "black" : "grey"));

    return true;
}

// Note: cmds m! (Get Auto-Shutdown Parameters) and l...! (Set Auto-Shutdown Parameters) are unimplemented and unused by INDI.

/******************************************************/
/* CW Cmd funtions from Rs232_Comms_v130.pdf Document */
/******************************************************/
bool CloudWatcherController::getHumidity(float &humidity) // CW Get Relative Humidity Cmd: h! (private)
{
    if (m_FirmwareVersion >= 5.6)
    {
        sendCloudwatcherCommand("h!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};
        int h = 0;

        if (!getCloudWatcherAnswer(inputBuffer, 2))
            return false;

        if (matchBlock(inputBuffer, "!h", h))
        {
            // Sensor error
            if (h == 100)
            {
                LOG_WARN("relative humidity sensor error detected");
                return false;
            }

            humidity = h * 120. / 100. - 6.;

            if (humidity < 0)
                humidity = 0;
            else if (humidity > 100)
                humidity = 100;

            return true;
        }
        else if (matchBlock(inputBuffer, "!hh", h))
        {
            if( h == 65535 )
            {
                humidity = 0;
                LOG_DEBUG("invalid humidity returned");
            }
            else
            {
                humidity = h * 125. / 65536. - 6.;
            }

            if (humidity < 0)
                humidity = 0;
            else if (humidity > 100)
                humidity = 100;

            return true;
        }
    }
    else
    {
        humidity = -1;
        return true;
    }

    // if we get here, both matchBlock calls failed
    return false;
}

bool CloudWatcherController::getTemperature(float &temperature) // CW Get Ambient Temperature Cmd: t! (private)
{
    if (m_FirmwareVersion >= 5.6)
    {
        sendCloudwatcherCommand("t!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};
        int t = 0;
        float tp;

        if (!getCloudWatcherAnswer(inputBuffer, 2))
            return false;

        if (matchBlock(inputBuffer, "!t", t))
        {
            // Sensor error
            if (t == 100)
            {
                LOG_WARN("temperature sensor error detected");
                return false;
            }

            // extra steps to make compiler happy (odd?!?)
            tp = t;
            tp = tp * 1.7572;
            tp -= 46.85;
            temperature = tp;

            if (temperature < -70)
                temperature = -1000; // indicates something is broken
            else if (temperature > 70)
                temperature = 1000; // indicates something is broken

            return true;
        }
        else if (matchBlock(inputBuffer, "!th", t))
        {
            if( t == 65535 )
            {
                temperature = -1000;
                LOG_DEBUG("invalid temperature returned");
            }
            else
            {
                // extra steps to make compiler happy (odd?!?)
                tp = t;
                tp = tp * 175.72 / 65536.;
                tp -= 46.85;
                temperature = tp;
            }

            if (temperature < -70)
                temperature = -1000; // indicates something is broken
            else if (temperature > 70)
                temperature = 1000; // indicates something is broken

            return true;
        }
    }
    else
    {
        temperature = -1000;
        return true;
    }

    // if we get here, both matchBlock calls failed
    return false;
}

bool CloudWatcherController::getPressure(float &pressure) // CW Get Atmospheric Pressure Cmd: p! (private)
{
    if (m_FirmwareVersion >= 5.8)
    {
        sendCloudwatcherCommand("p!");

        char inputBuffer[BLOCK_SIZE * 2] = {0};
        int p = 0;

        if (!getCloudWatcherAnswer(inputBuffer, 2))
        {
            pressure = 0;
            return true;
        }

        if (matchBlock(inputBuffer, "!p", p))
        {
            if( p == 65535 )
            {
                pressure = 0;
                LOG_DEBUG("invalid pressure returned");
            }
            else
            {
                pressure = p; // raw reading
            }
        }
        else
        {
            pressure = 0;
        }
    }
    else
    {
        pressure = 0;
    }

    return true; // always return an answer even if no pressure device present
}

// Note: reboot cmd is unimplemented and unused by INDI.

/******************************************************/
/* CW Cmd funtions from Rs232_Comms_v140.pdf Document */
/******************************************************/
// Since this document only included changes to the C! cmd, its infomation is already incorporated into the getValues function.
/******************************************************************/
/* End of CloudWatcher Command-Related Serial Port Functions      */
/******************************************************************/



/******************************************************************/
/* PRIVATE MEMBERS                                                */
/******************************************************************/
float CloudWatcherController::aggregateFloats(float values[], int numberOfValues)
{
    float average = 0.0;

    for (int i = 0; i < numberOfValues; i++)
    {
        //    printMessage("%f\n", values[i]);

        average += values[i];
    }

    average /= numberOfValues;

    float stdD = 0.0;

    for (int i = 0; i < numberOfValues; i++)
    {
        stdD += (values[i] - average) * (values[i] - average);
    }

    stdD /= numberOfValues;

    stdD = sqrt(stdD);

    //  printMessage("Average: %f, stdD: %f\n", average, stdD);

    float newAverage  = 0.0;
    int numberOfItems = 0;

    for (int i = 0; i < numberOfValues; i++)
    {
        if (fabs(values[i] - average) <= stdD)
        {
            //      printMessage("%f\n", fabs(values[i] - average));
            newAverage += values[i];
            numberOfItems++;
        }
    }

    newAverage /= numberOfItems;

    printMessage("New average: %f\n", newAverage);

    return newAverage;
}

int CloudWatcherController::aggregateInts(int values[], int numberOfValues)
{
    float newValues[numberOfValues];

    for (int i = 0; i < numberOfValues; i++)
    {
        newValues[i] = (float)values[i];
    }

    return (int)aggregateFloats(newValues, numberOfValues);
}

void CloudWatcherController::trimString(char *str)
{
    char *write_ptr = str;
    // Loop through the string
    while (*write_ptr != '\0')
    {
        if (*write_ptr == 17)
        {
            *write_ptr = 0;
            break;
        }
        else
            write_ptr++;
    }
}

bool CloudWatcherController::checkValidMessage(char *buffer, int nBlocks, int nBytes, bool trim)
{
    int length = nBlocks * BLOCK_SIZE;

    if (length != nBytes)
    {
        LOGF_DEBUG("message block bytes / read bytes mismatch (%d != %d)", length, nBytes);
        return false;
    }

    const char *handshakingBlock = "\x21\x11\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x30";

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (buffer[length - BLOCK_SIZE + i] != handshakingBlock[i])
        {
            return false;
        }
    }

    if (trim)
        trimString(buffer);

    return true;
}

bool CloudWatcherController::sendCloudwatcherCommand(const char *command, int size)
{
    int rc = -1;
    int n = 0;

    LOGF_DEBUG( "sendCloudwatcherCommand(%s,%i)", command, size );

    if ((rc = tty_write(PortFD, command, size, &n)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s write error: %s", __FUNCTION__, errstr);
        return false;
    }

    return true;
}

bool CloudWatcherController::sendCloudwatcherCommand(const char *command)
{
    return sendCloudwatcherCommand(command, 2);
}

bool CloudWatcherController::getCloudWatcherAnswer(char *buffer, int nBlocks)
{
    int rc = -1;
    int n = 0;

    if ((rc = tty_read(PortFD, buffer, nBlocks * BLOCK_SIZE, READ_TIMEOUT, &n)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s read error: %s", __FUNCTION__, errstr);
        return false;
    }

    if( buffer[0] == '!' && ( buffer[1] == 'f' || buffer[1] == 'd' ) )
    {
        LOGF_DEBUG( "skip answer %s %i", buffer, nBlocks );
        return getCloudWatcherAnswer(buffer, nBlocks);
    }

    auto valid = checkValidMessage(buffer, nBlocks, n);

    LOGF_DEBUG( "getCloudWatcherAnswer(%s,%i)[%i] = %s", buffer, nBlocks, n, valid ? "valid" : "invalid" );

    return valid;
}

void CloudWatcherController::printMessage(const char *fmt, ...)
{
    if (verbose)
    {
        va_list args;
        va_start(args, fmt);
        char outputBuffer[2000];
        vsprintf(outputBuffer, fmt, args);
        va_end(args);

        std::cout << outputBuffer;
    }
}

void CloudWatcherController::printBuffer(char *buffer, int num)
{
    for (int i = 0; i < num; i++)
    {
        std::cout << buffer[i];
    }
}

bool CloudWatcherController::matchBlock(const std::string &response, const std::string &prefix, int &value)
{
    std::regex rgx(prefix + R"(\s*([-+]?\d+))");
    std::smatch match;

    if (std::regex_search(response, match, rgx))
    {
        try
        {
            value = std::stol(match.str(1));
            LOGF_DEBUG("match block is: %i", value);
            return true;
        }
        catch (...)
        {
            LOGF_DEBUG("matchBlock std::stoi conversion error: %s", match.str(1).c_str());
            return false;
        }
    }

    LOGF_DEBUG("matchBlock could not match an integer in response: %s", response.c_str());

    return false;
}
