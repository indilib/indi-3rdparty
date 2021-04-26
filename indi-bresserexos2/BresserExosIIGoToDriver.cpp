#include "BresserExosIIGoToDriver.hpp"

#define COMMANDS_PER_SECOND (10)
//if the mount is not in a specific state after that time its, considered fault.
#define DRIVER_WATCHDOG_TIMEOUT (10000)

using namespace GoToDriver;
using namespace SerialDeviceControl;

static std::unique_ptr<BresserExosIIDriver> driver_instance(new BresserExosIIDriver());

#if 0 // No needed, see https://github.com/indilib/indi/pull/1375
void ISGetProperties(const char* dev)
{
    driver_instance->ISGetProperties(dev);
}

void ISNewSwitch(const char* dev, const char* name, ISState * states, char* names[], int n)
{
    driver_instance->ISNewSwitch(dev, name, states, names, n);
}

//TODO: this seems to have changed in indi 1.8.8
//void ISNewText(const char* dev, const char* name, ISState * states, char* names[], int n)
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    driver_instance->ISNewText(dev, name, texts, names, n);
}

//TODO: this seems to have changed in indi 1.8.8
//void ISNewNumber(const char* dev, const char* name, ISState * states, char* names[], int n)
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    driver_instance->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char* dev, const char* name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    //driver_instance->ISNewBLOB(dev,name,sizes,blobs,formats,names,n);
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle* root)
{
    driver_instance->ISSnoopDevice(root);
}
#endif

//default constructor.
//sets the scope abilities, and default settings.
BresserExosIIDriver::BresserExosIIDriver() :
    mInterfaceWrapper(),
    mMountControl(mInterfaceWrapper)
{
    setVersion(BresserExosIIGoToDriverForIndi_VERSION_MAJOR, BresserExosIIGoToDriverForIndi_VERSION_MINOR);

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION, 0);

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

    setTelescopeConnection(CONNECTION_SERIAL);

    addDebugControl();
    
	IUFillText(&SourceCodeRepositoryURLT[0], "REPOSITORY_URL", "Code Repository", "https://github.com/kneo/indi-bresserexos2");
	
    IUFillTextVector(&SourceCodeRepositoryURLTP, SourceCodeRepositoryURLT, 1, getDeviceName(), "REPOSITORY_URL", "Source Code", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);
	
	defineProperty(&SourceCodeRepositoryURLTP);
    
    SetParkDataType(PARK_NONE);

    TrackState = SCOPE_IDLE;

    addAuxControls();

    setDriverInterface(getDriverInterface());

    return true;
}

//update the properties of the scope visible in the EKOS dialogs for instance.
bool BresserExosIIDriver::updateProperties()
{
    bool rc = INDI::Telescope::updateProperties();

    return rc;
}

//Connect to the scope, and ready everything for serial data exchange.
bool BresserExosIIDriver::Connect()
{
    bool rc = INDI::Telescope::Connect();

    LOGF_INFO("BresserExosIIDriver::Connect: Initializing ExosII GoTo on FD %d...", PortFD);

    //this message reports back the site location, also starts position reports, without changing anything on the scope.
    mMountControl.RequestSiteLocation();

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
        LOG_INFO("BresserExosIIDriver::Sync: Unable to Syncronize! This function only works when tracking a sky object!");
        return false;
    }

    LOGF_INFO("BresserExosIIDriver::Sync: Syncronizing to Right Ascension: %f Declination :%f...", ra, dec);

    return mMountControl.Sync((float)ra, (float)dec);;
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
    INDI::Telescope::updateTime(utc, utc_offset);

    uint16_t years = (uint16_t)utc->years;
    uint8_t months = (uint8_t)utc->months;
    uint8_t days =   (uint8_t)utc->days;

    uint8_t hours =   (uint8_t) utc->hours;
    uint8_t minutes = (uint8_t) utc->minutes;
    uint8_t seconds = (uint8_t) utc->seconds;

    LOGF_INFO("Date/Time updated: %d:%d:%d %d-%d-%d", hours, minutes, seconds, years, months, days);

    return mMountControl.SetDateTime(years, months, days, hours, minutes, seconds);;
}

//update the location of the scope.
bool BresserExosIIDriver::updateLocation(double latitude, double longitude, double elevation)
{

    INDI_UNUSED(elevation);

    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g)", longitude, latitude);

    return mMountControl.SetSiteLocation((float)latitude, (float) longitude);;
}


bool BresserExosIIDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState != SCOPE_TRACKING)
    {
        LOG_ERROR("Error: this command only works while tracking.");
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
            LOG_ERROR("Error: invalid direction value!");
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
        LOG_ERROR("Error: this command only works while tracking.");
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
            LOG_ERROR("Error: invalid direction value!");
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
        driverInstance->LogError("Error: Watchdog Timeout without communication!");
        driverInstance->LogError("Please make sure your serial device is correct, and communication is possible.");
        return;
    }
    driverInstance->LogInfo("INFO: Communication seems to be established!");
}

void BresserExosIIDriver::LogError(const char* message)
{
    LOG_ERROR(message);
}

void BresserExosIIDriver::LogInfo(const char* message)
{
    LOG_INFO(message);
}
