/*
 * BresserExosIIGoToDriver.cpp
 *
 * Copyright 2020 Kevin Krüger <kkevin@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include "BresserExosIIGoToDriver.hpp"

#define COMMANDS_PER_SECOND (10)

#define GUIDE_PULSE_TIMEOUT (6)

#define GUIDE_TIMEOUT (20)

//if the mount is not in a specific state after that time its, considered fault.
#define DRIVER_WATCHDOG_TIMEOUT (10000)

using namespace GoToDriver;
using namespace SerialDeviceControl;

static std::unique_ptr<BresserExosIIDriver> mount(new BresserExosIIDriver());

//default constructor.
//sets the scope abilities, and default settings.
BresserExosIIDriver::BresserExosIIDriver() : GI(this),
    mInterfaceWrapper(),
    mMountControl(mInterfaceWrapper)
{
    setVersion(BresserExosIIGoToDriverForIndi_VERSION_MAJOR, BresserExosIIGoToDriverForIndi_VERSION_MINOR);

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION, 0);

    mGuideStateNS.direction = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
    mGuideStateNS.remaining_messages = 0;

    mGuideStateEW.direction = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
    mGuideStateEW.remaining_messages = 0;

    setDefaultPollingPeriod(500);
}

//destructor, not much going on here. Since most of the memory is statically allocated, there is not much to clean up.
BresserExosIIDriver::~BresserExosIIDriver()
{

}

//initialize the properties of the scope.
bool BresserExosIIDriver::initProperties()
{
    INDI::Telescope::initProperties();

    GI::initProperties(MOTION_TAB);

    setTelescopeConnection(CONNECTION_SERIAL);

    addDebugControl();

    IUFillText(&SourceCodeRepositoryURLT[0], "REPOSITORY_URL", "Code Repository", "https://github.com/kneo/indi-bresserexos2");

    IUFillTextVector(&SourceCodeRepositoryURLTP, SourceCodeRepositoryURLT, 1, getDeviceName(), "REPOSITORY_URL", "Source Code",
                     CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    defineProperty(&SourceCodeRepositoryURLTP);

    SetParkDataType(PARK_NONE);

    TrackState = SCOPE_IDLE;

    addAuxControls();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    return true;
}

//update the properties of the scope visible in the EKOS dialogs for instance.
bool BresserExosIIDriver::updateProperties()
{
    bool rc = INDI::Telescope::updateProperties();
    GI::updateProperties();

    return rc;
}

//Connect to the scope, and ready everything for serial data exchange.
bool BresserExosIIDriver::Connect()
{
    bool rc = INDI::Telescope::Connect();

    LOGF_INFO("BresserExosIIDriver::Connect: Initializing ExosII GoTo on FD %d...", PortFD);

    //this message reports back the site location, also starts position reports, without changing anything on the scope.
    mMountControl.RequestSiteLocation();

    mMountControl.ResetCurrentCoordinatesSyncCorrection();
    
    IEAddTimer(DRIVER_WATCHDOG_TIMEOUT, DriverWatchDog, this);

    return rc;
}

//Start the serial receiver thread, so the mount can report its pointing coordinates.
bool BresserExosIIDriver::Handshake()
{
    LOGF_INFO("BresserExosIIDriver::Handshake: Starting Receiver Thread on FD %d...", PortFD);

    mInterfaceWrapper.SetFD(PortFD);

    mMountControl.Start();

    bool rc = INDI::Telescope::Handshake();

    return rc;
}

//Disconnect from the mount, and disable serial transmission.
bool BresserExosIIDriver::Disconnect()
{
    mMountControl.Stop();

    LOG_INFO("BresserExosIIDriver::Disconnect: disabling pointing reporting, disconnected from scope. Bye!");

    bool rc = INDI::Telescope::Disconnect();

    return rc;
}

//Return the name of the device, displayed in the e.g. EKOS dialogs
const char* BresserExosIIDriver::getDefaultName()
{
    return "BRESSER Messier EXOS-2 EQ GoTo";
}

//Periodically polled function to update the state of the driver, and synchronize it with the mount.
bool BresserExosIIDriver::ReadScopeStatus()
{
    SerialDeviceControl::EquatorialCoordinates currentCoordinates = mMountControl.GetPointingCoordinates();
    NewRaDec(currentCoordinates.RightAscension, currentCoordinates.Declination);

    TelescopeMountControl::TelescopeMountState currentState = mMountControl.GetTelescopeState();

    //Translate the mount state to driver state.
    switch(currentState)
    {
        case TelescopeMountControl::TelescopeMountState::Disconnected:
            TrackState = SCOPE_IDLE;
            break;

        case TelescopeMountControl::TelescopeMountState::Unknown:
            TrackState = SCOPE_IDLE;
            break;

        case TelescopeMountControl::TelescopeMountState::ParkingIssued:
            TrackState = SCOPE_PARKING;
            break;

        case TelescopeMountControl::TelescopeMountState::Parked:
            TrackState = SCOPE_PARKED;
            break;

        case TelescopeMountControl::TelescopeMountState::Idle:
            TrackState = SCOPE_IDLE;
            break;

        case TelescopeMountControl::TelescopeMountState::Slewing:
            TrackState = SCOPE_SLEWING;
            break;

        case TelescopeMountControl::TelescopeMountState::Tracking:
            TrackState = SCOPE_TRACKING;
            break;

        case TelescopeMountControl::TelescopeMountState::MoveWhileTracking:
            TrackState = SCOPE_TRACKING;
            break;

        default:

            break;
    }

    return true;
}

bool BresserExosIIDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool BresserExosIIDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

//Park the telescope. This will slew the telescope to the parking position == home position.
bool BresserExosIIDriver::Park()
{
    mMountControl.ParkPosition();
    SetParked(true);

    return true;
}

//Set the state of the driver to unpark allowing the scope to be manipulated again.
bool BresserExosIIDriver::UnPark()
{
    SetParked(false);

    return true;
}

//Sync the astro software and mount coordinates.
bool BresserExosIIDriver::Sync(double ra, double dec)
{
    if(TrackState != SCOPE_TRACKING)
    {
        LOG_INFO("BresserExosIIDriver::Sync: Unable to Synchronize! This function only works when tracking a sky object!");
        return false;
    }

    LOGF_INFO("BresserExosIIDriver::Sync: Synchronizing to Right Ascension: %f Declination :%f...", ra, dec);

    return mMountControl.Sync((float)ra, (float)dec);
}

//Go to the coordinates in the sky, This automatically tracks the selected coordinates.
bool BresserExosIIDriver::Goto(double ra, double dec)
{
    LOGF_INFO("BresserExosIIDriver::Goto: Going to Right Ascension: %f Declination :%f...", ra, dec);

    return mMountControl.GoTo((float)ra, (float)dec);
}

//Abort any motion of the telescope. This is state indipendent, and always possible when connected.
bool BresserExosIIDriver::Abort()
{
    LOG_INFO("BresserExosIIDriver::Abort: motion stopped!");

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideNSTID = 0;
    }

    GuideNSNP.apply();
    GuideWENP.apply();

    return mMountControl.StopMotion();
}

//Set the tracking state of the scope, it either goes to the current coordinates or stops the scope motion.
bool BresserExosIIDriver::SetTrackingEnabled(bool enabled)
{
    if(enabled)
    {
        SerialDeviceControl::EquatorialCoordinates currentCoordinates = mMountControl.GetPointingCoordinates();

        LOGF_INFO("BresserExosIIDriver::SetTrackingEnabled: Tracking to Right Ascension: %f Declination :%f...",
                  currentCoordinates.RightAscension, currentCoordinates.Declination);

        return mMountControl.GoTo(currentCoordinates.RightAscension, currentCoordinates.Declination);
    }
    else
    {
        return mMountControl.StopMotion();
    }
}

//update the time of the scope.
bool BresserExosIIDriver::updateTime(ln_date *utc, double utc_offset)
{
    // Bresser takes local time and DST but ln_zonedate doesn't have DST
    struct ln_zonedate local_date;
    ln_date_to_zonedate(utc, &local_date, static_cast<int>(utc_offset * 3600));

    uint16_t years = (uint16_t)local_date.years;
    uint8_t months = (uint8_t)local_date.months;
    uint8_t days =   (uint8_t)local_date.days;

    uint8_t hours =   (uint8_t) local_date.hours;
    uint8_t minutes = (uint8_t) local_date.minutes;
    uint8_t seconds = (uint8_t) local_date.seconds;
    int8_t utc_off = (int8_t) utc_offset;

    LOGF_INFO("Date/Time updated (UTC Time): %d:%d:%d %d-%d-%d (%d)", utc->hours, utc->minutes, utc->seconds, utc->years,
              utc->months, utc->days, utc_off);
    LOGF_INFO("Date/Time updated (Local Time): %d:%d:%d %d-%d-%d (%d)", hours, minutes, seconds, years, months, days, utc_off);

    return mMountControl.SetDateTime(years, months, days, hours, minutes, seconds, utc_off);
}

//update the location of the scope.
bool BresserExosIIDriver::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    //orientation of the handbox is:
    //negative longitude is west of greenich
    //positive longitude is east of greenich
    //kstars sends 360 complements for negatives
    //this sole case needs to be corrected:
    double realLongitude = longitude;

    if(realLongitude > 180)
    {
        realLongitude -= 360;
    }

    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g)", realLongitude, latitude);

    return mMountControl.SetSiteLocation((float)latitude, (float) realLongitude);
}


bool BresserExosIIDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState != SCOPE_TRACKING)
    {
        LOG_ERROR("this command only works while tracking.");
        return false;
    }

    SerialDeviceControl::SerialCommandID direction;

    switch(dir)
    {
        case DIRECTION_NORTH:
            direction = SerialDeviceControl::SerialCommandID::MOVE_NORTH_COMMAND_ID;
            break;

        case DIRECTION_SOUTH:
            direction = SerialDeviceControl::SerialCommandID::MOVE_SOUTH_COMMAND_ID;
            break;

        default:
            LOG_ERROR("invalid direction value!");
            return false;
    }

    switch(command)
    {
        case MOTION_START:
            mMountControl.StartMotionToDirection(direction, COMMANDS_PER_SECOND);
            return true;

        case MOTION_STOP:
            mMountControl.StopMotionToDirection();
            return true;

        default:

            break;
    }

    return false;
}

bool BresserExosIIDriver::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState != SCOPE_TRACKING)
    {
        LOG_ERROR("this command only works while tracking.");
        return false;
    }

    SerialDeviceControl::SerialCommandID direction;

    switch(dir)
    {
        case DIRECTION_EAST:
            direction = SerialDeviceControl::SerialCommandID::MOVE_EAST_COMMAND_ID;
            break;

        case DIRECTION_WEST:
            direction = SerialDeviceControl::SerialCommandID::MOVE_WEST_COMMAND_ID;
            break;

        default:
            LOG_ERROR("invalid direction value!");
            return false;
    }

    switch(command)
    {
        case MOTION_START:
            mMountControl.StartMotionToDirection(direction, COMMANDS_PER_SECOND);
            return true;

        case MOTION_STOP:
            mMountControl.StopMotionToDirection();
            return true;

        default:

            break;
    }

    return false;
}

//amount of degree change per "pulse command" -> tracking speeds can be set in the HBX,
//it states 1x -> 0.125 * star speed (0.0041°/s ^= 15"/s) and goes up to 8x -> 1.00 * star speed, which would advance by on second, thus guiding speeds are user dependent.
//amount of time necessary to transmit a message -> 12.1 ms
//(9600 baud / 8 -> 1200, but deminished by the stop bit yielding a net data rate of around 1067 byte/s)
//allows around 82 messages send to the mount per second.
//Assume half if serial transmission is not full duplex capable.
//roughtly tranlsates to 42*0.125*0.004 -> 0.0205 degrees per second at minimum setting
//42*0.004 -> 0.164 degrees per second at maximum setting.
//double these amounts if full duplex is possible.
IPState BresserExosIIDriver::GuideNorth(uint32_t ms)
{
    //LOGF_INFO("BresserExosIIDriver::GuideNorth: guiding %d ms", ms);

    if(mMountControl.GetTelescopeState() == TelescopeMountControl::TelescopeMountState::MoveWhileTracking)
    {
        LOG_INFO("BresserExosIIDriver::GuideNorth: motion while tracking stopped!");
        mMountControl.StopMotionToDirection();
    }

    uint32_t messages = ms / GUIDE_TIMEOUT;

    LOGF_INFO("BresserExosIIDriver::GuideNord: guiding %d ms (%d messages)", ms, messages);

    if (GuideNSTID) //reset timer if any.
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if(messages > 0)
    {
        mMountControl.GuideNorth(); // send one pulse
        messages--;

        mGuideStateNS.remaining_messages = messages;
        mGuideStateNS.direction = SerialDeviceControl::SerialCommandID::MOVE_NORTH_COMMAND_ID;

        GuideNSTID = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperN, this); //wait for nex pulse if any.

        return IPS_BUSY;
    }

    return IPS_IDLE;
}

IPState BresserExosIIDriver::GuideSouth(uint32_t ms)
{
    //LOGF_INFO("BresserExosIIDriver::GuideSouth: guiding %d ms", ms);

    if(mMountControl.GetTelescopeState() == TelescopeMountControl::TelescopeMountState::MoveWhileTracking)
    {
        LOG_INFO("BresserExosIIDriver::GuideNorth: motion while tracking stopped!");
        mMountControl.StopMotionToDirection();
    }

    uint32_t messages = ms / GUIDE_TIMEOUT;

    LOGF_INFO("BresserExosIIDriver::GuideSouth: guiding %d ms (%d messages)", ms, messages);

    if (GuideNSTID) //reset timer if any.
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if(messages > 0)
    {
        mMountControl.GuideSouth(); // send one pulse
        messages--;

        mGuideStateNS.remaining_messages = messages;
        mGuideStateNS.direction = SerialDeviceControl::SerialCommandID::MOVE_SOUTH_COMMAND_ID;

        GuideNSTID = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperS, this); //wait for nex pulse if any.

        return IPS_BUSY;
    }

    return IPS_IDLE;
}

IPState BresserExosIIDriver::GuideEast(uint32_t ms)
{
    //LOGF_INFO("BresserExosIIDriver::GuideEast: guiding %d ms", ms);

    if(mMountControl.GetTelescopeState() == TelescopeMountControl::TelescopeMountState::MoveWhileTracking)
    {
        LOG_INFO("BresserExosIIDriver::GuideNorth: motion while tracking stopped!");
        mMountControl.StopMotionToDirection();
    }

    uint32_t messages = ms / GUIDE_TIMEOUT;

    LOGF_INFO("BresserExosIIDriver::GuideEast: guiding %d ms (%d messages)", ms, messages);

    if (GuideWETID) //reset timer if any.
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if(messages > 0)
    {
        mMountControl.GuideEast(); // send one pulse

        messages--;

        mGuideStateEW.direction = SerialDeviceControl::SerialCommandID::MOVE_EAST_COMMAND_ID;
        mGuideStateEW.remaining_messages = messages;

        GuideWETID = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperE, this); //wait for nex pulse if any.

        return IPS_BUSY;
    }

    return IPS_IDLE;
}

IPState BresserExosIIDriver::GuideWest(uint32_t ms)
{
    //LOGF_INFO("BresserExosIIDriver::GuideWest: guiding %d ms", ms);

    if(mMountControl.GetTelescopeState() == TelescopeMountControl::TelescopeMountState::MoveWhileTracking)
    {
        LOG_INFO("BresserExosIIDriver::GuideNorth: motion while tracking stopped!");
        mMountControl.StopMotionToDirection();
    }

    uint32_t messages = ms / GUIDE_TIMEOUT;

    LOGF_INFO("BresserExosIIDriver::GuideWest: guiding %d ms (%d messages)", ms, messages);

    if (GuideWETID) //reset timer if any.
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if(messages > 0)
    {
        mMountControl.GuideWest();

        messages--;
        mGuideStateEW.remaining_messages = messages;
        mGuideStateEW.direction = SerialDeviceControl::SerialCommandID::MOVE_WEST_COMMAND_ID;

        GuideWETID = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperW, this); //wait for nex pulse if any.

        return IPS_BUSY;
    }

    return IPS_IDLE;
}

void BresserExosIIDriver::DriverWatchDog(void *p)
{
    BresserExosIIDriver* driverInstance = static_cast<BresserExosIIDriver*>(p);

    if(driverInstance == nullptr)
    {
        return;
    }

    TelescopeMountControl::TelescopeMountState currentState = driverInstance->mMountControl.GetTelescopeState();

    if(currentState == TelescopeMountControl::TelescopeMountState::Unknown)
    {
        driverInstance->LogError("Watchdog Timeout without communication!");
        driverInstance->LogError("Please make sure your serial device is correct, and communication is possible.");
        return;
    }
    driverInstance->LogInfo("INFO: Communication seems to be established!");
}

void BresserExosIIDriver::guideTimeout(SerialDeviceControl::SerialCommandID direction)
{
    bool continuePulsing = false;
    switch(direction)
    {
        case SerialDeviceControl::SerialCommandID::MOVE_NORTH_COMMAND_ID:
            continuePulsing = mGuideStateNS.remaining_messages > 0;

            if(continuePulsing)
            {
                mGuideStateNS.remaining_messages--;
                mMountControl.GuideNorth();
                GuideNSNP.setState(IPS_BUSY);
                GuideNSTID  = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperN, this);
            }
            else
            {
                GuideNSNP.setState(IPS_IDLE);
                GuideNSTID = 0;
                mGuideStateNS.remaining_messages = 0;
                mGuideStateNS.direction = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
                GuideNSNP.apply();
            }
            break;

        case SerialDeviceControl::SerialCommandID::MOVE_SOUTH_COMMAND_ID:
            continuePulsing = mGuideStateNS.remaining_messages > 0;

            if(continuePulsing)
            {
                mGuideStateNS.remaining_messages--;
                mMountControl.GuideSouth();
                GuideNSNP.setState(IPS_BUSY);
                GuideNSTID  = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperS, this);
            }
            else
            {
                GuideNSNP.setState(IPS_IDLE);
                GuideNSTID = 0;
                mGuideStateNS.remaining_messages = 0;
                mGuideStateNS.direction = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
                GuideNSNP.apply();
            }
            break;

        case SerialDeviceControl::SerialCommandID::MOVE_WEST_COMMAND_ID:
            continuePulsing = mGuideStateEW.remaining_messages > 0;

            if(continuePulsing)
            {
                mGuideStateEW.remaining_messages--;
                mMountControl.GuideWest();
                GuideWENP.setState(IPS_BUSY);
                GuideNSTID  = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperW, this);
            }
            else
            {
                GuideWENP.setState(IPS_IDLE);
                GuideWETID = 0;
                mGuideStateEW.remaining_messages = 0;
                mGuideStateEW.direction = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
                GuideWENP.apply();
            }
            break;

        case SerialDeviceControl::SerialCommandID::MOVE_EAST_COMMAND_ID:
            continuePulsing = mGuideStateEW.remaining_messages > 0;

            if(continuePulsing)
            {
                mGuideStateEW.remaining_messages--;
                mMountControl.GuideEast();
                GuideWENP.setState(IPS_BUSY);
                GuideNSTID  = IEAddTimer(GUIDE_TIMEOUT, guideTimeoutHelperE, this);
            }
            else
            {
                GuideWENP.setState(IPS_IDLE);
                GuideWETID = 0;
                mGuideStateEW.remaining_messages = 0;
                mGuideStateEW.direction = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
                GuideWENP.apply();
            }
            break;

        default:
            GuideWENP.setState(IPS_IDLE);
            GuideWETID = 0;
            GuideWENP.apply();

            GuideNSNP.setState(IPS_IDLE);
            GuideNSTID = 0;
            GuideNSNP.apply();
            break;
    }
}

//GUIDE The timer helper functions.
void BresserExosIIDriver::guideTimeoutHelperN(void *p)
{
    static_cast<BresserExosIIDriver*>(p)->guideTimeout(SerialDeviceControl::SerialCommandID::MOVE_NORTH_COMMAND_ID);
}

void BresserExosIIDriver::guideTimeoutHelperS(void *p)
{
    static_cast<BresserExosIIDriver*>(p)->guideTimeout(SerialDeviceControl::SerialCommandID::MOVE_SOUTH_COMMAND_ID);
}

void BresserExosIIDriver::guideTimeoutHelperW(void *p)
{
    static_cast<BresserExosIIDriver*>(p)->guideTimeout(SerialDeviceControl::SerialCommandID::MOVE_WEST_COMMAND_ID);
}

void BresserExosIIDriver::guideTimeoutHelperE(void *p)
{
    static_cast<BresserExosIIDriver*>(p)->guideTimeout(SerialDeviceControl::SerialCommandID::MOVE_EAST_COMMAND_ID);
}

void BresserExosIIDriver::LogError(const char* message)
{
    LOG_ERROR(message);
}

void BresserExosIIDriver::LogInfo(const char* message)
{
    LOG_INFO(message);
}
