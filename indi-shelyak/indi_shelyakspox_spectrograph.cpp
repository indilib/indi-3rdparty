/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.
  Copyright(c) 2018 Jean-Baptiste Butet. All rights reserved.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <map>

#include "indicom.h"
#include "indimacros.h"
#include "indi_shelyakspox_spectrograph.h"
#include "config.h"

//const char *SPECTROGRAPH_SETTINGS_TAB = "Spectrograph Settings";
const char *CALIBRATION_UNIT_TAB = "Calibration Module";

static std::map<ISState, char> COMMANDS = {
    { ISS_ON, 0x31 },
    { ISS_OFF, 0x30 } //"1" and "0"
};

static std::map<std::string, char> PARAMETERS = {
    { "SKY", 0x30 },
    { "CALIBRATION", 0x31 },
    { "FLAT", 0x32 },
    { "DARK", 0x33 } //"0","1", "2", "3"
};

std::unique_ptr<ShelyakSpox>
    shelyakSpox(new ShelyakSpox()); // create std:unique_ptr (smart pointer) to  our spectrograph object

ShelyakSpox::ShelyakSpox()
{
    PortFD = -1;

    setVersion(SHELYAK_SPOX_VERSION_MAJOR, SHELYAK_SPOX_VERSION_MINOR);
}

ShelyakSpox::~ShelyakSpox() {}

/* Returns the name of the device. */
const char *ShelyakSpox::getDefaultName()
{
    return (char *)"Shelyak Spox";
}

/* Initialize and setup all properties on startup. */
bool ShelyakSpox::initProperties()
{
    INDI::DefaultDevice::initProperties();

    //--------------------------------------------------------------------------------
    // Calibration Unit
    //--------------------------------------------------------------------------------

    // setup the lamp switches
    IUFillSwitch(&LampS[3], "SKY", "SKY", ISS_OFF);
    IUFillSwitch(&LampS[2], "CALIBRATION", "CALIBRATION", ISS_OFF);
    IUFillSwitch(&LampS[1], "FLAT", "FLAT", ISS_OFF);
    IUFillSwitch(&LampS[0], "DARK", "DARK", ISS_OFF);
    IUFillSwitchVector(&LampSP, LampS, 4, getDeviceName(), "CALIBRATION", "Calibration lamps", CALIBRATION_UNIT_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    //--------------------------------------------------------------------------------
    // Options
    //--------------------------------------------------------------------------------

    // setup the text input for the serial port
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    return true;
}

void ShelyakSpox::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
    defineProperty(&PortTP);
    // defineProperty(&SettingsNP); // unused
    loadConfig(true, PortTP.name);
}

bool ShelyakSpox::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    if (isConnected())
    {
        // create properties if we are connected
        defineProperty(&LampSP);
    }
    else
    {
        // delete properties if we arent connected
        deleteProperty(LampSP.name);
    }
    return true;
}

