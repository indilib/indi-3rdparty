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

#pragma once

/**
 *  A struct to group and send all AAG Cloud Watcher constants
 */

struct CloudWatcherConstants
{
    double firmwareVersion;
    int internalSerialNumber;
    float zenerVoltage;
    float ldrMaxResistance;
    float ldrPullUpResistance;
    float rainBetaFactor;
    float rainResistanceAt25;
    float rainPullUpResistance;
    float ambientBetaFactor;
    float ambientResistanceAt25;
    float ambientPullUpResistance;
    int anemometerStatus; ///< The status of the anemometer
};

/**
 *  New models have a black anemometer with a slightly different speed calculation
 */

enum ANEMOMETER_TYPE
{
    GRAY,
    BLACK
};

/**
 *  A struct  to group and send all AAG Cloud Watcher gathered data (RAW data,
 *  directly from the device)
 */

struct CloudWatcherData
{
    int supply;  ///< Internal Supply Voltage
    int sky;     ///< IR Sky Temperature
    int sensor;  ///< IR Sensor Temperature
    int ambient; ///< Ambient temperature. In newer models there is no ambient temperature sensor so -10000 is returned.
    int rain;    ///< Rain frequency
    int rainHeater; ///< PWM Duty Cycle
    int rainTemperature; ///< Rain sensor temperature (used as ambient temperature in models where there is no ambient temperature sensor)
    int ldr;               ///< Ambient light sensor
    int ldrFreq;            ///< Ambient light sensor in K
    float readCycle;       ///< Time used in the readings
    int totalReadings;     ///< Total number of readings taken by the Cloud Watcher Controller
    int internalErrors;    ///< Total number of internal errors
    int firstByteErrors;   ///< First byte errors count
    int secondByteErrors;  ///< Second byte errors count
    int pecByteErrors;     ///< PEC byte errors count
    int commandByteErrors; ///< Command byte errors count
    int switchStatus;      ///< The status of the internal switch
    int windSpeed;         ///< The wind speed measured by the anemometer
    int humidity;          ///< The relative humidity
    int pressure;          ///< atmospheric pressure
};

/**
 * A class  to communicate with the AAG Cloud Watcher. It is responsible to
 * send and recieve all the commands specified in the AAG Cloud Watcher
 * documentation through the serial port.
 */

class CloudWatcherController
{
    public:
        /**
        * A constructor.
        */
        CloudWatcherController();

        /**
        * A constructor.
        * @param verbose to specify if communication details have to be printed in the
        * standar output. Do not use, just for testign pourposes.
        */
        CloudWatcherController(bool verbose);

        /**
        * A destructor
        */
        virtual ~CloudWatcherController() = default;

        const char *getDeviceName();

        /**
        * Sets the file descriptor to use for communication
        * @param newPortFD The new file descriptor
        */
        void setPortFD(int newPortFD);

        /**
        * Sets the anemometer type (currently grey for old models, black on newer)
        */
        void setAnemometerType(enum ANEMOMETER_TYPE type);

        /**
        * Checks if the AAG Cloud Watcher is connected and accesible by requesting
        * its device name.
        * @return true if the AAG CLoud Watcher is connected and accesible. false
        * otherwise.
        */
        bool checkCloudWatcher();

        /**
        * Obtains the status of the internal Switch of the AAG CLoud Watcher.
        * @param switchStatus where the switch status will be stored. 1 if open,
        * 0 if closed.
        * @return true if the status of the switch has been correctly determined.
        * false otherwise.
        */
        bool getSwitchStatus(int *switchStatus);

        /**
        * Gets all raw dynamic data from the AAG Cloud Watcher. It follows the
        * procedure described in the AAG Documents (5 readings for some values). This
        * function takes more than 2 seconds and less than 3 to complete.
        * @param cwd where the dynamic data of the AAG Cloud Watcher will be stored.
        * @return true if the data has been correctly gathered. false otherwise.
        */
        bool getAllData(CloudWatcherData * cwd);

