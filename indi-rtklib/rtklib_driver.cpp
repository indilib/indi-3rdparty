/*******************************************************************************
  Copyright(c) 2020 Ilia Platone - Jasem Mutlaq. All rights reserved.

  INDI RTKLIB Driver

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

#include "rtklib_driver.h"
#include "rtkrcv_parser.h"

#include "config.h"

#include <connectionplugins/connectiontcp.h>
#include <indicom.h>
#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>

#include <memory>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#define MAX_RTKRCV_PARSES     50              // Read 50 streams before giving up
#define MAX_TIMEOUT_COUNT   5               // Maximum timeout before auto-connect

// We declare an auto pointer to GPSD.
static std::unique_ptr<RTKLIB> rtkrcv(new RTKLIB());

RTKLIB::RTKLIB()
{
    setVersion(RTKLIB_VERSION_MAJOR, RTKLIB_VERSION_MINOR);
}

const char *RTKLIB::getDefaultName()
{
    return "RTKLIB Precise Positioning";
}

bool RTKLIB::initProperties()
{
    // We init parent properties first
    INDI::GPS::initProperties();

    IUFillText(&GPSstatusT[0], "GPS_FIX", "Fix Mode", nullptr);
    IUFillTextVector(&GPSstatusTP, GPSstatusT, 1, getDeviceName(), "GPS_STATUS", "GPS Status", MAIN_CONTROL_TAB, IP_RO,
                     60, IPS_IDLE);

    tcpConnection = new Connection::TCP(this);
    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(50000);
    tcpConnection->registerHandshake([&]()
    {
        PortFD = tcpConnection->getPortFD();
        return is_rtkrcv();
    });

    registerConnection(tcpConnection);

    addDebugControl();

    setDriverInterface(GPS_INTERFACE | AUX_INTERFACE);

    return true;
}

bool RTKLIB::updateProperties()
{
    // Call parent update properties first
    INDI::GPS::updateProperties();

    if (isConnected())
    {
        defineProperty(&GPSstatusTP);

        pthread_create(&rtkThread, nullptr, &RTKLIB::parse_rtkrcv_helper, this);
    }
    else
    {
        // We're disconnected
        deleteProperty(GPSstatusTP.name);
    }
    return true;
}

IPState RTKLIB::updateGPS()
{
    IPState rc = IPS_BUSY;

    pthread_mutex_lock(&lock);
    if (locationPending == false && timePending == false)
    {
        rc = IPS_OK;
        locationPending = true;
        timePending = true;
    }
    pthread_mutex_unlock(&lock);

    return rc;
}

bool RTKLIB::is_rtkrcv()
{
    char line[RTKRCV_MAX_LENGTH];

    int bytes_read = 0;
    int tty_rc = tty_nread_section(PortFD, line, RTKRCV_MAX_LENGTH, 0xC, 3, &bytes_read);
    if (tty_rc < 0)
    {
        LOGF_ERROR("Error getting device readings: %s", strerror(errno));
        return false;
    }
    line[bytes_read] = '\0';

    return true;
}

void* RTKLIB::parse_rtkrcv_helper(void *obj)
{
    static_cast<RTKLIB*>(obj)->parse_rtkrcv();
    return nullptr;
}

void RTKLIB::parse_rtkrcv()
{
    static char ts[32] = {0};

    char line[RTKRCV_MAX_LENGTH];

    while (isConnected())
    {
        int bytes_read = 0;
        int tty_rc = tty_nread_section(PortFD, line, RTKRCV_MAX_LENGTH, 0xC, 3, &bytes_read);
        if (tty_rc < 0)
        {
            if (tty_rc == TTY_OVERFLOW)
            {
                LOG_WARN("Overflow detected. Possible remote GPS disconnection. Disconnecting driver...");
                INDI::GPS::setConnected(false);
                updateProperties();
                break;
            }
            else
            {
                char errmsg[MAXRBUF];
                tty_error_msg(tty_rc, errmsg, MAXRBUF);
                if (tty_rc == TTY_TIME_OUT || errno == ECONNREFUSED)
                {
                    if (errno == ECONNREFUSED)
                    {
                        // sleep for 10 seconds
                        tcpConnection->Disconnect();
                        usleep(10 * 1e6);
                        tcpConnection->Connect();
                        PortFD = tcpConnection->getPortFD();
                    }
                    else if (timeoutCounter++ > MAX_TIMEOUT_COUNT)
                    {
                        LOG_WARN("Timeout limit reached, reconnecting...");

                        tcpConnection->Disconnect();
                        // sleep for 5 seconds
                        usleep(5 * 1e6);
                        tcpConnection->Connect();
                        PortFD = tcpConnection->getPortFD();
                        timeoutCounter = 0;
                    }
                }
                continue;
            }
        }
        line[bytes_read] = '\0';

        LOGF_DEBUG("%s", line);
        char flags;
        char type;
        double enu[3];
        double timestamp;
        rtkrcv_fix_status fix;
        scansolution(line, &flags, &type, enu, &fix, &timestamp);
        switch (fix)
        {
            case status_no_fix:
                LOG_DEBUG("no fix");
                break;
            case status_float:
                LOG_DEBUG("float fix");
                break;
            case status_sbas:
                LOG_DEBUG("sbas fix");
                break;
            case status_dgps:
                LOG_DEBUG("dgps fix");
                break;
            case status_single:
                LOG_DEBUG("single fix");
                break;
            case status_ppp:
                LOG_DEBUG("ppp fix");
                break;
            case status_unknown:
                LOG_DEBUG("unknown fix status");
                break;
            case status_fix:
            {
                LocationNP[LOCATION_LATITUDE].value  = enu[0];
                LocationNP[LOCATION_LONGITUDE].value = enu[1];
                LocationNP[LOCATION_ELEVATION].value = enu[2];
                if (LocationNP[LOCATION_LONGITUDE].value < 0)
                    LocationNP[LOCATION_LONGITUDE].value += 360;

                struct timespec timesp;
                time_t raw_time;
                struct tm *utc, *local;

                timesp.tv_sec = (time_t)timestamp;
                timesp.tv_nsec = (time_t)(timestamp * 1000000000.0);

                raw_time = timesp.tv_sec;
                m_GPSTime = raw_time;
                utc = gmtime(&raw_time);
                strftime(ts, 32, "%Y-%m-%dT%H:%M:%S", utc);
                TimeTP[0].setText(ts);

                setSystemTime(raw_time);

                local = localtime(&raw_time);
                snprintf(ts, 32, "%4.2f", (local->tm_gmtoff / 3600.0));
                TimeTP[1].setText(ts);

                pthread_mutex_lock(&lock);
                locationPending = false;
                timePending = false;
                LOG_DEBUG("Threaded Location and Time updates complete.");
                pthread_mutex_unlock(&lock);
                break;

                default:
                {
                    LOG_DEBUG("solution is not parsed");
                }
                break;
            }
        }
    }

    pthread_exit(nullptr);
}
