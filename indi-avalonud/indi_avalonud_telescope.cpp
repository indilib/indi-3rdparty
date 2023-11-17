/*
    Avalon Unified Driver Telescope

    Copyright (C) 2020

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

#include "config.h"

#include "indicom.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>
#include <json.h>
#include <chrono>
#include <pthread.h>
#include <zmq.h>

#include "indi_avalonud_telescope.h"


using json = nlohmann::json;


static char device_str[MAXINDIDEVICE] = "AvalonUD Telescope";


static std::unique_ptr<AUDTELESCOPE> telescope(new AUDTELESCOPE());


void ISInit()
{
    static int isInit =0;

    if (isInit == 1)
        return;

    isInit = 1;
    if(telescope.get() == 0) telescope.reset(new AUDTELESCOPE());
}

void ISGetProperties(const char *dev)
{
    telescope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    telescope->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    telescope->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    telescope->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(num);
}

void ISSnoopDevice (XMLEle *root)
{
    telescope->ISSnoopDevice(root);
}

/****************************************************************
**
**
*****************************************************************/
AUDTELESCOPE::AUDTELESCOPE()
{
    setVersion(AVALONUD_VERSION_MAJOR,AVALONUD_VERSION_MINOR);

    context = zmq_ctx_new();
    setTelescopeConnection( CONNECTION_NONE );
    SetTelescopeCapability( TELESCOPE_CAN_GOTO |
                            TELESCOPE_CAN_SYNC |
                            TELESCOPE_CAN_PARK |
                            TELESCOPE_CAN_ABORT |
                            TELESCOPE_HAS_TIME |
                            TELESCOPE_HAS_LOCATION |
                            TELESCOPE_HAS_TRACK_MODE |
                            TELESCOPE_CAN_CONTROL_TRACK |
                            TELESCOPE_HAS_TRACK_RATE |
                            TELESCOPE_HAS_PIER_SIDE,
                            4 );
    SetParkDataType(PARK_HA_DEC);
}

