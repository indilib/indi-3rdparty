/*******************************************************************************
  Copyright(c) 2024 Colin McGill and Rodolphe Pineau. All rights reserved.

  AStarBox power control driver.

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

  The code is based on the pegasus_pbb.cpp plug in.

*******************************************************************************/

#include "indi-astarbox.h"
#include "indicom.h"
// #include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <config.h>
#include <sys/ioctl.h>

// We declare an auto pointer to AStarBox.
static std::unique_ptr<AStarBox> astarbox(new AStarBox());

AStarBox::AStarBox()
{
  // Versions are set in CMakeLists.txt
  // Will need to run cmake to change
    setVersion(VERSION_MAJOR,VERSION_MINOR);
}

const char * AStarBox::getDefaultName()
{
    return "AStarBox";
}


bool AStarBox::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE);


    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power on Boot
    PowerPortsSP[POWER_PORT_1].fill("POWER_PORT_1", "Port 1", ISS_ON);
    PowerPortsSP[POWER_PORT_2].fill("POWER_PORT_2", "Port 2", ISS_ON);
    PowerPortsSP[POWER_PORT_3].fill("POWER_PORT_3", "Port 3", ISS_ON);
    PowerPortsSP[POWER_PORT_4].fill("POWER_PORT_4", "Port 4", ISS_ON);
    PowerPortsSP.fill(getDeviceName(),"POWER_CONTROL", "Power Control",
		      MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Dew PWM
    DewPWMNP[DEW_PWM_1].fill("DEW_1", "Dew 1 (%)", "%.2f", 0, 100, 5, 0);
    DewPWMNP[DEW_PWM_2].fill("DEW_2", "Dew 2 (%)", "%.2f", 0, 100, 5, 0);
    DewPWMNP.fill(getDeviceName(), "DEW_PWM", "Dew PWM", MAIN_CONTROL_TAB,
		  IP_RW, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Sensor Group
    ////////////////////////////////////////////////////////////////////////////

     // Power Sensors
    PowerSensorsNP[SENSOR_VOLTAGE].fill("SENSOR_VOLTAGE", "Voltage (V)",
					"%4.2f", 0, 999, 100, 0);
    PowerSensorsNP.fill(getDeviceName(), "POWER_SENSORS", "Sensors",
			MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    /*    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);
    */
    /* Start Timers for data collection */
    TimerHit();
    
    return true;
}

bool AStarBox::Connect()
{
  int n_err;

  // Now can see if we can connect
  n_err = m_AStarBoxPort.connect();
  if (n_err != PLUGIN_OK) {
    LOG_ERROR("Unable to connect AStarBox.");
    LOG_ERROR("Ensure I2C is enabled and 12V input is connected.");
    m_AStarBoxPort.disconnect();
    return false;
  }

  // Make sure PWM ports set to on. Level will be set later (and could be 0).
  m_AStarBoxPort.setPort(PWM_1, true);
  m_AStarBoxPort.setPort(PWM_2, true);

  return true;
}

 
bool AStarBox::Disconnect()
{
  m_AStarBoxPort.disconnect();
    LOG_INFO("AStarBox disconnected successfully!");
    return true;
}


bool AStarBox::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Main Control
      defineProperty(PowerPortsSP);
      defineProperty(DewPWMNP);      
      defineProperty(PowerSensorsNP);
      setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerSensorsNP);
        deleteProperty(PowerPortsSP);
        deleteProperty(DewPWMNP);
        setupComplete = false;
    }

    return true;
}


