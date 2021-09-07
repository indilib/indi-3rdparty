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
#include <iostream>

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

bool CloudWatcherController::checkCloudWatcher()
{
    sendCloudwatcherCommand("A!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    const char *internalNameBlock = "!N CloudWatcher";

    for (int i = 0; i < 15; i++)
    {
        if (inputBuffer[i] != internalNameBlock[i])
        {
            return false;
        }
    }

    return true;
}

bool CloudWatcherController::getSwitchStatus(int *switchStatus)
{
    sendCloudwatcherCommand("F!");

    char inputBuffer[BLOCK_SIZE * 2];

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
        return false;
    }

    return true;
}

bool CloudWatcherController::getAllData(CloudWatcherData *cwd)
{
    int skyTemperature[NUMBER_OF_READS] = {0};
    int sensorTemperature[NUMBER_OF_READS] = {0};
    int rainFrequency[NUMBER_OF_READS] = {0};
    int internalSupplyVoltage[NUMBER_OF_READS] = {0};
    int ambientTemperature[NUMBER_OF_READS] = {0};
    int ldrValue[NUMBER_OF_READS] = {0};
    int rainSensorTemperature[NUMBER_OF_READS] = {0};
    int windSpeed[NUMBER_OF_READS] = {0};
    int humidity[NUMBER_OF_READS] = {0};
    int pressure[NUMBER_OF_READS] = {0};

    int check = 0;

    totalReadings++;

    timeval begin;
    gettimeofday(&begin, nullptr);

    for (int i = 0; i < NUMBER_OF_READS; i++)
    {
        check = getIRSkyTemperature(&skyTemperature[i]);

        if (!check)
        {
            return false;
        }

        check = getIRSensorTemperature(&sensorTemperature[i]);

        if (!check)
        {
            return false;
        }

        check = getRainFrequency(&rainFrequency[i]);

        if (!check)
        {
            return false;
        }

        check = getValues(&internalSupplyVoltage[i], &ambientTemperature[i], &ldrValue[i], &rainSensorTemperature[i]);

        if (!check)
        {
            return false;
        }

        check = getWindSpeed(&windSpeed[i]);

        if (!check)
        {
            return false;
        }

        if (m_FirmwareVersion >= 5.6)
        {
            check = getHumidity(&humidity[i]);

            if (!check)
            {
                return false;
            }
        }

        if (m_FirmwareVersion >= 5.8)
        {

            check = getPressure(&pressure[i]);

            if (!check)
            {
                return false;
            }
        }
    }

    timeval end;
    gettimeofday(&end, nullptr);

    float rc = float(end.tv_sec - begin.tv_sec) + float(end.tv_usec - begin.tv_usec) / 1000000.0;

    cwd->readCycle = rc;

    cwd->sky             = aggregateInts(skyTemperature, NUMBER_OF_READS);
    cwd->sensor          = aggregateInts(sensorTemperature, NUMBER_OF_READS);
    cwd->rain            = aggregateInts(rainFrequency, NUMBER_OF_READS);
    cwd->supply          = aggregateInts(internalSupplyVoltage, NUMBER_OF_READS);
    cwd->ambient         = aggregateInts(ambientTemperature, NUMBER_OF_READS);
    cwd->ldr             = aggregateInts(ldrValue, NUMBER_OF_READS);
    cwd->rainTemperature = aggregateInts(rainSensorTemperature, NUMBER_OF_READS);
    cwd->windSpeed       = aggregateInts(windSpeed, NUMBER_OF_READS);
    if (m_FirmwareVersion >= 5.6)
        cwd->humidity        = aggregateInts(humidity, NUMBER_OF_READS);
    else
        cwd->humidity = -1;
    if (m_FirmwareVersion >= 5.8)
        cwd->pressure        = aggregateInts(pressure, NUMBER_OF_READS);
    else
        cwd->pressure = -1;
    cwd->totalReadings   = totalReadings;

    check = getIRErrors(&cwd->firstByteErrors, &cwd->commandByteErrors, &cwd->secondByteErrors, &cwd->pecByteErrors);

    if (!check)
    {
        return false;
    }

    cwd->internalErrors = cwd->firstByteErrors + cwd->commandByteErrors + cwd->secondByteErrors + cwd->pecByteErrors;

    check = getPWMDutyCycle(&cwd->rainHeater);

    if (!check)
    {
        return false;
    }

    check = getSwitchStatus(&cwd->switchStatus);

    if (!check)
    {
        return false;
    }

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

    r = getSerialNumber(&(cwc->internalSerialNumber));

    if (!r)
    {
        return false;
    }

    if (m_FirmwareVersion >= 3)
    {
        r = getElectricalConstants();

        if (!r)
        {
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

    getAnemometerStatus(&cwc->anemometerStatus);

    return true;
}

bool CloudWatcherController::closeSwitch()
{
    sendCloudwatcherCommand("G!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] != 'X')
    {
        return false;
    }

    return true;
}

bool CloudWatcherController::openSwitch()
{
    sendCloudwatcherCommand("H!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] != 'Y')
    {
        return false;
    }

    return true;
}

bool CloudWatcherController::setPWMDutyCycle(int pwmDutyCycle)
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

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!Q         %d", &newPWM);

    if (res != 1)
    {
        return false;
    }

    if (newPWM != pwmDutyCycle)
    {
        return false;
    }

    return true;
}

/******************************************************************/
/* PRIVATE MEMBERS                                                */
/******************************************************************/
bool CloudWatcherController::getFirmwareVersion(double &version)
{
    if (m_FirmwareVersion == 0)
    {
        char fw[8] = {0};

        sendCloudwatcherCommand("B!");

        char inputBuffer[BLOCK_SIZE * 2];

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int res = sscanf(inputBuffer, "!V         %4s", fw);

        if (res != 1)
        {
            return false;
        }

        try
        {
            m_FirmwareVersion = std::stod(fw);
            version = m_FirmwareVersion;
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}

bool CloudWatcherController::getIRSkyTemperature(int *temp)
{
    sendCloudwatcherCommand("S!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!1        %d", temp);

    if (res != 1)
    {
        return false;
    }

    return true;
}

bool CloudWatcherController::getIRSensorTemperature(int *temp)
{
    sendCloudwatcherCommand("T!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!2        %d", temp);

    if (res != 1)
    {
        return false;
    }

    return true;
}

bool CloudWatcherController::getRainFrequency(int *rainFreq)
{
    sendCloudwatcherCommand("E!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!R         %d", rainFreq);

    if (res != 1)
    {
        return false;
    }

    return true;
}

bool CloudWatcherController::getSerialNumber(int *serialNumber)
{
    if (m_FirmwareVersion >= 3)
    {
        sendCloudwatcherCommand("K!");

        char inputBuffer[BLOCK_SIZE * 2];

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int res = sscanf(inputBuffer, "!K%d", serialNumber);

        if (res != 1)
        {
            return false;
        }
    }
    else
    {
        *serialNumber = -1;
    }

    return true;
}

bool CloudWatcherController::getElectricalConstants()
{
    sendCloudwatcherCommand("M!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    if (inputBuffer[1] != 'M')
    {
        return false;
    }

    zenerConstant        = float(256 * inputBuffer[2] + inputBuffer[3]) / 100.0;
    LDRMaxResistance     = float(256 * inputBuffer[4] + inputBuffer[5]) / 1.0;
    LDRPullUpResistance  = float(256 * inputBuffer[6] + inputBuffer[7]) / 10.0;
    rainBeta             = float(256 * inputBuffer[8] + inputBuffer[9]) / 1.0;
    rainResAt25          = float(256 * inputBuffer[10] + inputBuffer[11]) / 10.0;
    rainPullUpResistance = float(256 * inputBuffer[12] + inputBuffer[13]) / 10.0;

    return true;
}

bool CloudWatcherController::getAnemometerStatus(int *anemometerStatus)
{
    if (m_FirmwareVersion >= 5)
    {
        sendCloudwatcherCommand("v!");

        char inputBuffer[BLOCK_SIZE * 2];

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int res = sscanf(inputBuffer, "!v         %d", anemometerStatus);

        if (res != 1)
        {
            return false;
        }
    }
    else
    {
        *anemometerStatus = 0;
    }

    return true;
}

bool CloudWatcherController::getWindSpeed(int *windSpeed)
{

    if (m_FirmwareVersion >= 5)
    {
        sendCloudwatcherCommand("V!");

        char inputBuffer[BLOCK_SIZE * 2];

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int speed = 0;
        int res = sscanf(inputBuffer, "!w       %d", &speed);

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

        *windSpeed = speed;

        if (res != 1)
        {
            return false;
        }
    }
    else
    {
        *windSpeed = 0;
    }

    return true;
}

bool CloudWatcherController::getHumidity(int *humidity)
{
    if (m_FirmwareVersion >= 5)
    {
        sendCloudwatcherCommand("h!");

        char inputBuffer[BLOCK_SIZE * 2];

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int h = 0;
        int res = sscanf(inputBuffer, "!h       %d", &h);

        if (res == 1)
        {
            // Sensor error
            if (h == 100)
                return false;

            *humidity = h * 120 / 100 - 6;

            return true;
        }

        // Try high resolution version
        res = sscanf(inputBuffer, "!hh       %d", &h);

        if (res == 1)
        {
            // Sensor error
            if (h == 100)
                return false;

            *humidity = h * 125 / 65536 - 6;

            return true;
        }

        return false;
    }
    else
    {
        *humidity = 0;
    }

    return true;
}

bool CloudWatcherController::getPressure(int *pressure)
{
    if (m_FirmwareVersion >= 5)
    {
        sendCloudwatcherCommand("p!");

        char inputBuffer[BLOCK_SIZE * 2];

        int r = getCloudWatcherAnswer(inputBuffer, 2);

        if (!r)
        {
            return false;
        }

        int p = 0;
        int res = sscanf(inputBuffer, "!p       %d", &p);

        *pressure = p / 16;

        if (res != 1)
        {
            return false;
        }
    }
    else
    {
        *pressure = 0;
    }

    return true;
}

bool CloudWatcherController::getValues(int *internalSupplyVoltage, int *ambientTemperature, int *ldrValue,
                                       int *rainSensorTemperature)
{
    sendCloudwatcherCommand("C!");

    int zenerV;
    int ambTemp = -10000;
    int ldrRes;
    int rainSensTemp;

    if (m_FirmwareVersion >= 3)
    {
        char inputBuffer[BLOCK_SIZE * 4];

        int r = getCloudWatcherAnswer(inputBuffer, 4);

        if (!r)
        {
            return false;
        }

        int res = sscanf(inputBuffer, "!6         %d!4         %d!5         %d", &zenerV, &ldrRes, &rainSensTemp);

        if (res != 3)
        {
            return false;
        }
    }
    else
    {
        char inputBuffer[BLOCK_SIZE * 5];

        int r = getCloudWatcherAnswer(inputBuffer, 5);

        if (!r)
        {
            return false;
        }

        int res = sscanf(inputBuffer, "!6         %d!3         %d!4         %d!5         %d", &zenerV, &ambTemp,
                         &ldrRes, &rainSensTemp);

        if (res != 3)
        {
            return false;
        }
    }

    *internalSupplyVoltage = zenerV;
    *ambientTemperature    = ambTemp;
    *ldrValue              = ldrRes;
    *rainSensorTemperature = rainSensTemp;

    return true;
}

bool CloudWatcherController::getPWMDutyCycle(int *pwmDutyCycle)
{
    sendCloudwatcherCommand("Q!");

    char inputBuffer[BLOCK_SIZE * 2];

    int r = getCloudWatcherAnswer(inputBuffer, 2);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!Q         %d", pwmDutyCycle);

    if (res != 1)
    {
        return false;
    }

    return true;
}

bool CloudWatcherController::getIRErrors(int *firstAddressByteErrors, int *commandByteErrors,
        int *secondAddressByteErrors, int *pecByteErrors)
{
    sendCloudwatcherCommand("D!");

    char inputBuffer[BLOCK_SIZE * 5];

    int r = getCloudWatcherAnswer(inputBuffer, 5);

    if (!r)
    {
        return false;
    }

    int res = sscanf(inputBuffer, "!E1       %d!E2       %d!E3       %d!E4       %d", firstAddressByteErrors,
                     commandByteErrors, secondAddressByteErrors, pecByteErrors);
    if (res != 4)
    {
        return false;
    }

    return true;
}

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

bool CloudWatcherController::checkValidMessage(char *buffer, int nBlocks)
{
    int length = nBlocks * BLOCK_SIZE;

    const char *handshakingBlock = "\x21\x11\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x30";

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (buffer[length - BLOCK_SIZE + i] != handshakingBlock[i])
        {
            return false;
        }
    }

    return true;
}

bool CloudWatcherController::sendCloudwatcherCommand(const char *command, int size)
{
    int rc = -1;
    int n = 0;
    char errstr[MAXRBUF];

    if ((rc = tty_write(PortFD, command, size, &n)) != TTY_OK)
    {
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
    char errstr[MAXRBUF];

    if ((rc = tty_read(PortFD, buffer, nBlocks * BLOCK_SIZE, READ_TIMEOUT, &n)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s read error: %s", __FUNCTION__, errstr);
        return false;
    }

    int valid = checkValidMessage(buffer, nBlocks);

    if (!valid)
    {
        return false;
    }

    return true;
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
