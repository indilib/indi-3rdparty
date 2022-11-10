/*
   INDI Developers Manual
   Tutorial #2

   "Simple Telescope Driver"

   We develop a simple telescope simulator.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file indi_ahp_gt.cpp
    \brief Construct a basic INDI telescope device that simulates GOTO commands.
    \author Jasem Mutlaq

    \example simplescope.cpp
    A simple GOTO telescope that simulator slewing operation.
*/

#include "indi_ahp_gt.h"
#include <ahp_gt.h>

#include "indicom.h"

#include <cmath>
#include <memory>

static std::unique_ptr<AHPGT> ahp_gt(new AHPGT());

AHPGT::AHPGT()
{
    // We add an additional debug level so we can log verbose scope status
    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");
}

/**************************************************************************************
** We init our properties here. The only thing we want to init are the Debug controls
***************************************************************************************/
bool AHPGT::initProperties()
{
    // ALWAYS call initProperties() of parent first
    INDI::Telescope::initProperties();

    // Set telescope capabilities. 0 is for the the number of slew rates that we support. We have none for this simple driver.
    SetTelescopeCapability(TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE | TELESCOPE_CAN_TRACK_SATELLITE, 0);

    // Add Debug control so end user can turn debugging/loggin on and off
    addDebugControl();

    // Enable simulation mode so that serial connection in INDI::Telescope does not try
    // to attempt to perform a physical connection to the serial port.
    setSimulation(false);

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    return true;
}

/**************************************************************************************
** INDI is asking us to check communication with the device via a handshake
***************************************************************************************/
bool AHPGT::Handshake()
{
    int fd = -1;
    if (!getActiveConnection()->name().compare("CONNECTION_TCP")) {
        if(tcpConnection->connectionType() == Connection::TCP::TYPE_UDP)
            tty_set_generic_udp_format(1);
        fd = tcpConnection->getPortFD();
    } else {
        fd = serialConnection->getPortFD();
    }
    if(!ahp_gt_connect_fd(fd)) {
        ahp_gt_read_values(0);
        ahp_gt_read_values(1);
        return true;
    }
    ahp_gt_disconnect();
    return false;
}


bool AHPGT::Disconnect() {
    Abort();
    ahp_gt_disconnect();
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *AHPGT::getDefaultName()
{
    return "GT Telescope";
}

/**************************************************************************************
** Client is asking us to slew to a new position
***************************************************************************************/
bool AHPGT::Goto(double ra, double dec)
{
    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};

    // Parse the RA/DEC into strings
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // Inform client we are slewing to a new position
    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    ahp_gt_goto_radec(targetRA, targetDEC);

    INDI::Telescope::Goto(ra, dec);
    // Success!
    return true;
}

bool AHPGT::Sync(double ra, double dec)
{
    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};

    // Parse the RA/DEC into strings
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // Inform client we are syncing to a new position
    LOGF_INFO("Syncing to RA: %s - DEC: %s", RAStr, DecStr);

    // Success!
    return true;
}

bool AHPGT::updateLocation(double latitude, double longitude, double elevation)
{
    char LatStr[64] = {0}, LonStr[64] = {0};

    // Parse the RA/DEC into strings
    fs_sexa(LatStr, latitude, 2, 3600);
    fs_sexa(LonStr, longitude, 2, 3600);

    // Inform client we are syncing to a new position
    LOGF_INFO("Set location to Latitude: %s - Longitude: %s - elevation: %lf", LatStr, LonStr, elevation);

    ahp_gt_set_location(latitude, longitude, elevation);

    // Success!
    return true;
}

bool AHPGT::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    double maxRate = fmin(ahp_gt_get_max_speed(0), ahp_gt_get_max_speed(1));
    if(command == MOTION_START) {
        switch (dir)
        {
        case DIRECTION_NORTH:
            ahp_gt_start_motion(0, maxRate / SlewRate);
            break;
        case DIRECTION_SOUTH:
            ahp_gt_start_motion(0, -maxRate / SlewRate);
            break;
        default: break;
        }
    } else {
        ahp_gt_stop_motion(0, 0);
    }

    // Success!
    return true;
}

bool AHPGT::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    double maxRate = fmin(ahp_gt_get_max_speed(0), ahp_gt_get_max_speed(1));
    if(command == MOTION_START) {
        switch (dir)
        {
        case DIRECTION_WEST:
            ahp_gt_start_motion(1, maxRate / SlewRate);
            break;
        case DIRECTION_EAST:
            ahp_gt_start_motion(1, -maxRate / SlewRate);
            break;
        default: break;
        }
    } else {
        ahp_gt_stop_motion(1, 0);
    }

    // Success!
    return true;
}

bool AHPGT::SetTrackRate(double raRate, double deRate)
{
    TrackRate[0] = raRate * STELLAR_DAY / 360.0;
    TrackRate[1] = deRate * STELLAR_DAY / 360.0;
    return true;
}

bool AHPGT::SetTrackMode(uint8_t mode)
{
    TrackMode = mode;
    return true;
}

bool AHPGT::SetSlewRate(int rate)
{
    SlewRate = rate;
    return true;
}

bool AHPGT::SetTrackEnabled(bool enabled)
{
    isTracking = enabled;
    return true;
}

/**************************************************************************************
** Client is asking us to abort our motion
***************************************************************************************/
bool AHPGT::Abort()
{
    ahp_gt_stop_motion(0, 0);
    ahp_gt_stop_motion(1, 0);
    isTracking = false;
    return true;
}

/**************************************************************************************
** Client is asking us to report telescope status
***************************************************************************************/
bool AHPGT::ReadScopeStatus()
{
    struct timeval tv
    {
        0, 0
    };

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    ahp_gt_set_time((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);

    SkywatcherAxisStatus ra_status = ahp_gt_get_status(0);
    SkywatcherAxisStatus dec_status = ahp_gt_get_status(1);
    TrackState = (ra_status.Mode == MODE_SLEW ? (isTracking ? SCOPE_TRACKING : SCOPE_SLEWING) : SCOPE_SLEWING);

    if(!ra_status.Running) {
        if(isTracking) {
            double trackrate = 1.0;
            switch(TrackMode) {
            case 1:
                trackrate = TRACKRATE_SOLAR * STELLAR_DAY / (360.0 * 60.0 * 60.0);
                break;
            case 2:
                trackrate = TRACKRATE_LUNAR * STELLAR_DAY / (360.0 * 60.0 * 60.0);
                break;
            case 3:
                trackrate = TrackRate[0];
            default:
                break;
            }
            if(trackrate != 0.0) {
                ahp_gt_start_motion(0, trackrate);
            }
        }
    }
    if(!dec_status.Running) {
        if(isTracking) {
            double trackrate = 0.0;
            switch(TrackMode) {
            case 3:
                trackrate = TrackRate[1];
            default:
                break;
            }
            if(trackrate != 0.0) {
                ahp_gt_start_motion(1, trackrate);
            }
        }
    }
    currentRA = ahp_gt_get_ra();
    currentDEC = ahp_gt_get_dec();

    char RAStr[64] = {0}, DecStr[64] = {0};

    // Parse the RA/DEC into strings
    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);
    return true;
}