bool AStarBox::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power on boot
        if (PowerPortsSP.isNameMatch(name))
        {
            PowerPortsSP.update(states, names, n);
            PowerPortsSP.setState(setPowerPorts() ? IPS_OK : IPS_ALERT);
            PowerPortsSP.apply();
            saveConfig(true, PowerPortsSP.getName());
            return saveBootValues() == PLUGIN_OK;
        }

    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AStarBox::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Dew PWM
        if (DewPWMNP.isNameMatch(name))
        {
	  int rc1 = PLUGIN_OK, rc2 = PLUGIN_OK;
            for (int i = 0; i < n; i++)
            {
	      int percent = values[i];
	      if (!strcmp(names[i], DewPWMNP[DEW_PWM_1].getName())) {
		rc1 =
		      m_AStarBoxPort.setPortPWMDutyCyclePercent(PWM_1, percent) == PLUGIN_OK;
	      }	else if (!strcmp(names[i], DewPWMNP[DEW_PWM_2].getName())) {
		rc2 = m_AStarBoxPort.setPortPWMDutyCyclePercent(PWM_2, percent) == PLUGIN_OK;
	      }
            }

            DewPWMNP.setState((rc1 != PLUGIN_OK || rc2 != PLUGIN_OK) ?
			      IPS_OK : IPS_ALERT);
            if (DewPWMNP.getState() == IPS_OK)
                DewPWMNP.update(values, names, n);
            DewPWMNP.apply();
	    // Now save the boot values
            return saveBootValues() == PLUGIN_OK;
        }

    }
    
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/* Function to write boot values */
int AStarBox::saveBootValues()
{
  int rc;
  bool bOn;
  // Create array of boot values from the interface
  std::vector<int> bootStates;
  bootStates.clear();
  rc = m_AStarBoxPort.getPortStatus(POWER_1, bOn); if (rc>0) return rc;
  bootStates.push_back(bOn?1:0);
  rc = m_AStarBoxPort.getPortStatus(POWER_2, bOn); if (rc>0) return rc;
  bootStates.push_back(bOn?1:0);
  rc = m_AStarBoxPort.getPortStatus(POWER_3, bOn); if (rc>0) return rc;
  bootStates.push_back(bOn?1:0);
  rc = m_AStarBoxPort.getPortStatus(POWER_4, bOn); if (rc>0) return rc;
  bootStates.push_back(bOn?1:0);
  rc = m_AStarBoxPort.getPortStatus(PWM_1, bOn); if (rc>0) return rc;
  bootStates.push_back(bOn?1:0);
  rc = m_AStarBoxPort.getPortStatus(PWM_2, bOn); if (rc>0) return rc;
  bootStates.push_back(bOn?1:0);

  return(m_AStarBoxPort.saveBootStates(bootStates));
}
  
bool AStarBox::setPowerPorts()
{
  int n_err;
    n_err = m_AStarBoxPort.setPort(POWER_1,PowerPortsSP[POWER_PORT_1].getState() == ISS_ON); if (n_err) return false;
    n_err = m_AStarBoxPort.setPort(POWER_2,PowerPortsSP[POWER_PORT_2].s == ISS_ON); if (n_err) return false;
    n_err = m_AStarBoxPort.setPort(POWER_3,PowerPortsSP[POWER_PORT_3].s == ISS_ON); if (n_err) return false;
    n_err = m_AStarBoxPort.setPort(POWER_4,PowerPortsSP[POWER_PORT_4].s == ISS_ON); if (n_err) return false;

    return true;
}


bool AStarBox::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    return true;
}

void AStarBox::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }
    getSensorData();
    SetTimer(getCurrentPollingPeriod());
}


bool AStarBox::getSensorData()
{
  int rc1, rc2, rc3, rc4;
  int pwmpercent;
  double voltage;
    bool bOn;

    // First get port state data 
    rc1 = m_AStarBoxPort.getPortStatus(POWER_1, bOn);
    if (rc1 == PLUGIN_OK)
      PowerPortsSP[POWER_PORT_1].setState(bOn ? ISS_ON : ISS_OFF);

    rc2 = m_AStarBoxPort.getPortStatus(POWER_2, bOn);
    if (rc2 == PLUGIN_OK)
      PowerPortsSP[POWER_PORT_2].setState(bOn ? ISS_ON : ISS_OFF);

    rc3 = m_AStarBoxPort.getPortStatus(POWER_3, bOn);
    if (rc3 == PLUGIN_OK)
      PowerPortsSP[POWER_PORT_3].setState(bOn ? ISS_ON : ISS_OFF);

    rc4 = m_AStarBoxPort.getPortStatus(POWER_4, bOn);
    if (rc4 == PLUGIN_OK)
      PowerPortsSP[POWER_PORT_4].setState(bOn ? ISS_ON : ISS_OFF);
    
    // Now set the status light 
    PowerPortsSP.setState(rc1 == PLUGIN_OK && rc2 == PLUGIN_OK &&
			    rc3 == PLUGIN_OK && rc2 == PLUGIN_OK ? IPS_OK: IPS_ALERT);
    // Make changes to the display
    PowerPortsSP.apply();

    // Now update Dewheater levels
    rc1 = m_AStarBoxPort.getPortPWMDutyCyclePercent(PWM_1, pwmpercent);
    if (rc1 == PLUGIN_OK)
      DewPWMNP[DEW_PWM_1].setValue(pwmpercent);

    rc2 = m_AStarBoxPort.getPortPWMDutyCyclePercent(PWM_2, pwmpercent);
    if (rc2 == PLUGIN_OK)
      DewPWMNP[DEW_PWM_2].setValue(pwmpercent);

    // Update Status Light
    DewPWMNP.setState(rc1 == PLUGIN_OK && rc2 == PLUGIN_OK ? IPS_OK :
		      IPS_ALERT);

    // Apply changes to the display
    DewPWMNP.apply();

    // Now read voltage
    voltage = m_AStarBoxPort.getVoltage();
    PowerSensorsNP[SENSOR_VOLTAGE].setValue(voltage);
    PowerSensorsNP.setState(IPS_OK);
    PowerSensorsNP.apply();
    return true;
}
