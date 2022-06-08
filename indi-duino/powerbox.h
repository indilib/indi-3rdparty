/*
    Power Box - an Arduino based power box controlling two power switches
    and two PWM controlled dimmers.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaegeropenfuture.de>

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

#pragma once

#include <map>

#include "gason/gason.h"

#include "defaultdevice.h"
#include "indilightboxinterface.h"

class PowerBox : public INDI::DefaultDevice, public INDI::LightBoxInterface
{
public:

    const char *LIGHT_BOX_TAB = "Light Box";

    PowerBox();

    // define basic properties to clients.
    virtual void ISGetProperties(const char *dev) override;
    // process Number properties
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    // process Text properties
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    // process Switch properties
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    // process BLOB properties
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;

    // initialize when the driver gets connected */
    virtual bool Handshake();

protected:
    enum pb_command {
        CMD_CONFIG,         /* read the device configuration                   */
        CMD_STATUS,         /* read the device status                          */
        CMD_PWM_FREQUENCY,  /* set the PWM frequency                           */
        CMD_PWM_DUTY_CYCLE, /* set the PWM duty cycle for a single port        */
        CMD_PWM_POWER,      /* turn on or off the power on a single PWM port   */
        CMD_SWITCH_POWER,   /* turn on or off the power on a single power port */
        CMD_RESET           /* reset the Arduino                               */
    };

    // port power status
    enum {POWER_ON, POWER_OFF};

    struct PwmStatus {
        bool power;
        double duty_cycle;
    };

    typedef std::map<pb_command, std::string> pb_command_map;
    pb_command_map commands;

    // properties for the two power ports
    ISwitch PowerPortStatus_1_S[2] = {};
    ISwitchVectorProperty PowerPortStatus_1_SP;
    ISwitch PowerPortStatus_2_S[2] = {};
    ISwitchVectorProperty PowerPortStatus_2_SP;

    // PWM frequency property
    INumber PWMFrequencyN[1] = {};
    INumberVectorProperty PWMFrequencyNP;

    // properties for the two PWM ports
    ISwitch PWMPortStatus_1_S[2] = {};
    ISwitchVectorProperty PWMPortStatus_1_SP;
    ISwitch PWMPortStatus_2_S[2] = {};
    ISwitchVectorProperty PWMPortStatus_2_SP;
    // PWM duty cycle
    INumber PWMDutyCycle_1_N[1] = {};
    INumberVectorProperty PWMDutyCycle_1_NP;
    INumber PWMDutyCycle_2_N[1] = {};
    INumberVectorProperty PWMDutyCycle_2_NP;

    // Light box control properties
    ISwitch LightBoxPWMPortS[2] = {};
    ISwitchVectorProperty LightBoxPWMPortSP;

    // default device name
    const char *getDefaultName() override;

    // Initialize all properties & set default values.
    virtual bool initProperties() override;
    // Update the INDI properties as a reaction on connect or disconnect.
    virtual bool updateProperties() override;

    // Initial function to get data after connection is successful
    bool getBasicData();
    // Read power box status
    bool getStatus();

    // From Light Box
    virtual bool SetLightBoxBrightness(uint16_t value) override;
    virtual bool EnableLightBox(bool enable) override;

    // save the current configuration
    virtual bool saveConfigItems(FILE *fp) override;
    // update status information
    void TimerHit() override;

    /**
     * @brief Read the firmware configuration
     * @param value parsed JSON document
     */
    IPState handleConfig(JsonValue jvalue);

    /**
     * @brief Read the device status
     * @param value parsed JSON document
     */
    IPState handleStatus(JsonValue jvalue);

    /**
     * @brief Extract the power switch status
     * @param value parsed JSON document
     */
    bool readPowerPortStatus(JsonValue jvalue);
    /**
     * @brief Turn a power switch on or off
     * @param port_number corresponding port number
     * @return true iff setting the value succeeded
     */
    bool setPowerPortStatus(int port_number);

    /**
     * @brief Extract the PWM port status
     * @param value parsed JSON document
     */
    PwmStatus readPWMPortStatus(JsonValue jvalue);
    /**
     * @brief Turn a PWM port on or off
     * @param port_number corresponding port number
     * @param powerOn true iff port to be turned on
     * @return true iff setting the value succeeded
     */
    bool setPWMPortStatus(int port_number, bool powerOn);
    /**
     * @brief Set the PWM duty cycle
     * @param port_number corresponding port number
     * @return true iff setting the value succeeded
     */
    bool setPWMDutyCycle(int port_number, int value);

    /**
     * @brief Update the light box settings matching the selected
     *        PWM control.
     */
    void updateLightBoxSettings();

    // TTY interface timeout
    int getTTYTimeout() { return ttyTimeout; }
    int ttyTimeout = 1;
    INumber ttyTimeoutN[1] = {};
    INumberVectorProperty ttyTimeoutNP;

    // Serial communication
    bool receiveSerial(char* buffer, int* bytes, char end, int wait);
    bool transmitSerial(std::string buffer);

    // send a command to the serial device
    bool executeCommand(pb_command cmd, std::string args = "");
    // handle one single response line
    void handleResponse(pb_command cmd, const char *response, u_long length);
    // handle a message from the power box
    void handleMessage(JsonValue value);

    // file descriptor for the serial port
    int PortFD = -1;
    // serial port connection
    Connection::Serial *serialConnection{ nullptr };
};