/****************************************************************
**
**
*****************************************************************/
AUDTELESCOPE::~AUDTELESCOPE()
{
//    zmq_ctx_term(context);
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::initProperties()
{
    INDI::Telescope::initProperties();

    IUFillText(&ConfigT[0], "ADDRESS", "Address", "127.0.0.1");
    IUFillTextVector(&ConfigTP, ConfigT, 1, getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&MountTypeS[MOUNT_EQUATORIAL], "MOUNT_EQUATORIAL", "Equatorial", ISS_OFF);
    IUFillSwitch(&MountTypeS[MOUNT_ALTAZ], "MOUNT_ALTAZ", "AltAz", ISS_OFF);
    IUFillSwitchVector(&MountTypeSP, MountTypeS, 2, getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&LocalEqN[0],"HA","HA (hh:mm:ss)","%010.6m",-12,12,0,0);
    IUFillNumber(&LocalEqN[1],"DEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&LocalEqNP, LocalEqN,2, getDeviceName(), "LOCAL_EQUATORIAL_EOD_COORD","Local Eq. Coordinates", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AzAltN[0],"Az","Az (deg)","%.2f",-180,180,0,0);
    IUFillNumber(&AzAltN[1],"Alt","Alt (deg)","%.2f",-90,90,0,0);
    IUFillNumberVector(&AzAltNP, AzAltN, 2, getDeviceName(), "AZALT_EOD_COORD", "Azimuthal Coordinates", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&HomeS[0],"SYNCHOME","Sync Home position",ISS_OFF);
    IUFillSwitch(&HomeS[1],"SLEWHOME","Slew to Home position",ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 2, getDeviceName(), "TELESCOPE_HOME","Home", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&TimeN[0],"JD","JD (days)","%.6f",0,0,0,0);
    IUFillNumber(&TimeN[1],"UTC","UTC (hh:mm:ss)","%09.6m",0,24,0,0);
    IUFillNumber(&TimeN[2],"LST","LST (hh:mm:ss)","%09.6m",0,24,0,0);
    IUFillNumberVector(&TimeNP, TimeN, 3, getDeviceName(), "TELESCOPE_TIME","Time", SITE_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&ParkOptionS[PARK_CURRENT], "PARK_CURRENT", "Set Park (Current)", ISS_OFF);
    IUFillSwitch(&ParkOptionS[PARK_DEFAULT], "PARK_DEFAULT", "Restore Park (Default)", ISS_OFF);
    IUFillSwitchVector(&ParkOptionSP, ParkOptionS, 2, getDeviceName(), "TELESCOPE_PARK_OPTION", "Park Options", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Since we have 4 slew rates, let's fill them out
    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTER", "Center", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Add Tracking Modes. If you have SOLAR, LUNAR..etc, add them here as well.
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Mount Meridian Flip
    IUFillSwitch(&MeridianFlipS[FLIP_ON], "FLIP_ON", "On", ISS_OFF);
    IUFillSwitch(&MeridianFlipS[FLIP_OFF], "FLIP_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&MeridianFlipSP, MeridianFlipS, 2, getDeviceName(), "MOUNT_MERIDIAN_FLIP", "Mount Meridian Flip", SITE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Mount Meridian Flip HA
    IUFillNumber(&MeridianFlipHAN[0], "FLIP_HA", "Flip HA (deg)", "%.2f", -30.0, 30.0, 0.1, 0.0);
    IUFillNumberVector(&MeridianFlipHANP, MeridianFlipHAN, 1, getDeviceName(), "MOUNT_MERIDIAN_FLIP_HA", "Mount Meridian Flip HA", SITE_TAB, IP_RW, 60, IPS_IDLE);

    // HW type
    IUFillText(&HWTypeT[0], "HW_TYPE", "Controller Type", "");
    IUFillTextVector(&HWTypeTP, HWTypeT, 1, getDeviceName(), "HW_TYPE_INFO", "Type", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // HW info
    IUFillText(&HWModelT[0], "HW_MODEL", "Mount Model", "");
    IUFillTextVector(&HWModelTP, HWModelT, 1, getDeviceName(), "HW_MODEL_INFO", "Model", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // HW identifier
    IUFillText(&HWIdentifierT[0], "HW_IDENTIFIER", "HW Identifier", "");
    IUFillTextVector(&HWIdentifierTP, HWIdentifierT, 1, getDeviceName(), "HW_IDENTIFIER_INFO", "Identifier", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // high level info
    IUFillText(&HighLevelSWT[0], "HLSW_NAME", "Name", "");
    IUFillText(&HighLevelSWT[1], "HLSW_VERSION", "Version", "--");
    IUFillTextVector(&HighLevelSWTP, HighLevelSWT, 2, getDeviceName(), "HLSW_INFO", "HighLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // low level SW info
    IUFillText(&LowLevelSWT[0], "LLSW_NAME", "Name", "");
    IUFillText(&LowLevelSWT[1], "LLSW_VERSION", "Version", "--");
    IUFillTextVector(&LowLevelSWTP, LowLevelSWT, 2, getDeviceName(), "LLSW_INFO", "LowLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    TrackState = SCOPE_IDLE;
    previousTrackState = SCOPE_IDLE;
    fTracking = false;
    fFirstTime = true;
    lastErrorMsg = NULL;

    SetTrackMode(TRACK_SIDEREAL);
    trackspeedra = TRACKRATE_SIDEREAL;
    trackspeeddec = 0;

    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addDebugControl();
    addConfigurationControl();

    pthread_mutex_init( &connectionmutex, NULL );

    return true;
}

/****************************************************************
**
**
*****************************************************************/
void AUDTELESCOPE::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);

    defineProperty(&ConfigTP);
    loadConfig(true,ConfigTP.name);

    deleteProperty(ParkOptionS[2].name);
    deleteProperty(ParkOptionS[3].name);
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::updateProperties()
{
    if (isConnected())
    {
        defineProperty(&HomeSP);
    }
    else
    {
        deleteProperty(HomeSP.name);
    }

    INDI::Telescope::updateProperties();
    if (isConnected())
    {
        defineProperty(&MountTypeSP);
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);

        defineProperty(&LocalEqNP);
        defineProperty(&AzAltNP);
        defineProperty(&TimeNP);
        defineProperty(&MeridianFlipSP);
        defineProperty(&MeridianFlipHANP);
        defineProperty(&HWTypeTP);
        defineProperty(&HWModelTP);
        defineProperty(&HWIdentifierTP);
        defineProperty(&HighLevelSWTP);
        defineProperty(&LowLevelSWTP);
    }
    else
    {
        deleteProperty(MountTypeSP.name);
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);

        deleteProperty(LocalEqNP.name);
        deleteProperty(AzAltNP.name);
        deleteProperty(TimeNP.name);
        deleteProperty(MeridianFlipSP.name);
        deleteProperty(MeridianFlipHANP.name);
        deleteProperty(HWTypeTP.name);
        deleteProperty(HWModelTP.name);
        deleteProperty(HWIdentifierTP.name);
        deleteProperty(HighLevelSWTP.name);
        deleteProperty(LowLevelSWTP.name);
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::Connect()
{
    char *answer;
    char addr[1024];
    int timeout = 500;


    if (isConnected())
        return true;

    IPaddress = strdup(ConfigT[0].text);

    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to connect %s telescope...",IPaddress);

    requester = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(requester, ZMQ_RCVTIMEO, &timeout, sizeof(timeout) );
    snprintf( addr, sizeof(addr), "tcp://%s:5451", IPaddress);
    zmq_connect(requester, addr);

    answer = sendRequest("ASTRO_INFO");
    if ( answer ) {
        json j;
        std::string sHWt,sHWm,sHWi,sLLSW,sLLSWv,sHLSW,sHLSWv;

        j = json::parse(answer,nullptr,false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("HWType") ||
                !j.contains("HWModel") ||
                !j.contains("HWIdentifier") ||
                !j.contains("lowLevelSW") ||
                !j.contains("lowLevelSWVersion") ||
                !j.contains("highLevelSW") ||
                !j.contains("highLevelSWVersion") )
        {
            zmq_close(requester);
            DEBUGF(INDI::Logger::DBG_ERROR, "Communication with %s telescope failed",IPaddress);
            free(IPaddress);
            return false;
        }

        j["HWType"].get_to(sHWt);
        IUSaveText(&HWTypeT[0], sHWt.c_str());
        IDSetText(&HWTypeTP, nullptr);
        j["HWModel"].get_to(sHWm);
        IUSaveText(&HWModelT[0], sHWm.c_str());
        IDSetText(&HWModelTP, nullptr);
        j["HWIdentifier"].get_to(sHWi);
        IUSaveText(&HWIdentifierT[0], sHWi.c_str());
        IDSetText(&HWIdentifierTP, nullptr);
        j["lowLevelSW"].get_to(sLLSW);
        IUSaveText(&LowLevelSWT[0], sLLSW.c_str());
        j["lowLevelSWVersion"].get_to(sLLSWv);
        IUSaveText(&LowLevelSWT[1], sLLSWv.c_str());
        IDSetText(&LowLevelSWTP, nullptr);
        j["highLevelSW"].get_to(sHLSW);
        IUSaveText(&HighLevelSWT[0], sHLSW.c_str());
        j["highLevelSWVersion"].get_to(sHLSWv);
        IUSaveText(&HighLevelSWT[1], sHLSWv.c_str());
        IDSetText(&HighLevelSWTP, nullptr);
    } else {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope",IPaddress);
        free(IPaddress);
        return false;
    }

    answer = sendRequest("ASTRO_GETMERIDIANFLIPHA");
    if ( answer && !strncmp(answer,"OK:",3) ) {
        MeridianFlipHAN[0].value = atof(answer+3);
        free( answer );
        IDSetNumber(&MeridianFlipHANP, nullptr);
    } else {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope",IPaddress);
        free(IPaddress);
        return false;
    }

    answer = sendRequest("ASTRO_GETMOUNTMODE");
    if ( answer && !strncmp(answer,"OK:",3) ) {
        if ( !strcmp(answer,"OK:ALTAZ") )
            mounttype = 1;
        else
            mounttype = 0;
        free( answer );
        MountTypeS[mounttype].s = ISS_ON;
        MountTypeS[(mounttype?0:1)].s = ISS_OFF;
        MountTypeSP.s = IPS_OK;
        MountTypeSP.p = IP_RO;
        IDSetSwitch(&MountTypeSP, nullptr);
    } else {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope",IPaddress);
        free(IPaddress);
        return false;
    }

    answer = sendRequest("ASTRO_GETLOCATION");
    if ( answer ) {
        json j;

        j = json::parse(answer,nullptr,false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("longitude") ||
                !j.contains("latitude") ||
                !j.contains("elevation") )
        {
            zmq_close(requester);
            DEBUGF(INDI::Logger::DBG_ERROR, "Communication with %s telescope failed",IPaddress);
            free(IPaddress);
            return false;
        }
        j["longitude"].get_to(LocationN[LOCATION_LONGITUDE].value);
        j["latitude"].get_to(LocationN[LOCATION_LATITUDE].value);
        j["elevation"].get_to(LocationN[LOCATION_ELEVATION].value);
        IDSetNumber(&LocationNP, nullptr);
    } else {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope",IPaddress);
        free(IPaddress);
        return false;
    }

    northernHemisphere = 1;

    slewState = IPS_IDLE;

    tid = SetTimer(getCurrentPollingPeriod());

    DEBUGF(INDI::Logger::DBG_SESSION, "Successfully connected %s telescope",IPaddress);
    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::Disconnect()
{
    if (!isConnected())
        return true;

    DEBUG(INDI::Logger::DBG_SESSION, "Attempting to disconnect telescope...");

    zmq_close(requester);

    RemoveTimer( tid );

    free(IPaddress);

    DEBUG(INDI::Logger::DBG_SESSION, "Successfully disconnected telescope");

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev,getDeviceName()))
    {
        // TCP Server settings
        if (!strcmp(name, ConfigTP.name))
        {
            IUUpdateText(&ConfigTP, texts, names, n);
            ConfigTP.s = IPS_OK;
            IDSetText(&ConfigTP, nullptr);
            if (isConnected() && strcmp(IPaddress,ConfigT[0].text) )
                DEBUG(INDI::Logger::DBG_WARNING, "Disconnect and reconnect to make IP address change effective!");
            return true;
        }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool AUDTELESCOPE::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(!strcmp(dev,getDeviceName()))
    {

        // Meridian Flip HourAngle
        if(!strcmp(MeridianFlipHANP.name,name))
        {
            MeridianFlipHANP.s = IPS_BUSY;
            IUUpdateNumber(&MeridianFlipHANP, values, names, n);

            if ( isConnected() )
            {
                if ( setMeridianFlipHA(values[0]) )
                    MeridianFlipHANP.s = IPS_OK;
                else
                    MeridianFlipHANP.s = IPS_ALERT;
            }
            IDSetNumber(&MeridianFlipHANP, nullptr);
            return true;
        }

        // Guiding
        if (!strcmp(name, GuideNSNP.name) || !strcmp(name, GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool AUDTELESCOPE::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev,getDeviceName()))
    {

        // Home
        if (!strcmp(HomeSP.name, name))
        {
            IUUpdateSwitch(&HomeSP, states, names, n);
            int index = IUFindOnSwitchIndex(&HomeSP);

            HomeSP.s = IPS_BUSY;
            IUResetSwitch(&HomeSP);

            if ( isConnected() )
            {
                switch (index) {
                case 0 :
                    if ( SyncHome() )
                        HomeSP.s = IPS_OK;
                    else
                        HomeSP.s = IPS_ALERT;
                    break;
                case 1 :
                    if ( SlewToHome() )
                        HomeSP.s = IPS_OK;
                    else
                        HomeSP.s = IPS_ALERT;
                    break;
                }
            }
            IDSetSwitch(&HomeSP,NULL);
            return true;
        }

        // Meridian flip
        if (!strcmp(MeridianFlipSP.name, name))
        {
            MeridianFlipSP.s = IPS_BUSY;
            IUUpdateSwitch(&MeridianFlipSP, states, names, n);

            if ( isConnected() )
            {
                int targetState = IUFindOnSwitchIndex(&MeridianFlipSP);
                if ( meridianFlipEnable((targetState==0)?true:false) )
                    MeridianFlipSP.s = IPS_OK;
                else
                    MeridianFlipSP.s = IPS_ALERT;
            }
            IDSetSwitch(&MeridianFlipSP,NULL);
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool AUDTELESCOPE::updateLocation(double latitude, double longitude, double elevation)
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Location update called before driver connection");
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION,"Location update ..." );
    answer = sendCommand("ASTRO_SETLOCATION %.8f %.8f %.1f",longitude,latitude,elevation);
    if ( !answer ) {
        DEBUG(INDI::Logger::DBG_SESSION,"Location update completed" );
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Location update failed due to %s", answer );
    free(answer);
    return false;
}

bool AUDTELESCOPE::updateTime(ln_date *utc, double utc_offset)
{
    char *answer, buffer[256];

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Time update called before driver connection");
        return false;
    }

    INDI_UNUSED(utc_offset);

    snprintf( buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%08.6fZ", utc->years, utc->months, utc->days, utc->hours, utc->minutes, utc->seconds );
    DEBUGF(INDI::Logger::DBG_SESSION,"Time update to %s ...", buffer );
    answer = sendCommandOnce("ASTRO_SETUTCDATE %s",buffer);
    if ( !answer ) {
        DEBUG(INDI::Logger::DBG_SESSION,"Time update completed" );
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Time update to %s failed due to %s", buffer, answer );
    free(answer);
    return false;
}

bool AUDTELESCOPE::ReadScopeStatus()
{
    char *answer;
    int sts,pierside,exposureready,meridianflip;
    double utc,lst,jd,ha,ra,dec,az,alt,meridianflipha;


    answer = sendRequest("ASTRO_STATUS");
    if ( answer ) {
        json j;

        j = json::parse(answer,nullptr,false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("UTC") ||
                !j.contains("JD") ||
                !j.contains("LST") ||
                !j.contains("HA") ||
                !j.contains("RA") ||
                !j.contains("Dec") ||
                !j.contains("Az") ||
                !j.contains("Alt") ||
                !j.contains("globalStatus") ||
                !j.contains("meridianFlip") ||
                !j.contains("pierSide") ||
                !j.contains("meridianFlipHA") ||
                !j.contains("exposureReady") )
        {
            DEBUG(INDI::Logger::DBG_WARNING,"Status communication error");
            return false;
        }
        j["UTC"].get_to(utc);
        j["JD"].get_to(jd);
        j["LST"].get_to(lst);
        j["HA"].get_to(ha);
        j["RA"].get_to(ra);
        j["Dec"].get_to(dec);
        j["Az"].get_to(az);
        j["Alt"].get_to(alt);
        j["globalStatus"].get_to(sts);
        j["meridianFlip"].get_to(meridianflip);
        j["pierSide"].get_to(pierside);
        j["meridianFlipHA"].get_to(meridianflipha);
        j["exposureReady"].get_to(exposureready);
        if ( j.contains("errorMsg") ) {
            std::string msg;
            j["errorMsg"].get_to(msg);
            if ( msg.length() > 0 ) {
                if ( !lastErrorMsg || ( lastErrorMsg && strcmp(msg.c_str(),lastErrorMsg) ) ) {
                    // the error message is written only once until it changes
                    DEBUGF(INDI::Logger::DBG_WARNING,"Failed due to %s",msg.c_str());
                    if ( lastErrorMsg )
                        free( lastErrorMsg );
                    lastErrorMsg = strdup(msg.c_str());
                }
            } else {
                if ( lastErrorMsg )
                    free( lastErrorMsg );
                lastErrorMsg = NULL;
            }
        } else {
            if ( lastErrorMsg )
                free( lastErrorMsg );
            lastErrorMsg = NULL;
        }

        previousTrackState = TrackState;

        NewRaDec( ra, dec );
        switch ( sts ) {
        case 0 :
            TrackState = SCOPE_IDLE;
            break;
        case 1 :
            TrackState = SCOPE_SLEWING;
            break;
        case 2 :
            TrackState = SCOPE_TRACKING;
            slewState = IPS_IDLE;
            break;
        case 3 :
            TrackState = SCOPE_PARKING;
            break;
        case 4 :
            TrackState = SCOPE_PARKED;
            break;
        }

        if ( fFirstTime ) {
            SetParked( (TrackState==SCOPE_PARKED) );
            fFirstTime = false;
        } else if ( previousTrackState != TrackState ) {
            if ( TrackState == SCOPE_PARKED )
                SetParked(true);
            else if ( previousTrackState == SCOPE_PARKED )
                SetParked(false);
        }

        if ( pierside >= 0 ) {
            if ( meridianflip && ( MeridianFlipS[0].s == ISS_OFF ) ) {
                MeridianFlipS[0].s = ISS_ON;
                MeridianFlipS[1].s = ISS_OFF;
            }
            if ( !meridianflip && ( MeridianFlipS[0].s == ISS_ON ) ) {
                MeridianFlipS[0].s = ISS_OFF;
                MeridianFlipS[1].s = ISS_ON;
            }
        } else {
            MeridianFlipS[0].s = ISS_OFF;
            MeridianFlipS[1].s = ISS_ON;
        }
        IDSetSwitch(&MeridianFlipSP, NULL);

        MeridianFlipHAN[0].value = meridianflipha;
        IDSetNumber(&MeridianFlipHANP, NULL);
        setPierSide((TelescopePierSide)pierside);

        if (LocalEqN[0].value != ha || LocalEqN[1].value != dec || LocalEqNP.s != EqNP.s)
        {
            LocalEqN[0].value=ha;
            LocalEqN[1].value=dec;
            LocalEqNP.s = slewState;
            IDSetNumber(&LocalEqNP, NULL);
        }

        if (AzAltN[0].value != az || AzAltN[1].value != alt || AzAltNP.s != EqNP.s)
        {
            AzAltN[0].value=az;
            AzAltN[1].value=alt;
            AzAltNP.s = slewState;
            IDSetNumber(&AzAltNP, NULL);
        }

        if (TimeN[0].value != utc || TimeN[1].value != jd || TimeN[2].value != lst)
        {
            TimeN[0].value=jd;
            TimeN[1].value=utc;
            TimeN[2].value=lst;
            TimeNP.s = IPS_IDLE;
            IDSetNumber(&TimeNP, NULL);
        }

        return true;
    }

    return false;
}

bool AUDTELESCOPE::meridianFlipEnable(int enable)
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Set meridian flip called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Set meridian flip to %s ...",(enable?"ENABLED":"DISABLED"));
    answer = sendCommand("ASTRO_SETMERIDIANFLIP %d",enable);
    if ( !answer ) {
        DEBUGF(INDI::Logger::DBG_SESSION,"Set meridian flip to %s completed",(enable?"ENABLED":"DISABLED"));
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Set meridian flip to %s failed due to %s",(enable?"ENABLED":"DISABLED"),answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::setMeridianFlipHA(double angle)
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Set meridian flip HA called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Set meridian flip HA to %.3fdeg ...",angle);
    answer = sendCommand("ASTRO_SETMERIDIANFLIPHA %.4f",angle);
    if ( !answer ) {
        DEBUGF(INDI::Logger::DBG_SESSION,"Set meridian flip HA to %.3fdeg completed",angle);
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Set meridian flip HA to %.3fdeg failed due to %s",angle,answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::Sync(double ra, double dec)
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Sync called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Sync to RA:%.3fhours Dec:%.3fdeg ...",ra,dec);
    answer = sendCommand("ASTRO_SYNC %.8f %.8f",ra,dec);
    if ( !answer ) {
        DEBUGF(INDI::Logger::DBG_SESSION,"Sync to RA:%.3fhours Dec:%.3fdeg completed",ra,dec);
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Sync to RA:%.3fhours Dec:%.3fdeg failed due to %s",ra,dec,answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::Park()
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Start telescope park called before driver connection");
        return false;
    }

    TrackState = SCOPE_PARKING;
    LOG_INFO("Start telescope park...");
    answer = sendCommand("ASTRO_PARK");
    if ( !answer ) {
        ParkSP.s = IPS_BUSY;
        TrackState = SCOPE_PARKING;
        DEBUG(INDI::Logger::DBG_SESSION,"Start telescope park completed");
        return true;
    }
    ParkSP.s = IPS_ALERT;
    TrackState = SCOPE_IDLE;
    DEBUGF(INDI::Logger::DBG_WARNING,"Start telescope park failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::UnPark()
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Unparking telescope called before driver connection");
        return false;
    }

    LOG_INFO("Unparking telescope...");
    answer = sendCommand("ASTRO_UNPARK");
    if ( !answer ) {
        SetParked( false );
        DEBUG(INDI::Logger::DBG_SESSION,"Unparking telescope completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Unparking telescope failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetCurrentPark()
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Set park position called before driver connection");
        return false;
    }

    LOG_INFO("Set park position...");
    answer = sendCommand("ASTRO_SETPARK");
    if ( !answer ) {
        DEBUG(INDI::Logger::DBG_SESSION,"Set park position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Set park position failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetDefaultPark()
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Restore park position called before driver connection");
        return false;
    }

    LOG_INFO("Restore park position...");
    answer = sendCommand("ASTRO_RESTOREPARK");
    if ( !answer ) {
        DEBUG(INDI::Logger::DBG_SESSION,"Restore park position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Restore park position failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SyncHome()
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Sync home position called before driver connection");
        return false;
    }

    LOG_INFO("Sync home position...");
    answer = sendCommand("ASTRO_SYNCHOME");
    if ( !answer ) {
        DEBUG(INDI::Logger::DBG_SESSION,"Sync home position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Sync home position failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SlewToHome()
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Slew to home position called before driver connection");
        return false;
    }

    LOG_INFO("Slew to home position...");
    answer = sendCommand("ASTRO_POINTHOME");
    if ( !answer ) {
        DEBUG(INDI::Logger::DBG_SESSION,"Slew to home position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Slew to home position failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::Goto(double ra,double dec)
{
    ISwitch *sw;

    sw = IUFindSwitch(&CoordSP, "TRACK");
    if ((sw != nullptr) && (sw->s == ISS_ON)) {
        return Slew(ra,dec,true);
    } else {
        return Slew(ra,dec,false);
    }
}

bool AUDTELESCOPE::Slew(double ra,double dec,int track)
{
    char *answer = NULL;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Start telescope slew called before driver connection");
        return false;
    }

    slewState = IPS_BUSY;
    DEBUGF(INDI::Logger::DBG_SESSION,"Start telescope slew to RA:%.4fhours Dec:%.3fdeg and %stracking ...",ra,dec,(track?"":"NO "));
    if ( track ) {
        // point and track
        answer = sendCommand("ASTRO_POINT %.8f %.8f %.8f %.8f",ra,dec,trackspeedra/3600.0,trackspeeddec/3600.0);
    } else {
        // point only
        answer = sendCommand("ASTRO_POINT %.8f %.8f 0 0",ra,dec);
    }
    if ( !answer ) {
        targetRA = ra;
        targetDEC = dec;
        TrackState = SCOPE_SLEWING;
        slewState = IPS_OK;
        DEBUGF(INDI::Logger::DBG_SESSION,"Start telescope slew to RA:%.4fhours Dec:%.3fdeg and %stracking completed",ra,dec,(track?"":"NO "));
        return true;
    }
    TrackState = SCOPE_IDLE;
    slewState = IPS_ALERT;
    DEBUGF(INDI::Logger::DBG_WARNING,"Start telescope slew to RA:%.4fhours Dec:%.3fdeg and %stracking failed due to %s",ra,dec,(track?"":"NO "),answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetTrackMode(uint8_t mode)
{
    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Set tracking mode called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Set tracking mode to %d...",mode);
    switch (mode) {
    case TRACK_SIDEREAL:
        trackspeedra = TRACKRATE_SIDEREAL;
        trackspeeddec = 0;
        break;
    case TRACK_SOLAR:
        trackspeedra = TRACKRATE_SOLAR;
        trackspeeddec = 0;
        break;
    case TRACK_LUNAR:
        trackspeedra = TRACKRATE_LUNAR;
        trackspeeddec = 0;
        break;
    case TRACK_CUSTOM:
        trackspeedra = TrackRateN[AXIS_RA].value;
        trackspeeddec = TrackRateN[AXIS_DE].value;
        break;
    }
    if ( TrackState == SCOPE_TRACKING )
        SetTrackRate( trackspeedra, trackspeeddec );
    DEBUGF(INDI::Logger::DBG_SESSION,"Set tracking mode to %d completed",mode);

    return true;
}

bool AUDTELESCOPE::SetTrackRate(double raRate, double deRate)
{
    char *answer;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Tracking change called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Tracking change to RA:%f\"/s Dec:%f\"/s ...",raRate,deRate);
    TrackStateSP.s = IPS_BUSY;
    answer = sendCommand("ASTRO_TRACK %.8f %.8f",raRate/3600.0,deRate/3600.0);
    if ( !answer ) {
        if ( ( raRate == 0 ) && ( deRate == 0 ) )
            TrackStateSP.s = IPS_IDLE;
        else
            TrackStateSP.s = IPS_OK;
        DEBUGF(INDI::Logger::DBG_SESSION,"Tracking change to RA:%f\"/s Dec:%f\"/s completed",raRate,deRate);
        return true;
    }
    TrackStateSP.s = IPS_ALERT;
    DEBUGF(INDI::Logger::DBG_WARNING,"Tracking change to RA:%f\"/s Dec:%f\"/s failed due to %s",raRate,deRate,answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetTrackEnabled(bool enabled)
{
    int rc;

    DEBUGF(INDI::Logger::DBG_SESSION,"Change tracking to %s...",((enabled)?"ENABLED":"DISABLED"));
    if ( enabled ) {
        int mode = IUFindOnSwitchIndex(&TrackModeSP);
        SetTrackMode(mode);
        if ( TrackState != SCOPE_TRACKING )
            rc = SetTrackRate( trackspeedra, trackspeeddec );
        else
            rc = true;
    } else {
        rc = SetTrackRate( 0, 0 );
    }
    DEBUGF(INDI::Logger::DBG_SESSION,"Change tracking to %s completed",((enabled)?"ENABLED":"DISABLED"));
    return rc;
}

bool AUDTELESCOPE::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    char *answer = NULL;
    string speed[] = {"SLEWGUIDE","SLEWCENTER","SLEWFIND","SLEWMAX"};
    int speedIndex;


    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "MoveNS called before driver connection");
        return false;
    }

    speedIndex = IUFindOnSwitchIndex(&SlewRateSP);
    MovementNSSP.s = IPS_BUSY;
    if ( command == MOTION_START ) {
        // force tracking after motion
        fTracking = true;
        if ( dir == DIRECTION_NORTH )
            answer = sendCommand("ASTRO_SLEW * (%.8f+%s)",trackspeeddec/3600.0,speed[speedIndex].c_str());
        else
            answer = sendCommand("ASTRO_SLEW * (%.8f-%s)",trackspeeddec/3600.0,speed[speedIndex].c_str());
    } else {
        if ( fTracking )
            answer = sendCommand("ASTRO_TRACK * %.8f",trackspeeddec/3600.0);
        else
            answer = sendCommand("ASTRO_SLEW * 0");
    }
    if ( !answer ) {
        if ( command == MOTION_START )
            MovementNSSP.s = IPS_OK;
        else
            MovementNSSP.s = IPS_IDLE;
        return true;
    }
    MovementNSSP.s = IPS_ALERT;
    DEBUGF(INDI::Logger::DBG_WARNING,"MoveNS command failed due to %s",answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    char *answer = NULL;
    string speed[] = {"SLEWGUIDE","SLEWCENTER","SLEWFIND","SLEWMAX"};
    int speedIndex;


    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "MoveWE called before driver connection");
        return false;
    }

    speedIndex = IUFindOnSwitchIndex(&SlewRateSP);
    MovementWESP.s = IPS_BUSY;
    if ( command == MOTION_START ) {
        // force tracking after motion
        fTracking = true;
        if ( dir == DIRECTION_WEST )
            answer = sendCommand("ASTRO_SLEW (%.8f+%s) *",trackspeedra/3600.0,speed[speedIndex].c_str());
        else
            answer = sendCommand("ASTRO_SLEW (%.8f-%s) *",trackspeedra/3600.0,speed[speedIndex].c_str());
    } else {
        if ( fTracking )
            answer = sendCommand("ASTRO_TRACK %.8f *",trackspeedra/3600.0);
        else
            answer = sendCommand("ASTRO_SLEW 0 *");
    }
    if ( !answer ) {
        if ( command == MOTION_START )
            MovementWESP.s = IPS_OK;
        else
            MovementWESP.s = IPS_IDLE;
        return true;
    }
    MovementWESP.s = IPS_ALERT;
    DEBUGF(INDI::Logger::DBG_WARNING,"MoveWE command failed due to %s",answer);
    free(answer);
    return false;
}

IPState AUDTELESCOPE::GuideNorth(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideNorth called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE * %u",ms);
    if ( !answer ) {
        rc = IPS_OK;
    } else {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING,"GuideNorth command failed due to %s",answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_DE);
    return rc;
}

IPState AUDTELESCOPE::GuideSouth(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideSouth called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE * -%u",ms);
    if ( !answer ) {
        rc = IPS_OK;
    } else {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING,"GuideSouth command failed due to %s",answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_DE);
    return rc;
}

IPState AUDTELESCOPE::GuideEast(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideEast called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE %u *",ms);
    if ( !answer ) {
        rc = IPS_OK;
    } else {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING,"GuideEast command failed due to %s",answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_RA);
    return rc;
}

IPState AUDTELESCOPE::GuideWest(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideWest called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE -%u *",ms);
    if ( !answer ) {
        rc = IPS_OK;
    } else {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING,"GuideWest command failed due to %s",answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_RA);
    return rc;
}

bool AUDTELESCOPE::Abort()
{
    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Abort called before driver connection");
        return false;
    }

    AbortSP.s = IPS_OK;

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope abort ...");

    if (isConnected()) {
        char *answer;
        answer = sendCommand("ASTRO_STOP");
        free(answer);
    }

    AbortSP.s = IPS_IDLE;
    IUResetSwitch(&AbortSP);
    IDSetSwitch(&AbortSP, NULL);

    slewState = IPS_IDLE;

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope abort completed");

    return true;
}

void AUDTELESCOPE::TimerHit()
{
    if (isConnected() == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    ReadScopeStatus();
    IDSetNumber(&EqNP, NULL);

    SetTimer(getCurrentPollingPeriod());
}

bool AUDTELESCOPE::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ConfigTP);
    IUSaveConfigSwitch(fp, &MeridianFlipSP);
    IUSaveConfigNumber(fp, &MeridianFlipHANP);

    return INDI::Telescope::saveConfigItems(fp);
}

const char *AUDTELESCOPE::getDefaultName()
{
    return device_str;
}

char* AUDTELESCOPE::sendCommand(const char *fmt, ...)
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc,retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 ) {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
                if ( !strncmp(answer,"OK",2) )
                    return NULL;
                if ( !strncmp(answer,"ERROR:",6) )
                    return strdup(answer+6);
                return strdup("SYNTAXERROR");
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:5451", IPaddress);
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

char* AUDTELESCOPE::sendCommandOnce(const char *fmt, ...)
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    zmq_send(requester, buffer, strlen(buffer), 0);
    item = { requester, 0, ZMQ_POLLIN, 0 };
    rc = zmq_poll( &item, 1, 500 ); // ms
    if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
        // communication succeeded
        rc = zmq_recv(requester, answer, sizeof(answer), 0);
        if ( rc >= 0 ) {
            pthread_mutex_unlock( &connectionmutex );
            answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
            if ( !strncmp(answer,"OK",2) )
                return NULL;
            if ( !strncmp(answer,"ERROR:",6) )
                return strdup(answer+6);
            return strdup("SYNTAXERROR");
        }
    }
    zmq_close(requester);
    requester = zmq_socket(context, ZMQ_REQ);
    snprintf( addr, sizeof(addr), "tcp://%s:5451", IPaddress);
    zmq_connect(requester, addr);
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

char* AUDTELESCOPE::sendRequest(const char *fmt, ...)
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc,retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 ) {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
                return strdup(answer);
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:5451", IPaddress);
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}