bool ShelyakSpox::Connect()
{
    int rc;
    char errMsg[MAXRBUF];
    if ((rc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(rc, errMsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect to port %s. Error: %s", PortT[0].text, errMsg);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "%s is online.", getDeviceName());
    usleep(500 * 1000);
    //read the serial to flush welcome message of SPOX.
    char line[80];

    int bytes_read = 0;
    int tty_rc     = tty_nread_section(PortFD, line, 80, 0x0a, 3, &bytes_read);
    INDI_UNUSED(tty_rc);

    line[bytes_read] = '\n';
    DEBUGF(INDI::Logger::DBG_SESSION, "bytes read :  %i", bytes_read);

    // Actually nothing is done with theses informations. But code is here.
    //pollingLamps();

    resetLamps();

    return true;
}

// bool ShelyakSpox::pollingLamps()
// {//send polling message
//     lastLampOn = "None";
//
//     sleep(0.1);
//     int rc, nbytes_written;
//     char c[3] = {'1','?',0x0a};
//
//     if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)
//
//       {
//         char errmsg[MAXRBUF];
//         tty_error_msg(rc, errmsg, MAXRBUF);
//         DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
//         return false;
//       } else {
//           DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);
//
//       }
//       sleep(0.1);
//
// //read message send in response by spectrometer
//     char lineCalib[80];
//
//     int bytes_read=0;
//     int tty_rc = tty_nread_section(PortFD, lineCalib, 80, 0x0a, 3, &bytes_read);
//     if (tty_rc < 0)
//     {
//         LOGF_ERROR("Error getting device readings: %s", strerror(errno));
//         return false;
//     }
//     lineCalib[bytes_read] = '\n';
//
//
//   DEBUGF(INDI::Logger::DBG_SESSION,"State of Calib lamp: #%s#", lineCalib );
//
//
//     sleep(0.1);
//     int rc2, nbytes_written2;
//     char c2[3] = {'2','?',0x0a};
//
//     if ((rc2 = tty_write(PortFD, c2, 3, &nbytes_written2)) != TTY_OK)
//       {
//         char errmsg[MAXRBUF];
//         tty_error_msg(rc, errmsg, MAXRBUF);
//         DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
//         return false;
//       } else {
//           DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c2);
//
//       }
//       sleep(0.1);
//
// //read message send in response by spectrometer
//     char lineFlat[80];
//
//     int bytes_read2=0;
//     int tty_rc2 = tty_nread_section(PortFD, lineFlat, 80, 0x0a, 3, &bytes_read2);
//     if (tty_rc < 0)
//     {
//         LOGF_ERROR("Error getting device readings: %s", strerror(errno));
//         return false;
//     }
//     lineFlat[bytes_read] = '\n';
//
//
//   DEBUGF(INDI::Logger::DBG_SESSION,"State of Flat lamp: #%s#", lineFlat );
//
//      if (strcmp(lineCalib,"11")==13)  {
//          lastLampOn = "CALIB";
//      }
//
//      if (strcmp(lineFlat,"21")==13) {
//          lastLampOn = "FLAT";
//      }
//
//      if ((strcmp(lineCalib,"11")==13)&&(strcmp(lineFlat,"21")== 13)) {
//          lastLampOn = "DARK";
//      }
//
//   DEBUGF(INDI::Logger::DBG_SESSION,"Spectrometer has %s state", lastLampOn.c_str());
//
//    return true;
//
// }

bool ShelyakSpox::Disconnect()
{
    sleep(1); // wait for the calibration unit to actually flip the switch
    tty_disconnect(PortFD);
    DEBUGF(INDI::Logger::DBG_SESSION, "%s is offline.", getDeviceName());
    return true;
}

/* Handle a request to change a switch. */

bool ShelyakSpox::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, getDeviceName())) // check if the message is for our device
    {
        if (!strcmp(LampSP.name, name)) // check if its lamp request
        {
            LampSP.s = IPS_OK; // set state to ok (change later if something goes wrong)
            for (int i = 0; i < n; i++)
            {
                ISwitch *s = IUFindSwitch(&LampSP, names[i]);

                if (states[i] != s->s)
                { // check if state has changed
                    DEBUGF(INDI::Logger::DBG_SESSION, "State change %s.", s);
                    DEBUGF(INDI::Logger::DBG_SESSION, "command %x.", COMMANDS[states[i]]);
                    DEBUGF(INDI::Logger::DBG_SESSION, "parameter %x.", PARAMETERS[names[i]]);

                    bool rc = calibrationUnitCommand(COMMANDS[states[i]], PARAMETERS[names[i]]);
                    if (!rc)
                        LampSP.s = IPS_ALERT;
                }
            }
            IUUpdateSwitch(&LampSP, states, names, n); // update lamps
            IDSetSwitch(&LampSP, NULL);                // tell clients to update
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n); // send it to the parent classes
}

/* Handle a request to change text. */
bool ShelyakSpox::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName())) // check if the message is for our device
    {
        if (!strcmp(PortTP.name, name)) //check if is a port change request
        {
            IUUpdateText(&PortTP, texts, names, n); // update port
            PortTP.s = IPS_OK;                      // set state to ok
            IDSetText(&PortTP, NULL);               // tell clients to update the port
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

/* Construct a reset command*/
bool ShelyakSpox::resetLamps()
{
    // wait for the calibration unit to actually flip the switch
    int rc, nbytes_written;
    char c[3] = { '0', '0', 0x0a };

    if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)
    {
        char errmsg[MAXRBUF];
        tty_error_msg(rc, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
        return false;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "RESET : sent on serial: %s.", c);
    }

    return true;
}

/* Construct a command and send it to the spectrograph. It doesn't return
 * anything so we have to sleep until we know it has flipped the switch.
 */
bool ShelyakSpox::calibrationUnitCommand(char command, char parameter)
{
    resetLamps(); //clear all states
    usleep(500 * 1000);   // wait for the calibration unit to actually flip the switch
    int rc, nbytes_written;

    switch (parameter)
    {
        case 0x33:
        { //special for dark : have to put both lamps on

            DEBUGF(INDI::Logger::DBG_SESSION, "DARK LAMP ON :  %s", "OK");
            if (command == 0x31)
            { // dark is on
                DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", "dark is on");
                char cmd[6] = { 0x31, 0x31, 0x0a, 0x32, 0x31, 0x0a }; //"11\n21\n"
                //char c[3] = {parameter,command,0x0a};

                if ((rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)

                {
                    char errmsg[MAXRBUF];
                    tty_error_msg(rc, errmsg, MAXRBUF);
                    DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
                    return false;
                }
                else
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", cmd);
                }
                sleep(1); // wait for the calibration unit to actually flip the switch
                return true;
            }
            INDI_FALLTHROUGH;
        }

        case 0x30:
        { //SKY button
            //SKY -> we shut down all

            DEBUGF(INDI::Logger::DBG_SESSION, "SKY HIT : %s", "No Lamps");
            resetLamps();
            return true;
        }

        case 0x31:
        { //CALIB LAMP
            DEBUGF(INDI::Logger::DBG_SESSION, "CALIB LAMP : %s", "OK");
            char c[3] = { 0x31, 0x31, 0x0a }; //"11\n"

            if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)
            {
                char errmsg[MAXRBUF];
                //                     tty_error_msg(rc, errmsg, MAXRBUF);
                //                     DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
                //                     return false;
                //                 } else {
                DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);
                tty_error_msg(rc, errmsg, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
                return false;
            }
            else
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);
                return true;
            }
        }

        case 0x32:
        { //FLAT LAMP
            DEBUGF(INDI::Logger::DBG_SESSION, "FLAT LAMP : %s", "OK");
            char c[3] = { 0x32, 0x31, 0x0a }; //"21\n"

            if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)

            {
                char errmsg[MAXRBUF];
                tty_error_msg(rc, errmsg, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
                return false;
            }
            else
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);
                return true;
            }
        }
    }
    return true;
}

//other lamps
