/*
    Celestron Aux Mount Driver.

    Copyright (C) 2020 Paweł T. Jochym
    Copyright (C) 2020 Fabrizio Pollastri
    Copyright (C) 2021 Jasem Mutlaq

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

#include <algorithm>
#include <math.h>
#include <queue>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <thread>
#include <chrono>

#include "celestronaux.h"
#include "config.h"

using namespace INDI::AlignmentSubsystem;

std::unique_ptr<CelestronAUX> telescope_caux(new CelestronAUX());

double anglediff(double a, double b)
{
    // Signed angle difference
    double d;
    a = fmod(a, 360.0);
    b = fmod(b, 360.0);
    d = fmod(a - b + 360.0, 360.0);
    if (d > 180)
        d = 360.0 - d;
    return std::abs(d) * ((a - b >= 0 && a - b <= 180) || (a - b <= -180 && a - b >= -360) ? 1 : -1);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
CelestronAUX::CelestronAUX()
    : ScopeStatus(IDLE), AxisStatusALT(STOPPED), AxisDirectionALT(FORWARD), AxisStatusAZ(STOPPED),
      AxisDirectionAZ(FORWARD),
      DBG_CAUX(INDI::Logger::getInstance().addDebugLevel("AUX", "CAUX")),
      DBG_SERIAL(INDI::Logger::getInstance().addDebugLevel("Serial", "CSER"))
{
    setVersion(CAUX_VERSION_MAJOR, CAUX_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION | TELESCOPE_CAN_CONTROL_TRACK, 8);

    //Both communication available, Serial and network (tcp/ip).
    setTelescopeConnection(CONNECTION_TCP | CONNECTION_SERIAL);

    // Approach from no further then degs away
    Approach = 1.0;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
CelestronAUX::~CelestronAUX()
{
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Abort()
{
    AxisStatusAZ = AxisStatusALT = STOPPED;
    TrackState = SCOPE_IDLE;
    Track(0, 0);
    AUXBuffer b(1);
    b[0] = 0;
    AUXCommand stopAlt(MC_MOVE_POS, APP, ALT, b);
    AUXCommand stopAz(MC_MOVE_POS, APP, AZM, b);
    sendAUXCommand(stopAlt);
    readAUXResponse(stopAlt);
    sendAUXCommand(stopAz);
    readAUXResponse(stopAz);

    LOG_INFO("Mount motion aborted.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Handshake()
{
    LOGF_DEBUG("CAUX: connect %d (%s)", PortFD, (getActiveConnection() == serialConnection) ? "serial" : "net");
    if (PortFD > 0)
    {
        if (getActiveConnection() == serialConnection)
        {
            LOGF_DEBUG("detectRTSCTS = %s.",  detectRTSCTS() ? "true" : "false");

            // if serial connection, check if hardware control flow is required.
            // yes for AUX and PC ports, no for HC port and mount USB port.
            if ((m_IsRTSCTS = detectRTSCTS()))
            {
                LOG_INFO("Detected AUX or PC port connection.");
                serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
                if (!tty_set_speed(PortFD, B19200))
                    return false;
                LOG_INFO("Setting serial speed to 19200 baud.");
            }
            else
            {
                serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
                if (!tty_set_speed(PortFD, B9600))
                {
                    LOG_ERROR("Cannot set serial speed to 9600 baud.");
                    return false;
                }

                // wait for speed to settle
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                LOG_INFO("Setting serial speed to 9600 baud.");

                // detect if connectd to HC port or to mount USB port
                // ask for HC version
                char version[10];
                if ((m_isHandController = detectHC(version, (size_t)10)))
                    LOGF_INFO("Detected Hand Controller (v%s) serial connection.", version);
                else
                    LOG_INFO("Detected Mount USB serial connection.");
            }
        }
        else
        {
            LOG_INFO("Waiting for mount connection to settle...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            return true;
        }

        // read firmware version, if read ok, detected scope
        LOG_DEBUG("Communicating with mount motor controllers...");
        if (getVersion(AZM) && getVersion(ALT))
        {
            LOG_INFO("Got response from target ALT or AZM.");
        }
        else
        {
            LOG_ERROR("Got no response from target ALT or AZM.");
            LOG_ERROR("Cannot continue without connection to motor controllers.");
            return false;
        }

        LOG_DEBUG("Connection ready. Starting Processing.");

        // set mount type to alignment subsystem
        SetApproximateMountAlignmentFromMountType(static_cast<MountType>(IUFindOnSwitchIndex(&MountTypeSP)));
        // tell the alignment math plugin to reinitialise
        Initialise(this);

        // update cordwrap position at each init of the alignment subsystem
        if (isConnected())
            syncCoordWrapPosition();
        return true;
    }
    else
    {
        return false ;
    }

}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Disconnect()
{
    Abort();
    return INDI::Telescope::Disconnect();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
const char *CelestronAUX::getDefaultName()
{
    return "Celestron AUX";
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::initProperties()
{
    INDI::Telescope::initProperties();
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    // Set Debug Information for AUX Commands
    AUXCommand::setDebugInfo(getDeviceName(), DBG_CAUX);

    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    TrackState = SCOPE_IDLE;

    // Add debug controls so we may debug driver if necessary
    addDebugControl();

    // Add alignment properties
    InitAlignmentProperties(this);

    // set alignment system be on the first time by default
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;

    // Default connection options
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    tcpConnection->setDefaultHost(CAUX_DEFAULT_IP);
    tcpConnection->setDefaultPort(CAUX_DEFAULT_PORT);

    // Firmware Info
    FirmwareTP[FW_HC].fill("HC version", "", nullptr);
    FirmwareTP[FW_MB].fill("Mother Board version", "", nullptr);
    FirmwareTP[FW_AZM].fill("Ra/AZM version", "", nullptr);
    FirmwareTP[FW_ALT].fill("Dec/ALT version", "", nullptr);
    FirmwareTP[FW_WiFi].fill("WiFi version", "", nullptr);
    FirmwareTP[FW_BAT].fill("Battery version", "", nullptr);
    FirmwareTP[FW_GPS].fill("GPS version", "", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware Info", "Firmware Info", MOUNTINFO_TAB, IP_RO, 0, IPS_IDLE);

    // Mount type
    MountTypeSP[EQUATORIAL].fill("EQUATORIAL", "Equatorial", ISS_OFF);
    MountTypeSP[ALTAZ].fill("ALTAZ", "AltAz", ISS_ON);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MOUNTINFO_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Cord wrap Toggle
    CordWrapToggleSP[CORDWRAP_OFF].fill("CORDWRAP_OFF", "OFF", ISS_OFF);
    CordWrapToggleSP[CORDWRAP_ON].fill("CORDWRAP_ON", "ON", ISS_ON);
    CordWrapToggleSP.fill(getDeviceName(), "CORDWRAP", "Cord Wrap", CORDWRAP_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Cord Wrap Position
    CordWrapPositionSP[CORDWRAP_N].fill("CORDWRAP_N", "North", ISS_ON);
    CordWrapPositionSP[CORDWRAP_E].fill("CORDWRAP_E", "East",  ISS_OFF);
    CordWrapPositionSP[CORDWRAP_S].fill("CORDWRAP_S", "South", ISS_OFF);
    CordWrapPositionSP[CORDWRAP_W].fill("CORDWRAP_W", "West",  ISS_OFF);
    CordWrapPositionSP.fill(getDeviceName(), "CORDWRAP_POS", "CW Position", CORDWRAP_TAB, IP_RW, ISR_1OFMANY, 60,  IPS_IDLE);

    // Cord Wrap / Park Base
    CordWrapBaseSP[CW_BASE_ENC].fill("CW_BASE_ENC", "Encoders", ISS_ON);
    CordWrapBaseSP[CW_BASE_SKY].fill("CW_BASE_SKY", "Alignment positions", ISS_OFF);
    CordWrapBaseSP.fill(getDeviceName(), "CW_BASE", "CW Position Base", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // GPS Emulation
    GPSEmuSP[GPSEMU_OFF].fill("GPSEMU_OFF", "OFF", ISS_OFF);
    GPSEmuSP[GPSEMU_ON].fill("GPSEMU_ON", "ON", ISS_ON);
    GPSEmuSP.fill(getDeviceName(), "GPSEMU", "GPS Emu", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Guide Properties
    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    /* How fast do we guide compared to sidereal rate */
    GuideRateNP[AXIS_RA].fill("GUIDE_RATE_WE", "W/E Rate", "%.f", 10, 100, 1, 50);
    GuideRateNP[AXIS_DE].fill("GUIDE_RATE_NS", "N/S Rate", "%.f", 10, 100, 1, 50);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    // to update cordwrap pos at each init of alignment subsystem
    IDSnoopDevice(getDeviceName(), "ALIGNMENT_SUBSYSTEM_MATH_PLUGIN_INITIALISE");

    // JM 2020-09-23 Make it easier for users to connect by default via WiFi if they
    // selected the Celestron WiFi labeled driver.
    if (strstr(getDeviceName(), "WiFi"))
    {
        setActiveConnection(tcpConnection);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::formatVersionString(char *s, int n, uint8_t *verBuf)
{
    if (verBuf[0] == 0 && verBuf[1] == 0 && verBuf[2] == 0 && verBuf[3] == 0)
        snprintf(s, n, "Unknown");
    else
        snprintf(s, n, "%d.%02d.%d", verBuf[0], verBuf[1], verBuf[2] * 256 + verBuf[3]);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(&MountTypeSP);

        // Guide
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);

        // Cord wrap Enabled?
        getCordWrapEnabled();
        CordWrapToggleSP[CORDWRAP_OFF].s = m_CordWrapActive ? ISS_OFF : ISS_ON;
        CordWrapToggleSP[CORDWRAP_ON].s  = m_CordWrapActive ? ISS_ON : ISS_OFF;
        defineProperty(&CordWrapToggleSP);

        // Cord wrap Position?
        getCordWrapPosition();
        double cordWrapAngle = range360(m_CordWrapPosition / STEPS_PER_DEGREE);
        LOGF_INFO("Cord Wrap position angle %.2f", cordWrapAngle);
        CordWrapPositionSP[static_cast<int>(std::floor(cordWrapAngle / 90))].s = ISS_ON;
        defineProperty(&CordWrapPositionSP);

        defineProperty(&CordWrapBaseSP);
        defineProperty(&GPSEmuSP);
        getVersions();

        // display firmware versions
        char fwText[16] = {0};
        formatVersionString(fwText, 10, m_HCVersion);
        FirmwareTP[FW_HC].setText(fwText);
        formatVersionString(fwText, 10, m_MainBoardVersion);
        FirmwareTP[FW_MB].setText(fwText);
        formatVersionString(fwText, 10, m_AzimuthVersion);
        FirmwareTP[FW_AZM].setText(fwText);
        formatVersionString(fwText, 10, m_AltitudeVersion);
        FirmwareTP[FW_ALT].setText(fwText);
        formatVersionString(fwText, 10, m_WiFiVersion);
        FirmwareTP[FW_WiFi].setText(fwText);
        formatVersionString(fwText, 10, m_BATVersion);
        FirmwareTP[FW_BAT].setText(fwText);
        formatVersionString(fwText, 10, m_GPSVersion);
        FirmwareTP[FW_GPS].setText(fwText);
        defineProperty(&FirmwareTP);
    }
    else
    {
        deleteProperty(MountTypeSP.getName());
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.getName());

        deleteProperty(CordWrapToggleSP.getName());
        deleteProperty(CordWrapPositionSP.getName());
        deleteProperty(CordWrapBaseSP.getName());

        deleteProperty(GPSEmuSP.getName());
        deleteProperty(FirmwareTP.getName());
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    SaveAlignmentConfigProperties(fp);

    IUSaveConfigSwitch(fp, &MountTypeSP);
    IUSaveConfigSwitch(fp, &CordWrapToggleSP);
    IUSaveConfigSwitch(fp, &CordWrapPositionSP);
    IUSaveConfigSwitch(fp, &CordWrapBaseSP);
    IUSaveConfigSwitch(fp, &GPSEmuSP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::ISSnoopDevice(XMLEle *root)
{
    const char *propName = findXMLAttValu(root, "name");

    // update cordwrap position at each init of the alignment subsystem
    if (!strcmp(propName, "ALIGNMENT_SUBSYSTEM_MATH_PLUGIN_INITIALISE") && telescope_caux->isConnected())
        telescope_caux->syncCoordWrapPosition();

    return Telescope::ISSnoopDevice(root);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                             char *formats[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Process alignment properties
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, n);
            GuideRateNP.setState(IPS_OK);
            GuideRateNP.apply();
            return true;
        }

        // Process Guide Properties
        processGuiderProperties(name, values, names, n);

        // Process Alignment Properties
        ProcessAlignmentNumberProperties(this, name, values, names, n);
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // mount type
        if (MountTypeSP.isNameMatch(name))
        {
            // Get current type
            MountType currentMountType = IUFindOnSwitchIndex(&MountTypeSP) ? ALTAZ : EQUATORIAL;

            MountTypeSP.update(states, names, n);
            MountTypeSP.setState(IPS_OK);
            MountTypeSP.apply();
            IDSetSwitch(&MountTypeSP, nullptr);

            // Get target type
            MountType targetMountType = IUFindOnSwitchIndex(&MountTypeSP) ? ALTAZ : EQUATORIAL;

            // If different then update
            if (currentMountType != targetMountType)
            {
                // set mount type to alignment subsystem
                SetApproximateMountAlignmentFromMountType(currentMountType);
                // tell the alignment math plugin to reinitialise
                Initialise(this);

                // update cordwrap position at each init of the alignment subsystem
                if ( isConnected() )
                    syncCoordWrapPosition();
            }

            return true;
        }

        // Cord Wrap Toggle
        if (CordWrapToggleSP.isNameMatch(name))
        {
            CordWrapToggleSP.update(states, names, n);
            const bool toEnable = CordWrapToggleSP[CORDWRAP_ON].s == ISS_ON;
            LOGF_INFO("Cord Wrap is %s.", toEnable ? "enabled" : "disabled");
            CordWrapToggleSP.setState(IPS_OK);
            setCordWrapEnabled(toEnable);
            getCordWrapEnabled();
            return true;
        }

        // Cord Wrap Position
        if (CordWrapPositionSP.isNameMatch(name))
        {
            CordWrapPositionSP.update(states, names, n);
            LOGF_DEBUG("Cord Wrap Position is %s.", CordWrapPositionSP.findOnSwitch()->getLabel());
            CordWrapPositionSP.setState(IPS_OK);
            CordWrapPositionSP.apply();
            switch (CordWrapPositionSP.findOnSwitchIndex())
            {
                case CORDWRAP_N:
                    m_RequestedCordwrapPos = 0;
                    break;
                case CORDWRAP_E:
                    m_RequestedCordwrapPos = 90;
                    break;
                case CORDWRAP_S:
                    m_RequestedCordwrapPos = 180;
                    break;
                case CORDWRAP_W:
                    m_RequestedCordwrapPos = 270;
                    break;
                default:
                    m_RequestedCordwrapPos = 0;
                    break;
            }

            syncCoordWrapPosition();
            return true;
        }

        // Park position base
        if (CordWrapBaseSP.isNameMatch(name))
        {
            CordWrapBaseSP.update(states, names, n);
            CordWrapBaseSP.setState(IPS_OK);
            CordWrapBaseSP.apply();
            return true;
        }

        // GPS Emulation
        if (GPSEmuSP.isNameMatch(name))
        {
            GPSEmuSP.update(states, names, n);
            GPSEmuSP.setState(IPS_OK);
            GPSEmuSP.apply();
            m_GPSEmulation = GPSEmuSP[GPSEMU_ON].s == ISS_ON;
            return true;
        }

        // Process alignment properties
        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Process alignment properties
        ProcessAlignmentTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Park()
{
    // Park at the northern or southern horizon
    // This is a designated by celestron parking position
    Abort();
    LOG_INFO("Mount park in progress...");
    GoToFast(0, LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180, false);
    TrackState = SCOPE_PARKING;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::UnPark()
{
    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
INDI::IHorizontalCoordinates CelestronAUX::AltAzFromRaDec(double ra, double dec, double ts)
{
    // Call the alignment subsystem to translate the celestial reference frame coordinate
    // into a telescope reference frame coordinate
    TelescopeDirectionVector TDV;
    INDI::IHorizontalCoordinates AltAz;

    if (TransformCelestialToTelescope(ra, dec, ts, TDV))
        // The alignment subsystem has successfully transformed my coordinate
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    else
    {
        LOG_DEBUG("AltAzFromRaDec - TransformCelestialToTelescope failed");

        INDI::IEquatorialCoordinates EquatorialCoordinates { ra, dec };
        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, ln_get_julian_from_sys(), &AltAz);
        TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
        switch (GetApproximateMountAlignment())
        {
            case ZENITH:
                break;

            case NORTH_CELESTIAL_POLE:
                // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 minus
                // the (positive)observatory latitude. The vector itself is rotated anticlockwise
                TDV.RotateAroundY(m_Location.latitude - 90.0);
                break;

            case SOUTH_CELESTIAL_POLE:
                // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 plus
                // the (negative)observatory latitude. The vector itself is rotated clockwise
                TDV.RotateAroundY(m_Location.latitude + 90.0);
                break;
        }
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    }

    return AltAz;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::getNorthAz()
{
    INDI::IGeographicCoordinates location;
    double northAz;
    if (!GetDatabaseReferencePosition(location))
        northAz = 0.;
    else
        northAz = AltAzFromRaDec(get_local_sidereal_time(m_Location.longitude), 0., 0.).azimuth;
    LOGF_DEBUG("North Azimuth = %lf", northAz);
    return northAz;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::syncCoordWrapPosition()
{
    uint32_t coordWrapPosition = 0;
    if (CordWrapBaseSP[CW_BASE_SKY].s == ISS_ON)
        coordWrapPosition = range360(m_RequestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
    else
        coordWrapPosition = range360(m_RequestedCordwrapPos) * STEPS_PER_DEGREE;
    setCordWrapPosition(coordWrapPosition);
    getCordWrapPosition();
}

/////////////////////////////////////////////////////////////////////////////////////
/// TODO: Make adjustment for the approx time it takes to slew to the given pos.
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Goto(double ra, double dec)
{
    LOGF_DEBUG("Goto - Celestial reference frame target RA:%lf(%lf h) Dec:%lf", ra * 360.0 / 24.0, ra, dec);
    if (ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, ra, 2, 3600);
        fs_sexa(DecStr, dec, 2, 3600);
        CurrentTrackingTarget.rightascension  = ra;
        CurrentTrackingTarget.declination = dec;
        NewTrackingTarget         = CurrentTrackingTarget;
        LOGF_INFO("Slewing to RA: %s DE: %s with tracking.", RAStr, DecStr);
    }

    GoToTarget.rightascension  = ra;
    GoToTarget.declination = dec;

    double timeshift = 0.0;
    if (ScopeStatus != APPROACH)
    {
        // The scope is not in slow approach mode - target should be modified
        // for precission approach. We go to the position from some time ago,
        // to keep the motors going in the same direction as in tracking

        // Three minutes worth of tracking
        // 2021-02-21 JM: Shouldn't this depend on HOW FAR we are from target?
        // Why is it constant?
        timeshift = 3.0 / (24.0 * 60.0);
    }

    // Call the alignment subsystem to translate the celestial reference frame coordinate
    // into a telescope reference frame coordinate
    TelescopeDirectionVector TDV;
    INDI::IHorizontalCoordinates AltAz;

    AltAz = AltAzFromRaDec(ra, dec, -timeshift);

    // For high Alt azimuth may change very fast.
    // Let us limit azimuth approach to maxApproach degrees
    if (ScopeStatus != APPROACH)
    {
        INDI::IHorizontalCoordinates trgAltAz = AltAzFromRaDec(ra, dec, 0);
        double d;

        d = anglediff(AltAz.azimuth, trgAltAz.azimuth);
        LOGF_DEBUG("Azimuth approach:  %lf (%lf)", d, Approach);
        AltAz.azimuth = trgAltAz.azimuth + ((d > 0) ? Approach : -Approach);

        d = anglediff(AltAz.altitude, trgAltAz.altitude);
        LOGF_DEBUG("Altitude approach:  %lf (%lf)", d, Approach);
        AltAz.altitude = trgAltAz.altitude + ((d > 0) ? Approach : -Approach);
    }

    // Fold Azimuth into 0-360
    AltAz.azimuth = range360(AltAz.azimuth);
    // Altitude encoder runs -90 to +90 there is no point going outside.
    if (AltAz.altitude > 90.0)
        AltAz.altitude = 90.0;
    if (AltAz.altitude < -90.0)
        AltAz.altitude = -90.0;

    LOGF_DEBUG("Goto: Scope reference frame target altitude %lf azimuth %lf", AltAz.altitude, AltAz.azimuth);

    TrackState = SCOPE_SLEWING;
    if (ScopeStatus == APPROACH)
    {
        // We need to make a slow slew to approach the final position
        ScopeStatus = SLEWING_SLOW;
        GoToSlow(long(AltAz.altitude * STEPS_PER_DEGREE),
                 long(AltAz.azimuth * STEPS_PER_DEGREE),
                 ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s);
    }
    else
    {
        // Just make a standard fast slew
        slewTicks   = 0;
        ScopeStatus = SLEWING_FAST;
        GoToFast(long(AltAz.altitude * STEPS_PER_DEGREE),
                 long(AltAz.azimuth * STEPS_PER_DEGREE),
                 ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP) + 1;
    LOGF_DEBUG("MoveNS dir:%d, cmd:%d, rate:%d", dir, command, rate);
    AxisDirectionALT = (dir == DIRECTION_NORTH) ? FORWARD : REVERSE;
    AxisStatusALT    = (command == MOTION_START) ? SLEWING : STOPPED;
    ScopeStatus      = SLEWING_MANUAL;
    TrackState       = SCOPE_SLEWING;
    if (command == MOTION_START)
    {
        return SlewALT(((AxisDirectionALT == FORWARD) ? 1 : -1) * rate);
    }
    else
        return SlewALT(0);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////

bool CelestronAUX::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP);
    LOGF_DEBUG("MoveWE dir:%d, cmd:%d, rate:%d", dir, command, rate);
    AxisDirectionAZ = (dir == DIRECTION_WEST) ? FORWARD : REVERSE;
    AxisStatusAZ    = (command == MOTION_START) ? SLEWING : STOPPED;
    ScopeStatus     = SLEWING_MANUAL;
    TrackState      = SCOPE_SLEWING;
    if (command == MOTION_START)
    {
        switch (rate)
        {
            case SLEW_GUIDE:
                rate = GUIDE_SLEW_RATE;
                break;

            case SLEW_CENTERING:
                rate = CENTERING_SLEW_RATE;
                break;

            case SLEW_FIND:
                rate = FIND_SLEW_RATE;
                break;

            default:
                rate = MAX_SLEW_RATE;
                break;
        }
        return SlewAZ(((AxisDirectionAZ == FORWARD) ? -1 : 1) * rate);
    }
    else
        return SlewAZ(0);
}


//++++ autoguide

IPState CelestronAUX::GuideNorth(uint32_t ms)
{
    LOGF_DEBUG("Guiding: N %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateN[AXIS_DE].value);

    GuidePulse(AXIS_DE, ms, rate);

    return IPS_BUSY;
}

IPState CelestronAUX::GuideSouth(uint32_t ms)
{
    LOGF_DEBUG("Guiding: S %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateN[AXIS_DE].value);

    GuidePulse(AXIS_DE, ms, -rate);

    return IPS_BUSY;
}

IPState CelestronAUX::GuideEast(uint32_t ms)
{
    LOGF_DEBUG("Guiding: E %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateN[AXIS_RA].value);

    GuidePulse(AXIS_RA, ms, -rate);

    return IPS_BUSY;
}

IPState CelestronAUX::GuideWest(uint32_t ms)
{
    LOGF_DEBUG("Guiding: W %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateN[AXIS_RA].value);

    GuidePulse(AXIS_RA, ms, rate);

    return IPS_BUSY;
}

bool CelestronAUX::GuidePulse(INDI_EQ_AXIS axis, uint32_t ms, int8_t rate)
{
    uint8_t ticks = std::min(uint32_t(255), ms / 10);
    AUXBuffer data(2);
    data[0] = rate;
    data[1] = ticks;
    AUXCommand cmd(MC_AUX_GUIDE, APP, axis == AXIS_DE ? ALT : AZM, data);
    return sendAUXCommand(cmd);
}


bool CelestronAUX::HandleGetAutoguideRate(INDI_EQ_AXIS axis, uint8_t rate)
{
    switch (axis)
    {
        case AXIS_DE:
            GuideRateN[AXIS_DE].value = rate;
            break;
        case AXIS_RA:
            GuideRateN[AXIS_RA].value = rate;
            break;
    }

    return true;
}

bool CelestronAUX::HandleSetAutoguideRate(INDI_EQ_AXIS axis)
{
    INDI_UNUSED(axis);
    return true;
}

bool CelestronAUX::HandleGuidePulse(INDI_EQ_AXIS axis)
{
    INDI_UNUSED(axis);
    return true;
}

bool CelestronAUX::HandleGuidePulseDone(INDI_EQ_AXIS axis, bool done)
{
    if (done)
        GuideComplete(axis);

    return true;
}

//---- autoguide

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::trackingRequested()
{
    return (ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    TelescopeDirectionVector TDV;
    INDI::IEquatorialCoordinates RaDec;
    INDI::IHorizontalCoordinates AltAz;
    double RightAscension, Declination;

    if (MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
    {
        RaDec.rightascension   = double(GetAZ()) / STEPS_PER_DEGREE;
        RaDec.declination  = double(GetALT()) / STEPS_PER_DEGREE;
        TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(RaDec);

        if (TraceThisTick)
            LOGF_DEBUG("ReadScopeStatus - HA %lf deg ; Dec %lf deg", RaDec.rightascension, RaDec.declination);
    }
    else
    {
        // libnova indexes Az from south while Celestron controllers index from north
        // Never mix two controllers/drivers they will never agree perfectly.
        // Furthermore the celestron hand controler resets the position encoders
        // on alignment and this will mess-up all orientation in the driver.
        // Here we are not attempting to make the driver agree with the hand
        // controller (That would involve adding 180deg here to the azimuth -
        // this way the celestron nexstar driver and this would agree in some
        // situations but not in other - better not to attepmpt impossible!).
        AltAz.azimuth  = double(GetAZ()) / STEPS_PER_DEGREE;
        AltAz.altitude = double(GetALT()) / STEPS_PER_DEGREE;
        TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);


    }

    if (TransformTelescopeToCelestial(TDV, RightAscension, Declination))
    {


        // In case we are slewing while tracking update the potential target
        NewTrackingTarget.rightascension  = RightAscension;
        NewTrackingTarget.declination = Declination;
        NewRaDec(RightAscension, Declination);

        return true;
    }

    LOG_ERROR("ReadScopeStatus - TransformTelescopeToCelestial failed");
    LOG_ERROR("Activate the Alignment Subsystem");

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Sync(double ra, double dec)
{
    AlignmentDatabaseEntry NewEntry;
    INDI::IEquatorialCoordinates RaDec {0, 0};
    INDI::IHorizontalCoordinates AltAz {0, 0};

    if (MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
    {
        RaDec.rightascension   = double(GetAZ()) / STEPS_PER_DEGREE;
        RaDec.declination  = double(GetALT()) / STEPS_PER_DEGREE;
    }
    else
    {
        AltAz.azimuth  = double(GetAZ()) / STEPS_PER_DEGREE;
        AltAz.altitude = double(GetALT()) / STEPS_PER_DEGREE;
    }

    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension        = ra;
    NewEntry.Declination           = dec;

    if (MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromLocalHourAngleDeclination(RaDec);
    else
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);

    NewEntry.PrivateDataSize = 0;

    LOGF_DEBUG("Sync - Celestial reference frame target right ascension %lf(%lf) declination %lf",
               ra * 360.0 / 24.0, ra, dec);

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);

        // Tell the client about size change
        UpdateSize();

        // equatorial/telescope conversions needs more than 1 sync point
        if (GetAlignmentDatabase().size() < 2 &&
                MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
            LOG_WARN("Equatorial mounts need two SYNC points at least.");

        // Tell the math plugin to reinitialise
        Initialise(this);
        LOGF_DEBUG("Sync - new entry added RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);

        // update cordwrap position at each init of the alignment subsystem
        long cwpos;
        if (cw_base_sky)
            cwpos = range360(m_RequestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
        else
            cwpos = range360(m_RequestedCordwrapPos) * STEPS_PER_DEGREE;
        setCordWrapPosition(cwpos);
        getCordWrapPosition();

        // update tracking target
        ReadScopeStatus();
        CurrentTrackingTarget = NewTrackingTarget;

        return true;
    }
    LOGF_DEBUG("Sync - duplicate entry RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::TimerHit()
{

    TimerTick(dt);

    // This will call ReadScopeStatus
    INDI::Telescope::TimerHit();

    // OK I have updated the celestial reference frame RA/DEC in ReadScopeStatus
    // Now handle the tracking state
    switch (TrackState)
    {
        case SCOPE_PARKING:
            if (!isSlewing())
            {
                SetTrackEnabled(false);
                SetParked(true);
                LOG_DEBUG("Telescope parked.");
            }
            break;

        case SCOPE_SLEWING:
            if (isSlewing())
            {

                break;
            }
            else
            {
                // The slew has finished check if that was a coarse slew
                // or precission approach
                if (ScopeStatus == SLEWING_FAST)
                {
                    // This was coarse slew. Execute precise approach.
                    ScopeStatus = APPROACH;
                    Goto(GoToTarget.rightascension, GoToTarget.declination);
                    break;
                }
                else if (trackingRequested())
                {
                    // Precise Goto or manual slew has finished.
                    // Start tracking if requested.
                    if (ScopeStatus == SLEWING_MANUAL)
                    {
                        // We have been slewing manually.
                        // Update the tracking target.
                        CurrentTrackingTarget = NewTrackingTarget;
                    }
                    LOGF_DEBUG("Goto finished start tracking TargetRA: %f TargetDEC: %f",
                               CurrentTrackingTarget.rightascension, CurrentTrackingTarget.declination);
                    TrackState = SCOPE_TRACKING;
                    // Fall through to tracking case
                }
                else
                {
                    LOG_DEBUG("Goto finished. No tracking requested");
                    // Precise goto or manual slew finished.
                    // No tracking requested -> go idle.
                    TrackState = SCOPE_IDLE;
                    break;
                }
            }
            break;

        case SCOPE_TRACKING:
        {
            /*
            TODO
            The tracking should take into account movement of the scope
            by the hand controller (we can hear the commands)
            and the movements made by the joystick.
            Right now when we move the scope by HC it returns to the
            designated target by corrective tracking.
            */

            // Continue or start tracking
            // Calculate where the mount needs to be in a minute (+/- 30s)
            double JulianOffset = 30.0 / (24.0 * 60 * 60);
            TelescopeDirectionVector TDV;
            INDI::IHorizontalCoordinates AAplus, AAzero, AAminus;

            AAplus  = AltAzFromRaDec(CurrentTrackingTarget.rightascension, CurrentTrackingTarget.declination, JulianOffset);
            AAzero = AltAzFromRaDec(CurrentTrackingTarget.rightascension, CurrentTrackingTarget.declination, 0);
            AAminus  = AltAzFromRaDec(CurrentTrackingTarget.rightascension, CurrentTrackingTarget.declination, -JulianOffset);

            // Fold Azimuth into 0-360
            AAminus.azimuth = range360(AAminus.azimuth);
            AAzero.azimuth = range360(AAzero.azimuth);
            AAplus.azimuth = range360(AAplus.azimuth);
            {
                double altRate, azRate;
                double altError, azError;

                // Proportional term in the control loop
                // Weight of error in the control loop
                // For now set at zero - no corrective tracking
                double Kp {0.0};

                // This is in steps per minute
                altError = AAzero.altitude * STEPS_PER_DEGREE - GetALT();
                azError  = range360(AAzero.azimuth) * STEPS_PER_DEGREE - GetAZ();

                altRate =  STEPS_PER_DEGREE * (AAplus.altitude - AAminus.altitude);
                azRate = STEPS_PER_DEGREE * anglediff(AAplus.azimuth, AAminus.azimuth);

                if (TraceThisTick)
                    LOGF_DEBUG("Target (AltAz): %f  %f  Scope  (AltAz)  %f  %f", AAzero.altitude, AAzero.azimuth,
                               GetALT() / STEPS_PER_DEGREE, GetAZ() / STEPS_PER_DEGREE);

                // Fold the Az difference to +/- STEPS_PER_REVOLUTION / 2
                while (azError > STEPS_PER_REVOLUTION / 2)
                    azError -= STEPS_PER_REVOLUTION;
                while (azError < -(STEPS_PER_REVOLUTION / 2))
                    azError += STEPS_PER_REVOLUTION;

                if (TraceThisTick)
                    LOGF_DEBUG("Tracking rate: Alt %f Az %f ; Errors : Alt: %f Az: %f",
                               altRate, azRate, altError, azError);

                altRate = SIDERAL_RATE * TRACK_SCALE * (altRate + altError * Kp);
                azRate  = SIDERAL_RATE * TRACK_SCALE * (azRate + azError * Kp);

                Track(static_cast<int32_t>(altRate), static_cast<int32_t>(azRate));

            }
            break;
        }

        default:
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::updateLocation(double latitude, double longitude, double elevation)
{
    // Update INDI Alignment Subsystem Location
    UpdateLocation(latitude, longitude, elevation);

    // Do we really need this in update Location??
    // take care of latitude for north or south emisphere
    //SetApproximateMountAlignmentFromMountType(static_cast<MountType>(IUFindOnSwitchIndex(&MountTypeSP)));
    // tell the alignment math plugin to reinitialise
    //Initialise(this);

    // update cordwrap position at each init of the alignment subsystem
    syncCoordWrapPosition();

    return true;
}

int32_t CelestronAUX::clampStepsPerRevolution(int32_t steps)
{
    while (steps < 0)
        steps += STEPS_PER_REVOLUTION;
    while (steps > STEPS_PER_REVOLUTION)
        steps -= STEPS_PER_REVOLUTION;

    return steps;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::isSlewing()
{
    return m_SlewingAlt || m_SlewingAz;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Slew(AUXTargets target, int rate)
{
    AUXCommand cmd((rate < 0) ? MC_MOVE_NEG : MC_MOVE_POS, APP, target);
    cmd.setRate((unsigned char)(std::abs(rate) & 0xFF));

    sendAUXCommand(cmd);
    readAUXResponse(cmd);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::SlewALT(int rate)
{
    m_SlewingAlt = (rate != 0);
    return Slew(ALT, rate);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::SlewAZ(int rate)
{
    m_SlewingAz = (rate != 0);
    return Slew(AZM, rate);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::GoToFast(int32_t alt, int32_t az, bool track)
{
    targetAlt  = alt;
    targetAz   = az;
    m_Tracking   = track;
    m_SlewingAlt = m_SlewingAz = true;
    Track(0, 0);
    AUXCommand altcmd(MC_GOTO_FAST, APP, ALT);
    AUXCommand azmcmd(MC_GOTO_FAST, APP, AZM);
    altcmd.set24bitValue(alt);
    azmcmd.set24bitValue(az);
    sendAUXCommand(altcmd);
    readAUXResponse(altcmd);
    sendAUXCommand(azmcmd);
    readAUXResponse(azmcmd);
    //DEBUG=false;
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::GoToSlow(int32_t alt, int32_t az, bool track)
{
    targetAlt  = alt;
    targetAz   = az;
    m_Tracking   = track;
    m_SlewingAlt = m_SlewingAz = true;
    Track(0, 0);
    AUXCommand altcmd(MC_GOTO_SLOW, APP, ALT);
    AUXCommand azmcmd(MC_GOTO_SLOW, APP, AZM);
    altcmd.set24bitValue(alt);
    azmcmd.set24bitValue(az);
    sendAUXCommand(altcmd);
    readAUXResponse(altcmd);
    sendAUXCommand(azmcmd);
    readAUXResponse(azmcmd);
    //DEBUG=false;
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::getVersion(AUXTargets target)
{
    AUXCommand firmver(GET_VER, APP, target);
    if (! sendAUXCommand(firmver))
        return false;
    if (! readAUXResponse(firmver))
        return false;
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::getVersions()
{
    if (!m_isHandController)
    {
        // Do not ask HC/MB for the version over AUX channel
        // We got HC version from detectHC
        getVersion(MB);
        getVersion(HC);
        getVersion(HCP);
    }
    getVersion(AZM);
    getVersion(ALT);
    getVersion(GPS);
    getVersion(WiFi);
    getVersion(BAT);

    // These are the same as battery controller
    // Probably the same chip inside the mount
    //getVersion(CHG);
    //getVersion(LIGHT);
    //getVersion(ANY);
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::setCordWrapEnabled(bool enable)
{

    AUXCommand command(enable ? MC_ENABLE_CORDWRAP : MC_DISABLE_CORDWRAP, APP, AZM);
    LOGF_INFO("setCordWrap before %d", m_CordWrapActive);
    sendAUXCommand(command);
    readAUXResponse(command);
    LOGF_INFO("setCordWrap after %d", m_CordWrapActive);
    return true;
};

bool CelestronAUX::getCordWrapEnabled()
{
    AUXCommand command(MC_POLL_CORDWRAP, APP, AZM);
    LOGF_INFO("getCordWrap before %d", m_CordWrapActive);
    sendAUXCommand(command);
    readAUXResponse(command);
    LOGF_INFO("getCordWrap after %d", m_CordWrapActive);
    return m_CordWrapActive;
};


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::setCordWrapPosition(int32_t pos)
{
    AUXCommand command(MC_SET_CORDWRAP_POS, APP, AZM);
    command.set24bitValue(pos);
    LOGF_INFO("setCordwrapPos %.1f deg", (pos / STEPS_PER_DEGREE) );
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
uint32_t CelestronAUX::getCordWrapPosition()
{
    AUXCommand command(MC_GET_CORDWRAP_POS, APP, AZM);
    sendAUXCommand(command);
    readAUXResponse(command);
    LOGF_INFO("getCordwrapPos %.1f deg", (m_CordWrapPosition / STEPS_PER_DEGREE) );
    return m_CordWrapPosition;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Track(int32_t altRate, int32_t azRate)
{
    // The scope rates are per minute?
    m_AltRate = altRate;
    m_AzRate  = azRate;
    if (m_SlewingAlt || m_SlewingAz)
    {
        m_AltRate = 0;
        m_AzRate  = 0;
    }
    AUXCommand altcmd((altRate < 0) ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, ALT);
    AUXCommand azmcmd((azRate < 0) ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, AZM);
    altcmd.set24bitValue(std::abs(m_AltRate));
    azmcmd.set24bitValue(std::abs(m_AzRate));

    sendAUXCommand(altcmd);
    readAUXResponse(altcmd);
    sendAUXCommand(azmcmd);
    readAUXResponse(azmcmd);
    return true;
};


bool CelestronAUX::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        if (TrackState == SCOPE_IDLE)
        {
            LOG_INFO("Start Tracking.");
            CurrentTrackingTarget = NewTrackingTarget;
            TrackState = SCOPE_TRACKING;
        }
    }
    else
    {
        if (TrackState == SCOPE_TRACKING || TrackState == SCOPE_PARKING)
        {
            LOG_WARN("Stopping Tracking.");
            TrackState = SCOPE_IDLE;
            AxisStatusAZ = AxisStatusALT = STOPPED;
            ScopeStatus = IDLE;
            Track(0, 0);
            AUXBuffer b(1);
            b[0] = 0;
            AUXCommand stopAlt(MC_MOVE_POS, APP, ALT, b);
            AUXCommand stopAz(MC_MOVE_POS, APP, AZM, b);
            sendAUXCommand(stopAlt);
            readAUXResponse(stopAlt);
            sendAUXCommand(stopAz);
            readAUXResponse(stopAz);
        }
    }

    return true;
};


int debug_timeout = 30;

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::TimerTick(double dt)
{
    queryStatus();

    if (isSimulation())
    {
        bool slewing = false;
        int32_t da;
        int dir;

        // update both axis
        if (m_AltSteps != targetAlt)
        {
            da  = targetAlt - m_AltSteps;
            dir = (da > 0) ? 1 : -1;
            m_AltSteps += dir * std::max(std::min(std::abs(da) / 2, slewRate), 1);
            slewing = true;
        }
        if (m_AzSteps != targetAz)
        {
            da  = targetAz - m_AzSteps;
            dir = (da > 0) ? 1 : -1;
            m_AzSteps += dir * std::max(std::min(static_cast<int32_t>(std::abs(da) / 2), slewRate), 1);
            slewing = true;
        }

        // if we reach the target at previous tick start tracking if tracking requested
        if (m_Tracking && !slewing && m_AltSteps == targetAlt && m_AzSteps == targetAz)
        {
            targetAlt = (m_AltSteps += m_AltRate * dt);
            targetAz  = (m_AzSteps += m_AzRate * dt);
        }
    }
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::queryStatus()
{
    if ( isConnected() )
    {
        AUXTargets trg[2] = { ALT, AZM };
        for (int i = 0; i < 2; i++)
        {
            AUXCommand cmd(MC_GET_POSITION, APP, trg[i]);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
        }
        if (m_SlewingAlt && ScopeStatus != SLEWING_MANUAL)
        {
            AUXCommand cmd(MC_SLEW_DONE, APP, ALT);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
        }
        if (m_SlewingAz && ScopeStatus != SLEWING_MANUAL)
        {
            AUXCommand cmd(MC_SLEW_DONE, APP, AZM);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
/// This is simple GPS emulation for HC.
/// If HC asks for the GPS we reply with data from our GPS/Site info.
/// We send reply blind (not reading any response) to avoid processing loop.
/// That is why the readAUXResponse calls are commented out.
/// This is OK since we are not going to do anything with the response anyway.
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::emulateGPS(AUXCommand &m)
{
    if (m.m_Destination != GPS)
        return;

    LOGF_DEBUG("GPS: Got 0x%02x", m.m_Command);
    if (m_GPSEmulation == false)
        return;

    switch (m.m_Command)
    {
        case GET_VER:
        {
            LOGF_DEBUG("GPS: GET_VER from 0x%02x", m.m_Source);
            AUXBuffer dat(2);
            dat[0] = 0x01;
            dat[1] = 0x02;
            AUXCommand cmd(GET_VER, GPS, m.m_Source, dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_LAT:
        case GPS_GET_LONG:
        {
            LOGF_DEBUG("GPS: Sending LAT/LONG Lat:%f Lon:%f\n", LocationN[LOCATION_LATITUDE].value,
                       LocationN[LOCATION_LONGITUDE].value);
            AUXCommand cmd(m.m_Command, GPS, m.m_Source);
            if (m.m_Command == GPS_GET_LAT)
                cmd.set24bitValue(LocationN[LOCATION_LATITUDE].value);
            else
                cmd.set24bitValue(LocationN[LOCATION_LONGITUDE].value);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_TIME:
        {
            LOGF_DEBUG("GPS: GET_TIME from 0x%02x", m.m_Source);
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(3);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_hour);
            dat[1] = unsigned(ptm->tm_min);
            dat[2] = unsigned(ptm->tm_sec);
            AUXCommand cmd(GPS_GET_TIME, GPS, m.m_Source, dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_DATE:
        {
            LOGF_DEBUG("GPS: GET_DATE from 0x%02x", m.m_Source);
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_mon + 1);
            dat[1] = unsigned(ptm->tm_mday);
            AUXCommand cmd(GPS_GET_DATE, GPS, m.m_Source, dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_YEAR:
        {
            LOGF_DEBUG("GPS: GET_YEAR from 0x%02x", m.m_Source);
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_year + 1900) >> 8;
            dat[1] = unsigned(ptm->tm_year + 1900) & 0xFF;
            LOGF_DEBUG("GPS: Sending: %d [%d,%d]", ptm->tm_year, dat[0], dat[1]);
            AUXCommand cmd(GPS_GET_YEAR, GPS, m.m_Source, dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_LINKED:
        {
            LOGF_DEBUG("GPS: LINKED from 0x%02x", m.m_Source);
            AUXBuffer dat(1);

            dat[0] = unsigned(1);
            AUXCommand cmd(GPS_LINKED, GPS, m.m_Source, dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        default:
            LOGF_DEBUG("GPS: Got 0x%02x", m.m_Command);
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::processResponse(AUXCommand &m)
{
    m.logResponse();

    if (m.m_Destination == GPS)
        emulateGPS(m);
    else if (m.m_Destination == APP)
        switch (m.m_Command)
        {
            case MC_GET_POSITION:
                switch (m.m_Source)
                {
                    case ALT:
                        // The Alt encoder value is signed!
                        m_AltSteps = m.getPosition();
                        DEBUGF(DBG_CAUX, "Got Alt: %ld", m_AltSteps);
                        break;
                    case AZM:
                        // Celestron uses N as zero Azimuth!
                        m_AzSteps = clampStepsPerRevolution(m.getPosition());
                        DEBUGF(DBG_CAUX, "Got Az: %ld", m_AzSteps);
                        break;
                    default:
                        break;
                }
                break;
            case MC_SLEW_DONE:
                switch (m.m_Source)
                {
                    case ALT:
                        m_SlewingAlt = m.m_Data[0] != 0xff;
                        break;
                    case AZM:
                        m_SlewingAz = m.m_Data[0] != 0xff;
                        break;
                    default:
                        break;
                }
                break;
            case MC_POLL_CORDWRAP:
                if (m.m_Source == AZM)
                    m_CordWrapActive = m.m_Data[0] == 0xff;
                break;
            case MC_GET_CORDWRAP_POS:
                if (m.m_Source == AZM)
                {
                    m_CordWrapPosition = clampStepsPerRevolution(m.getPosition());
                    LOGF_DEBUG("Got cordwrap position %.1f", float(m_CordWrapPosition) / STEPS_PER_DEGREE);
                }
                break;

            case MC_GET_AUTOGUIDE_RATE:
                switch (m.m_Source)
                {
                    case ALT:
                        return HandleGetAutoguideRate(AXIS_DE, m.m_Data[0] * 100.0 / 255);
                    case AZM:
                        return HandleGetAutoguideRate(AXIS_RA, m.m_Data[0] * 100.0 / 255);
                    default:
                        IDLog("unknown src 0x%02x\n", m.m_Source);
                }
                break;

            case MC_SET_AUTOGUIDE_RATE:
                switch (m.m_Source)
                {
                    case ALT:
                        return HandleSetAutoguideRate(AXIS_DE);
                    case AZM:
                        return HandleSetAutoguideRate(AXIS_RA);
                    default:
                        IDLog("unknown src 0x%02x\n", m.m_Source);
                }
                break;

            case MC_AUX_GUIDE:
                switch (m.m_Source)
                {
                    case ALT:
                        return HandleGuidePulse(AXIS_DE);
                    case AZM:
                        return HandleGuidePulse(AXIS_RA);
                    default:
                        IDLog("unknown src 0x%02x\n", m.m_Source);
                }
                break;

            case MC_AUX_GUIDE_ACTIVE:
                switch (m.m_Source)
                {
                    case ALT:
                        return HandleGuidePulseDone(AXIS_DE, m.m_Data[0] == 0x00);
                    case AZM:
                        return HandleGuidePulseDone(AXIS_RA, m.m_Data[0] == 0x00);
                    default:
                        IDLog("unknown src 0x%02x\n", m.m_Source);
                }
                break;

            case GET_VER:
                uint8_t *verBuf;

                verBuf = 0;
                if (m.m_Source != APP)
                    LOGF_INFO("Got GET_VERSION response from %s: %d.%d.%d ", m.moduleName(m.m_Source), m.m_Data[0], m.m_Data[1],
                              256 * m.m_Data[2] + m.m_Data[3]);
                switch (m.m_Source)
                {
                    case MB:
                        verBuf = m_MainBoardVersion;
                        break;
                    case ALT:
                        verBuf = m_AltitudeVersion;
                        break;
                    case AZM:
                        verBuf = m_AzimuthVersion;
                        break;
                    case HCP:
                    case HC:
                        verBuf = m_HCVersion;
                        break;
                    case BAT:
                        verBuf = m_BATVersion;
                        break;
                    case WiFi:
                        verBuf = m_WiFiVersion;
                        // Shift data to upper bytes
                        m.m_Data[0] = m.m_Data[2];
                        m.m_Data[1] = m.m_Data[3];
                        // Zero out uninitialized bytes
                        m.m_Data[2] = 0;
                        m.m_Data[3] = 0;
                        break;
                    case GPS:
                        verBuf = m_GPSVersion;
                        // Zero out uninitialized bytes
                        m.m_Data[2] = 0;
                        m.m_Data[3] = 0;
                        break;
                    case APP:
                        LOGF_DEBUG("Got echo of GET_VERSION from %s", m.moduleName(m.m_Destination));
                        break;
                    default:
                        break;
                }
                if (verBuf != 0)
                {
                    memcpy(verBuf, m.m_Data.data(), 4);
                }
                break;
            default:
                break;

        }
    else
    {
        DEBUGF(DBG_CAUX, "Got msg not for me (%s). Ignoring.", m.moduleName(m.m_Destination));
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::serialReadResponse(AUXCommand c)
{
    int n;
    unsigned char buf[32];
    char hexbuf[24];
    AUXCommand cmd;

    // We are not connected. Nothing to do.
    if ( PortFD <= 0 )
        return false;

    if (m_IsRTSCTS || !m_isHandController)
    {
        // if connected to AUX or PC ports, receive AUX command response.
        // search for packet preamble (0x3b)
        do
        {
            if (aux_tty_read(PortFD, (char*)buf, 1, READ_TIMEOUT, &n) != TTY_OK)
                return false;
        }
        while (buf[0] != 0x3b);

        // packet preamble is found, now read packet length.
        if (aux_tty_read(PortFD, (char*)(buf + 1), 1, READ_TIMEOUT, &n) != TTY_OK)
            return false;

        // now packet length is known, read the rest of the packet.
        if (aux_tty_read(PortFD, (char*)(buf + 2), buf[1] + 1, READ_TIMEOUT, &n)
                != TTY_OK || n != buf[1] + 1)
        {
            LOG_DEBUG("Did not got whole packet. Dropping out.");
            return false;
        }

        AUXBuffer b(buf, buf + (n + 2));
        hex_dump(hexbuf, b, b.size());
        DEBUGF(DBG_SERIAL, "RES <%s>", hexbuf);
        cmd.parseBuf(b);
    }
    else
    {
        // if connected to HC serial, build up the AUX command response from
        // given AUX command and passthrough response without checksum.
        // read passthrough response
        if ((tty_read(PortFD, (char *)buf + 5, response_data_size + 1, READ_TIMEOUT, &n) !=
                TTY_OK) || (n != response_data_size + 1))
            return false;

        // if last char is not '#', there was an error.
        if (buf[response_data_size + 5] != '#')
        {
            LOGF_ERROR("Resp. char %d is %2.2x ascii %c", n, buf[n + 5], (char)buf[n + 5]);
            AUXBuffer b(buf, buf + (response_data_size + 5));
            hex_dump(hexbuf, b, b.size());
            LOGF_ERROR("RES <%s>", hexbuf);
            return false;
        }

        buf[0] = 0x3b;
        buf[1] = response_data_size + 1;
        buf[2] = c.m_Destination;
        buf[3] = c.m_Source;
        buf[4] = c.m_Command;

        AUXBuffer b(buf, buf + (response_data_size + 5));
        hex_dump(hexbuf, b, b.size());
        DEBUGF(DBG_SERIAL, "RES (%d B): <%s>", (int)b.size(), hexbuf);
        cmd.parseBuf(b, false);
    }

    // Got the packet, process it
    // n:length field >=3
    // The buffer of n+2>=5 bytes contains:
    // 0x3b <n>=3> <from> <to> <type> <n-3 bytes> <xsum>

    DEBUGF(DBG_SERIAL, "Got %d bytes:  ; payload length field: %d ; MSG:", n, buf[1]);
    logBytes(buf, n + 2, getDeviceName(), DBG_SERIAL);
    processResponse(cmd);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::tcpReadResponse()
{
    int n, i;
    unsigned char buf[BUFFER_SIZE] = {0};
    AUXCommand cmd;

    // We are not connected. Nothing to do.
    if ( PortFD <= 0 )
        return false;

    timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 50000;
    setsockopt(PortFD, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

    // Drain the channel
    while ((n = recv(PortFD, buf, sizeof(buf), MSG_DONTWAIT | MSG_PEEK)) > 0)
    {
        //        DEBUGF(DBG_CAUX, "Got %d bytes: ", n);
        //        for (i = 0; i < n; i++)
        //            IDLog("%02x ", buf[i]);

        for (i = 0; i < n;)
        {
            if (buf[i] == 0x3b)
            {
                int shft;
                shft = i + buf[i + 1] + 3;
                if (shft <= n)
                {
                    AUXBuffer b(buf + i, buf + shft);
                    cmd.parseBuf(b);

                    char hexbuf[32 * 3] = {0};
                    hex_dump(hexbuf, b, b.size());
                    DEBUGF(DBG_SERIAL, "RES <%s>", hexbuf);

                    processResponse(cmd);
                }
                else
                {
                    DEBUGF(DBG_SERIAL, "Partial message recv. dropping (i=%d %d/%d)", i, shft, n);
                    logBytes(buf + i, n - i, getDeviceName(), DBG_SERIAL);
                    recv(PortFD, buf, n, MSG_DONTWAIT);
                    break;
                }
                i = shft;
            }
            else
            {
                i++;
            }
        }
        // Actually consume data we parsed. Leave the rest for later.
        if (i > 0)
        {
            n = recv(PortFD, buf, i, MSG_DONTWAIT);
            //DEBUGF(DBG_CAUX, "Consumed %d/%d bytes", n, i);
        }
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::readAUXResponse(AUXCommand c)
{
    if (getActiveConnection() == serialConnection)
        return serialReadResponse(c);
    else
        return tcpReadResponse();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int CelestronAUX::sendBuffer(int PortFD, AUXBuffer buf)
{
    if ( PortFD > 0 )
    {
        int n;

        if (aux_tty_write(PortFD, (char*)buf.data(), buf.size(), CTS_TIMEOUT, &n) != TTY_OK)
            return 0;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (n == -1)
            LOG_ERROR("CAUX::sendBuffer");
        if ((unsigned)n != buf.size())
            LOGF_WARN("sendBuffer: incomplete send n=%d size=%d", n, (int)buf.size());

        char hexbuf[32 * 3] = {0};
        hex_dump(hexbuf, buf, buf.size());
        DEBUGF(DBG_SERIAL, "CMD <%s>", hexbuf);

        return n;
    }
    else
        return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::sendAUXCommand(AUXCommand &c)
{
    AUXBuffer buf;
    c.logCommand();

    if (m_IsRTSCTS || !m_isHandController || getActiveConnection() != serialConnection)
        // Direct connection (AUX/PC/USB port)
        c.fillBuf(buf);
    else
    {
        // connection is through HC serial and destination is not HC,
        // convert AUX command to a passthrough command
        buf.resize(8);                 // fixed len = 8
        buf[0] = 0x50;                 // prefix
        buf[1] = 1 + c.m_Data.size();    // length
        buf[2] = c.m_Destination;                // destination
        buf[3] = c.m_Command;                // command id
        for (unsigned int i = 0; i < c.m_Data.size(); i++) // payload
        {
            buf[i + 4] = c.m_Data[i];
        }
        buf[7] = response_data_size = c.responseDataSize();
    }

    tcflush(PortFD, TCIOFLUSH);
    return (sendBuffer(PortFD, buf) == static_cast<int>(buf.size()));
}


////////////////////////////////////////////////////////////////////////////////
// Wrap functions around the standard driver communication functions tty_read
// and tty_write.
// When the communication is serial, these wrap functions implement the
// Celestron hardware handshake used by telescope serial ports AUX and PC.
// When the communication is by network, these wrap functions are trasparent.
// Read and write calls are passed, as is, to the standard functions tty_read
// and tty_write.
// 16-Feb-2020 Fabrizio Pollastri <mxgbot@gmail.com>
////////////////////////////////////////////////////////////////////////////////


void CelestronAUX::setRTS(bool rts)
{
    if (ioctl(PortFD, TIOCMGET, &modem_ctrl) == -1)
        LOGF_ERROR("Error getting handshake lines %s(%d).", strerror(errno), errno);
    if (rts)
        modem_ctrl |= TIOCM_RTS;
    else
        modem_ctrl &= ~TIOCM_RTS;
    if (ioctl(PortFD, TIOCMSET, &modem_ctrl) == -1)
        LOGF_ERROR("Error setting handshake lines %s(%d).", strerror(errno), errno);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::waitCTS(float timeout)
{
    float step = timeout / 20.;
    for (; timeout >= 0; timeout -= step)
    {
        msleep(step);
        if (ioctl(PortFD, TIOCMGET, &modem_ctrl) == -1)
        {
            LOGF_ERROR("Error getting handshake lines %s(%d).", strerror(errno), errno);
            return 0;
        }
        if (modem_ctrl & TIOCM_CTS)
            return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::detectRTSCTS()
{
    setRTS(1);
    bool retval = waitCTS(300.);
    setRTS(0);
    return retval;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::detectHC(char *version, size_t size)
{
    AUXBuffer b;
    char buf[3];

    b.resize(1);
    b[0] = 'V';

    // We are not connected. Nothing to do.
    if ( PortFD <= 0 )
        return false;

    // send get firmware version command
    if (sendBuffer(PortFD, b) != (int)b.size())
        return false;

    // read response
    int n;
    if (aux_tty_read(PortFD, (char*)buf, 3, READ_TIMEOUT, &n) != TTY_OK)
        return false;

    // non error response must end with '#'
    if (buf[2] != '#')
        return false;

    // return printable HC version
    // fill in the version field
    m_HCVersion[0] = static_cast<uint8_t>(buf[0]);
    m_HCVersion[1] = static_cast<uint8_t>(buf[1]);
    m_HCVersion[2] = 0;
    m_HCVersion[3] = 0;
    snprintf(version, size, "%d.%02d", static_cast<uint8_t>(buf[0]), static_cast<uint8_t>(buf[1]));

    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int CelestronAUX::aux_tty_read(int PortFD, char *buf, int bufsiz, int timeout, int *n)
{
    int errcode;
    DEBUGF(DBG_SERIAL, "aux_tty_read: %d", PortFD);

    // if hardware flow control is required, set RTS to off to receive: PC port
    // bahaves as half duplex.
    if (m_IsRTSCTS)
        setRTS(0);

    if((errcode = tty_read(PortFD, buf, bufsiz, timeout, n)) != TTY_OK)
    {
        char errmsg[MAXRBUF] = {0};
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
    }

    return errcode;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int CelestronAUX::aux_tty_write(int PortFD, char *buf, int bufsiz, float timeout, int *n)
{
    int errcode, ne;
    char errmsg[MAXRBUF];

    //DEBUGF(DBG_CAUX, "aux_tty_write: %d", PortFD);

    // if hardware flow control is required, set RTS to on then wait for CTS
    // on to write: PC port bahaves as half duplex. RTS may be already on.
    if (m_IsRTSCTS)
    {
        DEBUG(DBG_SERIAL, "aux_tty_write: set RTS");
        setRTS(1);
        DEBUG(DBG_SERIAL, "aux_tty_write: wait CTS");
        if (!waitCTS(timeout))
        {
            LOGF_ERROR("Error getting handshake lines %s(%d).\n", strerror(errno), errno);
            return TTY_TIME_OUT;
        }
    }

    errcode = tty_write(PortFD, buf, bufsiz, n);

    if (errcode != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return errcode;
    }

    // if hardware flow control is required, Wait for tx complete, set RTS to
    // off, to receive (half duplex).
    if (m_IsRTSCTS)
    {
        DEBUG(DBG_SERIAL, "aux_tty_write: clear RTS");
        msleep(RTS_DELAY);
        setRTS(0);

        // ports requiring hardware flow control echo all sent characters,
        // verify them.
        DEBUG(DBG_SERIAL, "aux_tty_write: verify echo");
        if ((errcode = tty_read(PortFD, errmsg, *n, READ_TIMEOUT, &ne)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return errcode;
        }

        if (*n != ne)
            return TTY_WRITE_ERROR;

        for (int i = 0; i < ne; i++)
            if (buf[i] != errmsg[i])
                return TTY_WRITE_ERROR;
    }

    return TTY_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::tty_set_speed(int PortFD, speed_t speed)
{
    struct termios tty_setting;

    if (tcgetattr(PortFD, &tty_setting))
    {
        LOGF_ERROR("Error getting tty attributes %s(%d).", strerror(errno), errno);
        return false;
    }

    if (cfsetspeed(&tty_setting, speed))
    {
        LOGF_ERROR("Error setting serial speed %s(%d).", strerror(errno), errno);
        return false;
    }

    if (tcsetattr(PortFD, TCSANOW, &tty_setting))
    {
        LOGF_ERROR("Error setting tty attributes %s(%d).", strerror(errno), errno);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::hex_dump(char *buf, AUXBuffer data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