        /**
        * Gets all constants from the AAG Cloud Watcher. Some of the constants are
        * retrieved from the device (from firmware version >3.0)
        * @param cwc where the constants of the AAG Cloud Watcher will be stored.
        * @return true if the constants has been correctly gathered. false otherwise.
        */
        bool getConstants(CloudWatcherConstants * cwc);

        /**
        * Closes the internal switch of the AAG Cloud Watcher
        *
        * @return true if the internal switch is closed. false otherwise.
        */
        bool closeSwitch();

        /**
        * Opens the internal switch of the AAG Cloud Watcher
        *
        * @return true if the internal switch is opened. false otherwise.
        */
        bool openSwitch();

        /**
        * Sets the PWM Duty Cycle (heater control) of the AAG Cloud Watcher
        * @param pwmDutyCycle The value of the PWM Duty Cycle
        *        (min - 0 to max - 1023)
        * @return true if the PWM Duty Cycle has been successfully setted.
        *   false otherwise.
        */
        bool setPWMDutyCycle(int pwmDutyCycle);

    private:
        /**
        * true if info verbose output should be shown. Just for debugging pourposes.
        */
        int verbose = false;

        /**
        *  File descriptor for the serial or tcp connection
        */
        int PortFD = -1;

        /**
         *  Firmware Version
         */
        double m_FirmwareVersion = 0;

        /**
        *  Anemometer type
        */
        enum ANEMOMETER_TYPE anemometerType = BLACK;

        /**
        * AAG CloudWatcher send information in 15 bytes blocks
        */
        const static int BLOCK_SIZE = 15;

        /**
        * Number of reads to aggregate for the cloudwatcher data
        */
        const static int NUMBER_OF_READS = 5;

        /**
        * Hard coded constant. May be changed with internal device constants.
        * @see getElectricalConstants()
        */
        float zenerConstant = 3.0;

        /**
        * Hard coded constant.
        */
        float ambPullUpResistance = 9.9;

        /**
        * Hard coded constant.
        */
        float ambResAt25 = 10;

        /**
        * Hard coded constant.
        */
        float ambBeta = 3811;

        /**
        * Hard coded constant. May be changed with internal device constants.
        * @see getElectricalConstants()
        */
        float LDRMaxResistance = 2000;

        /**
        * Hard coded constant. May be changed with internal device constants.
        * @see getElectricalConstants()
        */
        float LDRPullUpResistance = 56;

        /**
        * Hard coded constant. May be changed with internal device constants.
        * @see getElectricalConstants()
        */
        float rainPullUpResistance = 1;

        /**
        * Hard coded constant. May be changed with internal device constants.
        * @see getElectricalConstants()
        */
        float rainResAt25 = 1;

        /**
        * Hard coded constant. May be changed with internal device constants.
        * @see getElectricalConstants()
        */
        float rainBeta = 3450;

        /**
        * The total number of readings performed by the controller
        */
        int totalReadings = 0;

        /**
        * Print a buffer of chars. Just for debugging
        * @param buffer the buffer to be printed
        * @param num the number of chars to be printed
        */
        void printBuffer(char *buffer, int num);

        /**
        * Prints a printf like expression if verbose mode is enabled. Just for
        * debugging
        * @param fmt the pritnfexpression to be printed
        * @see CloudWatcherController(char *serialP, bool verbose)
        */
        void printMessage(const char *fmt, ...);

        /**
        * Checks if the received message is a AAG CLoud Wathcer valid message.
        * @param buffer the readed message
        * @param nBlocks the number of expected blocks in the message
        * @return true if it is a valid message. false otherwise
        */
        bool checkValidMessage(char *buffer, int nBlocks);

        /**
        * Sends a command to the AAG Cloud Watcher.
        * @param command a two byte array with the command to be sent
        * @return true if successfully sent. false otherwise
        */
        bool sendCloudwatcherCommand(const char *command);

        /**
        * Sends a command to the AAG Cloud Watcher.
        * @param command a byte array with the command to be sent
        * @param size the number of bytes of the command
        * @return true if successfully sent. false otherwise
        */
        bool sendCloudwatcherCommand(const char *command, int size);

        /**
        * Reads a AAG Cloud Watcher answer
        * @param buffer where the answer will be stored. Should be big enough
        * @param nBlocks number of blocks to be readed
        */
        bool getCloudWatcherAnswer(char *buffer, int nBlocks);

        /**
        * Reads the firmware version of the AAG Cloud Watcher (if not previously
        * read).
        * @param version
        * @return true if succesfully read. false otherwise.
        * @see getFirmwareVersion()
        */
        bool getFirmwareVersion(double &version);

        /**
        * Reads the serial number of the AAG Cloud Watcher
        * @param serialNumber where the serial number will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getSerialNumber(int *serialNumber);

        /**
        * Performs an aggregation of the values stored in a float array. It computes
        * average and standard deviation of the array and averages only the values
        * within [average - deviation, average + deviation]
        * @param values the values to be aggregated
        * @param numberOfValues the size of values
        * @return the aggregated value
        */
        float aggregateFloats(float values[], int numberOfValues);

        /**
        * Performs an aggregation of the values stored in a int array. It computes
        * average and standard deviation of the array and averages only the values
        * within [average - deviation, average + deviation]
        * @param values the values to be aggregated
        * @param numberOfValues the size of values
        * @return the aggregated value
        */
        int aggregateInts(int values[], int numberOfValues);

        /**
        * Reads the current IR Sky Temperature value of the AAG Cloud Watcher
        * @param temp where the sensor value will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getIRSkyTemperature(int *temp);

        /**
        * Reads the current IR Sensor Temperature value of the AAG Cloud Watcher
        * @param temp where the sensor value will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getIRSensorTemperature(int *temp);

        /**
        * Reads the current Rain Frequency value of the AAG Cloud Watcher
        * @param rainFreq where the sensor value will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getRainFrequency(int *rainFreq);

        /**
        * Reads the current Internal Supply Voltage, Ambient Temperature, LDR Value
        * and Rain Sensor Temperature values of the AAG Cloud Watcher
        * @param internalSupplyVoltage where the sensor value will be stored
        * @param ambientTemperature where the sensor value will be stored
        * @param ldrValue where the sensor value will be 
        * @param ldrFreqValue where the sensor value in K will be stored, if Firmware >= 5.88
        * @param rainSensorTemperature where the sensor value will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getValues(int *internalSupplyVoltage, int *ambientTemperature, int *ldrValue, int *ldrFreqValue, int *rainSensorTemperature);

        /**
        * Reads the current PWM Duty Cycle value of the AAG Cloud Watcher
        * @param pwmDutyCycle where the sensor value will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getPWMDutyCycle(int *pwmDutyCycle);

        /**
        * Reads the current Error values of the AAG Cloud Watcher
        * @param firstAddressByteErrors where the first byte error count will be stored
        * @param commandByteErrors where the command byte error count will be stored
        * @param secondAddressByteErrors where the second byte error count will be stored
        * @param pecByteErrors where the PEC byte error count will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getIRErrors(int *firstAddressByteErrors, int *commandByteErrors, int *secondAddressByteErrors,
                         int *pecByteErrors);

        /**
        * Reads the electrical constants from the AAG Cloud Watcher and stores them
        * into the internal attributes
        * @return true if succesfully read. false otherwise.
        * @see zenerConstant
        * @see LDRMaxResistance
        * @see LDRPullUpResistance
        * @see rainPullUpResistance
        * @see rainResAt25
        * @see rainBeta
        */
        bool getElectricalConstants();

        /**
        * Reads the anemometer status of the AAG Cloud Watcher
        * @param anemometerStatus where the anemometer status will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getAnemometerStatus(int *anemomterStatus);

        /**
        * Reads the wind speed from the anemomter
        * @param windSpeed where the wind speed will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getWindSpeed(int *windSpeed);

        /**
        * Reads the humidity from external sensor
        * @param humidity where the humidity will be stored
        * @return true if succesfully read. false otherwise.
        */
        bool getHumidity(int *humidity);

        /**
        * Reads the pressure from external sensor
        * @param pressure where the pressure will be stored. Unit is Pa
        * @return true if succesfully read. false otherwise.
        */
        bool getPressure(int *pressure);
};
