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

#include <alignment/DriverCommon.h>
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
    : ScopeStatus(IDLE),
      DBG_CAUX(INDI::Logger::getInstance().addDebugLevel("AUX", "CAUX")),
      DBG_SERIAL(INDI::Logger::getInstance().addDebugLevel("Serial", "CSER"))
{
    setVersion(CAUX_VERSION_MAJOR, CAUX_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_HAS_TRACK_RATE
                           , 8);

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
                if (!tty_set_speed(B19200))
                    return false;
                LOG_INFO("Setting serial speed to 19200 baud.");
            }
            else
            {
                serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
                if (!tty_set_speed(B9600))
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

    /////////////////////////////////////////////////////////////////////////////////////
    /// Main Control Tab
    /////////////////////////////////////////////////////////////////////////////////////
    // Mount type
    MountTypeSP[EQUATORIAL].fill("EQUATORIAL", "Equatorial", ISS_OFF);
    MountTypeSP[ALTAZ].fill("ALTAZ", "AltAz", ISS_ON);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Cord Wrap Tab
    /////////////////////////////////////////////////////////////////////////////////////

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
    CordWrapBaseSP.fill(getDeviceName(), "CW_BASE", "CW Position Base", CORDWRAP_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Options
    /////////////////////////////////////////////////////////////////////////////////////
    // GPS Emulation
    GPSEmuSP[GPSEMU_OFF].fill("GPSEMU_OFF", "OFF", ISS_OFF);
    GPSEmuSP[GPSEMU_ON].fill("GPSEMU_ON", "ON", ISS_ON);
    GPSEmuSP.fill(getDeviceName(), "GPSEMU", "GPS Emu", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Guide Tab
    /////////////////////////////////////////////////////////////////////////////////////

    // Guide Properties
    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    // Rate rate
    GuideRateNP[AXIS_AZ].fill("GUIDE_RATE_WE", "W/E Rate", "%.f", 10, 100, 1, 50);
    GuideRateNP[AXIS_ALT].fill("GUIDE_RATE_NS", "N/S Rate", "%.f", 10, 100, 1, 50);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Mount Information
    /////////////////////////////////////////////////////////////////////////////////////

    // Raw encoder values
    EncoderNP[AXIS_AZ].fill("AXIS_AZ", "Axis 1", "%.f", 0, STEPS_PER_REVOLUTION, 0, 0);
    EncoderNP[AXIS_ALT].fill("AXIS_ALT", "Axis 2", "%.f", 0, STEPS_PER_REVOLUTION, 0, 0);
    EncoderNP.fill(getDeviceName(), "TELESCOPE_ENCODER_STEPS", "Encoders", MOUNTINFO_TAB, IP_RW, 60, IPS_IDLE);

    // Encoder -> Angle values
    AngleNP[AXIS_AZ].fill("AXIS_AZ", "Axis 1", "%.2f", 0, 360, 0, 0);
    AngleNP[AXIS_ALT].fill("AXIS_ALT", "Axis 2", "%.2f", -90, 90, 0, 0);
    AngleNP.fill(getDeviceName(), "TELESCOPE_ENCODER_ANGLES", "Angles", MOUNTINFO_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware Info
    FirmwareTP[FW_HC].fill("HC version", "", nullptr);
    FirmwareTP[FW_MB].fill("Mother Board version", "", nullptr);
    FirmwareTP[FW_AZM].fill("Ra/AZM version", "", nullptr);
    FirmwareTP[FW_ALT].fill("Dec/ALT version", "", nullptr);
    FirmwareTP[FW_WiFi].fill("WiFi version", "", nullptr);
    FirmwareTP[FW_BAT].fill("Battery version", "", nullptr);
    FirmwareTP[FW_GPS].fill("GPS version", "", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware Info", "Firmware Info", MOUNTINFO_TAB, IP_RO, 0, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Initial Configuration
    /////////////////////////////////////////////////////////////////////////////////////

    // Set Debug Information for AUX Commands
    AUXCommand::setDebugInfo(getDeviceName(), DBG_CAUX);

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

    // to update cordwrap pos at each init of alignment subsystem
    IDSnoopDevice(getDeviceName(), "ALIGNMENT_SUBSYSTEM_MATH_PLUGIN_INITIALISE");

    // JM 2020-09-23 Make it easier for users to connect by default via WiFi if they
    // selected the Celestron WiFi labeled driver.
    if (strstr(getDeviceName(), "WiFi"))
        setActiveConnection(tcpConnection);

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
        // Main Control Panel
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

        // Encoders
        defineProperty(&EncoderNP);
        defineProperty(&AngleNP);

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

        deleteProperty(EncoderNP.getName());
        deleteProperty(AngleNP.getName());
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
        // Guide Rate
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, n);
            GuideRateNP.setState(IPS_OK);
            GuideRateNP.apply();
            return true;
        }

        // Encoder Values
        if (EncoderNP.isNameMatch(name))
        {
            uint32_t axisSteps1 {0}, axisSteps2 {0};
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], EncoderNP[AXIS_AZ].getName()))
                    axisSteps1 = values[i];
                else if (!strcmp(names[i], EncoderNP[AXIS_ALT].getName()))
                    axisSteps2 = values[i];
            }


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
    if (!strcmp(dev, getDeviceName()))
        ProcessAlignmentTextProperties(this, name, texts, names, n);

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
    // TODO implement custom parking
    slewTo(AXIS_AZ, LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
    slewTo(AXIS_ALT, 0);

    LOG_INFO("Parking in progress...");
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
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP) + 1;
    m_AxisDirection[AXIS_ALT] = (dir == DIRECTION_NORTH) ? FORWARD : REVERSE;
    m_AxisStatus[AXIS_ALT] = (command == MOTION_START) ? SLEWING : STOPPED;
    ScopeStatus      = SLEWING_MANUAL;
    TrackState       = SCOPE_SLEWING;
    if (command == MOTION_START)
    {
        return slewByRate(AXIS_ALT, ((m_AxisDirection[AXIS_ALT] == FORWARD) ? 1 : -1) * rate);
    }
    else
        return stopAxis(AXIS_ALT);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP) + 1;
    m_AxisDirection[AXIS_AZ] = (dir == DIRECTION_WEST) ? FORWARD : REVERSE;
    m_AxisStatus[AXIS_AZ] = (command == MOTION_START) ? SLEWING : STOPPED;
    ScopeStatus      = SLEWING_MANUAL;
    TrackState       = SCOPE_SLEWING;
    if (command == MOTION_START)
    {
        return slewByRate(AXIS_AZ, ((m_AxisDirection[AXIS_AZ] == FORWARD) ? 1 : -1) * rate);
    }
    else
        return stopAxis(AXIS_AZ);
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
IPState CelestronAUX::GuideNorth(uint32_t ms)
{
    LOGF_DEBUG("Guiding: N %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_ALT].getValue());

    //guidePulse(AXIS_ALT, ms, rate);

    return IPS_BUSY;
}

IPState CelestronAUX::GuideSouth(uint32_t ms)
{
    LOGF_DEBUG("Guiding: S %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_ALT].getValue());

    //guidePulse(AXIS_ALT, ms, -rate);

    return IPS_BUSY;
}

IPState CelestronAUX::GuideEast(uint32_t ms)
{
    LOGF_DEBUG("Guiding: E %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_AZ].getValue());

    //guidePulse(AXIS_AZ, ms, -rate);

    return IPS_BUSY;
}

IPState CelestronAUX::GuideWest(uint32_t ms)
{
    LOGF_DEBUG("Guiding: W %d ms", ms);

    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_AZ].getValue());

    //guidePulse(AXIS_AZ, ms, rate);

    return IPS_BUSY;
}

bool CelestronAUX::guidePulse(INDI_EQ_AXIS axis, uint32_t ms, int8_t rate)
{
    uint8_t ticks = std::min(uint32_t(255), ms / 10);
    AUXBuffer data(2);
    data[0] = rate;
    data[1] = ticks;
    AUXCommand cmd(MC_AUX_GUIDE, APP, axis == AXIS_DE ? ALT : AZM, data);
    return sendAUXCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::resetTracking()
{
    m_TrackingElapsedTimer.restart();
    m_GuideOffset[AXIS_AZ] = m_GuideOffset[AXIS_ALT] = 0;
}

//bool CelestronAUX::HandleSetAutoguideRate(INDI_EQ_AXIS axis)
//{
//    INDI_UNUSED(axis);
//    return true;
//}


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

    if (!getStatus(AXIS_AZ))
        return false;
    if (!getStatus(AXIS_ALT))
        return false;

    if (!getEncoder(AXIS_AZ))
        return false;
    if (!getEncoder(AXIS_ALT))
        return false;

    // Calculate new RA DEC
    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    AltAz.azimuth = MicrostepsToDegrees(AXIS_AZ, EncoderNP[AXIS_AZ].getValue());
    AltAz.altitude = MicrostepsToDegrees(AXIS_ALT, EncoderNP[AXIS_ALT].getValue());
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld -> AZ/RA %lf°", EncoderNP[AXIS_AZ].getValue(),
           AltAz.azimuth);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld -> AZ/RA %lf°", EncoderNP[AXIS_ALT].getValue(),
           AltAz.altitude);

    m_MountCoordinates = AltAz;

    // Get equatorial coords.
    getCurrentRADE(m_MountCoordinates, m_SkyCurrentRADE);
    char RAStr[32], DecStr[32];
    fs_sexa(RAStr, m_SkyCurrentRADE.rightascension, 2, 3600);
    fs_sexa(DecStr, m_SkyCurrentRADE.declination, 2, 3600);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Sky RA %s DE %s", RAStr, DecStr);


    if (TrackState == SCOPE_SLEWING)
    {
        if (isSlewing() == false)
        {
            // Stages are GOTO --> SLEWING FAST --> APPRAOCH --> SLEWING SLOW --> TRACKING
            if (ScopeStatus == SLEWING_FAST)
            {
                ScopeStatus = APPROACH;
                return Goto(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination);
            }

            // If tracking was engaged, start it.
            if (ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s)
            {
                // Goto has finished start tracking
                TrackState = SCOPE_TRACKING;
                resetTracking();
                LOG_INFO("Tracking started.");
            }
            else
            {
                TrackState = SCOPE_IDLE;
            }
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewing() == false)
        {
            SetTrackEnabled(false);
            SetParked(true);
        }
    }

    NewRaDec(m_SkyCurrentRADE.rightascension, m_SkyCurrentRADE.declination);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Goto(double ra, double dec)
{
    if (ScopeStatus == APPROACH)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, m_SkyCurrentRADE.rightascension, 2, 3600);
        fs_sexa(DecStr, m_SkyCurrentRADE.declination, 2, 3600);
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Iterative GOTO RA %lf DEC %lf (Current Sky RA %s DE %s)", ra, dec, RAStr,
               DecStr);
    }
    else
    {
        if (TrackState != SCOPE_IDLE)
            Abort();

        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "GOTO RA %lf DEC %lf", ra, dec);

        if (IUFindSwitch(&CoordSP, "TRACK")->s == ISS_ON || IUFindSwitch(&CoordSP, "SLEW")->s == ISS_ON)
        {
            char RAStr[32], DecStr[32];
            fs_sexa(RAStr, ra, 2, 3600);
            fs_sexa(DecStr, dec, 2, 3600);
            m_SkyTrackingTarget.rightascension  = ra;
            m_SkyTrackingTarget.declination = dec;
            LOGF_INFO("Goto target RA %s DEC %s", RAStr, DecStr);
        }
    }

    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    TelescopeDirectionVector TDV;

    // Transform Celestial to Telescope coordinates.
    // We have no good way to estimate how long will the mount takes to reach target (with deceleration,
    // and not just speed). So we will use iterative GOTO once the first GOTO is complete.
    if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
    {
        INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };

        if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
        {
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
            INDI::HorizontalToEquatorial(&AltAz, &m_Location, ln_get_julian_from_sys(), &EquatorialCoordinates);
        }
        else
        {
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV, EquatorialCoordinates);
        }

        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, EquatorialCoordinates.rightascension, 2, 3600);
        fs_sexa(DecStr, EquatorialCoordinates.declination, 2, 3600);

        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Sky -> Mount RA %s DE %s (TDV x %lf y %lf z %lf)", RAStr, DecStr, TDV.x,
               TDV.y, TDV.z);
    }
    else
    {
        // Try a conversion with the stored observatory position if any
        INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
        EquatorialCoordinates.rightascension  = ra;
        EquatorialCoordinates.declination = dec;
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

    uint32_t axis1Steps = DegreesToMicrosteps(AXIS_AZ, AltAz.azimuth);
    uint32_t axis2Steps = DegreesToMicrosteps(AXIS_ALT, AltAz.altitude);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
           "Sky -> Mount AZ %lf° (%ld) AL %lf° (%ld)",
           AltAz.azimuth,
           axis1Steps,
           AltAz.altitude,
           axis2Steps);


    slewTo(AXIS_AZ, axis1Steps, ScopeStatus != APPROACH);
    slewTo(AXIS_ALT, axis2Steps, ScopeStatus != APPROACH);

    ScopeStatus = (ScopeStatus == Approach) ? SLEWING_SLOW : SLEWING_FAST;
    TrackState = SCOPE_SLEWING;
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Sync(double ra, double dec)
{
    // Compute a telescope direction vector from the current encoders
    if (!getEncoder(AXIS_AZ))
        return false;
    if (!getEncoder(AXIS_ALT))
        return false;

    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    AltAz.azimuth = MicrostepsToDegrees(AXIS_AZ, EncoderNP[AXIS_AZ].getValue());
    AltAz.altitude = MicrostepsToDegrees(AXIS_ALT, EncoderNP[AXIS_ALT].getValue());

    AlignmentDatabaseEntry NewEntry;
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension     = ra;
    NewEntry.Declination        = dec;
    if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    else
    {
        INDI::IEquatorialCoordinates EquatorialCoordinates {range24(AltAz.azimuth), AltAz.altitude};
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromEquatorialCoordinates(EquatorialCoordinates);
    }
    NewEntry.PrivateDataSize    = 0;

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New sync point Date %lf RA %lf DEC %lf TDV(x %lf y %lf z %lf)",
           NewEntry.ObservationJulianDate, NewEntry.RightAscension, NewEntry.Declination, NewEntry.TelescopeDirection.x,
           NewEntry.TelescopeDirection.y, NewEntry.TelescopeDirection.z);

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);

        // Tell the client about size change
        UpdateSize();

        // Tell the math plugin to reinitialise
        Initialise(this);

        // Force read before restarting
        ReadScopeStatus();

        // Sync cord wrap
        syncCoordWrapPosition();

        // The tracking seconds should be reset to restart the drift compensation
        resetTracking();

        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::getCurrentRADE(INDI::IHorizontalCoordinates altaz, INDI::IEquatorialCoordinates &rade)
{
    double RightAscension, Declination;

    if (MountTypeSP[ALTAZ].getState() == ISS_ON)
    {
        TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(altaz);
        if (!TransformTelescopeToCelestial(TDV, RightAscension, Declination))
        {
            TelescopeDirectionVector RotatedTDV(TDV);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    break;

                case NORTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated clockwise
                    RotatedTDV.RotateAroundY(90.0 - m_Location.latitude);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, altaz);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                    RotatedTDV.RotateAroundY(-90.0 - m_Location.latitude);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, altaz);
                    break;
            }

            INDI::IEquatorialCoordinates EquatorialCoordinates;
            INDI::HorizontalToEquatorial(&altaz, &m_Location, ln_get_julian_from_sys(), &EquatorialCoordinates);
            RightAscension = EquatorialCoordinates.rightascension;
            Declination = EquatorialCoordinates.declination;
        }
    }
    else
    {
        INDI::IEquatorialCoordinates EquatorialCoordinates;
        EquatorialCoordinates.rightascension = range24(altaz.azimuth);
        double homeDeclination = m_Location.latitude >= 0 ? 90 : -90;
        // Alt-Az mount in northern hemisphere at home position would have Alt = 0
        // On an Equatorial mount, this would be equal to Dec = +90
        // TODO must check if this is true assumption or not. Maybe it would be +90 on startup level.
        EquatorialCoordinates.declination = rangeDec(homeDeclination - altaz.altitude);
        TelescopeDirectionVector TDV = TelescopeDirectionVectorFromEquatorialCoordinates(EquatorialCoordinates);
        if (!TransformTelescopeToCelestial(TDV, RightAscension, Declination))
        {
            TelescopeDirectionVector RotatedTDV(TDV);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    break;

                case NORTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated clockwise
                    RotatedTDV.RotateAroundY(90.0 - m_Location.latitude);
                    EquatorialCoordinatesFromTelescopeDirectionVector(RotatedTDV, EquatorialCoordinates);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                    RotatedTDV.RotateAroundY(-90.0 - m_Location.latitude);
                    EquatorialCoordinatesFromTelescopeDirectionVector(RotatedTDV, EquatorialCoordinates);
                    break;
            }

            RightAscension = EquatorialCoordinates.rightascension;
            Declination = EquatorialCoordinates.declination;
        }
    }

    rade.rightascension = RightAscension;
    rade.declination = Declination;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::TimerHit()
{
    INDI::Telescope::TimerHit();

    switch (TrackState)
    {
        case SCOPE_SLEWING:
            break;

        case SCOPE_TRACKING:
        {
            // Check if manual motion in progress but we stopped
            if (m_ManualMotionActive && isSlewing() == false)
            {
                m_ManualMotionActive = false;
                // If we slewed manually using NSWE keys, then we need to restart tracking
                // whatever point we are AT now. We need to update the SkyTrackingTarget accordingly.
                m_SkyTrackingTarget.rightascension = EqN[AXIS_RA].value;
                m_SkyTrackingTarget.declination = EqN[AXIS_DE].value;
                resetTracking();
            }
            // If we're manually moving by WESN controls, update the tracking coordinates.
            if (m_ManualMotionActive)
            {
                break;
            }
            else
            {

                TelescopeDirectionVector TDV;
                INDI::IHorizontalCoordinates targetAltAz { 0, 0 };

                if (TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination,
                                                  0, TDV))
                {
                    if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
                        AltitudeAzimuthFromTelescopeDirectionVector(TDV, targetAltAz);
                    else
                    {
                        INDI::IEquatorialCoordinates EquatorialCoordinates {0, 0};
                        EquatorialCoordinatesFromTelescopeDirectionVector(TDV, EquatorialCoordinates);
                        targetAltAz.azimuth = EquatorialCoordinates.rightascension * 15;
                        targetAltAz.altitude = EquatorialCoordinates.declination;
                    }
                }
                else
                {
                    INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
                    EquatorialCoordinates.rightascension  = m_SkyTrackingTarget.rightascension;
                    EquatorialCoordinates.declination = m_SkyTrackingTarget.declination;
                    if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
                        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, ln_get_julian_from_sys(), &targetAltAz);
                    else
                    {
                        targetAltAz.azimuth = EquatorialCoordinates.rightascension * 15;
                        targetAltAz.altitude = EquatorialCoordinates.declination;
                    }
                }

                // Now add the guiding offsets.
                targetAltAz.azimuth += m_GuideOffset[AXIS_AZ];
                targetAltAz.altitude += m_GuideOffset[AXIS_ALT];

                // Next get current alt-az
                INDI::IHorizontalCoordinates currentAltAz { 0, 0 };
                currentAltAz.azimuth = MicrostepsToDegrees(AXIS_AZ, EncoderNP[AXIS_AZ].getValue());
                currentAltAz.altitude = MicrostepsToDegrees(AXIS_ALT, EncoderNP[AXIS_ALT].getValue());

                // Offset in arcsecs
                double azOffsetAngle = range360(targetAltAz.azimuth - currentAltAz.azimuth) * 3600;
                double alOffsetAngle = rangeDec(targetAltAz.altitude - currentAltAz.altitude) * 3600;

                if (std::abs(azOffsetAngle) > 0)
                    trackByRate(AXIS_AZ, azOffsetAngle);
                if (std::abs(alOffsetAngle) > 0)
                    trackByRate(AXIS_ALT, alOffsetAngle);

                break;
            }
            break;

            default:
                break;
            }
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

/////////////////////////////////////////////////////////////////////////////////////
/// Axis 1 is RA/AZ which has a range of 0 to 360 degrees (0 to 24 hours)
/// Axis 1 is DE/AL which has a range of -90 to +90 degrees.
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::MicrostepsToDegrees(INDI_HO_AXIS axis, uint32_t steps)
{
    double value = steps * DEGREES_PER_STEP;
    if (axis == AXIS_AZ)
        return range360(value);
    else
        return rangeDec(value);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
uint32_t CelestronAUX::DegreesToMicrosteps(INDI_HO_AXIS axis, double degrees)
{
    uint32_t value = 0;
    if (axis == AXIS_AZ)
    {
        value = degrees * STEPS_PER_DEGREE;
    }
    else
    {
        value = std::abs(degrees) * STEPS_PER_DEGREE;
        // We need to wrap around?
        if (degrees < 0)
            value += STEPS_PER_DEGREE / 2;
    }

    if (value > STEPS_PER_DEGREE)
        value -= STEPS_PER_DEGREE;

    return value;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::isSlewing()
{
    return m_AxisStatus[AXIS_AZ] == SLEWING || m_AxisStatus[AXIS_ALT] == SLEWING;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::slewTo(INDI_HO_AXIS axis, uint32_t steps, bool fast)
{
    // Stop first.
    trackByRate(AXIS_AZ, 0);
    trackByRate(AXIS_ALT, 0);

    AUXCommand command(fast ? MC_GOTO_FAST : MC_GOTO_SLOW, APP, axis == AXIS_AZ ? AZM : ALT);
    m_AxisStatus[axis] = SLEWING;
    command.setData(steps);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
};


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::slewByRate(INDI_HO_AXIS axis, int8_t rate)
{
    // Stop first.
    trackByRate(AXIS_AZ, 0);
    trackByRate(AXIS_ALT, 0);

    // TODO
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
bool CelestronAUX::setCordWrapPosition(uint32_t steps)
{
    AUXCommand command(MC_SET_CORDWRAP_POS, APP, AZM);
    command.setData(steps);
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
    return m_CordWrapPosition;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::stopAxis(INDI_HO_AXIS axis)
{
    m_AxisStatus[axis] = STOPPED;
    trackByRate(axis, 0);
    AUXBuffer b(1);
    b[0] = 0;
    AUXCommand command(MC_MOVE_POS, APP, (axis == AXIS_ALT) ? ALT : AZM, b);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;

}
/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Abort()
{
    stopAxis(AXIS_AZ);
    stopAxis(AXIS_ALT);
    TrackState = SCOPE_IDLE;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::trackByRate(INDI_HO_AXIS axis, int32_t rate)
{
    AUXCommand command(rate < 0 ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, axis == AXIS_AZ ? AZM : ALT);
    command.setData(std::abs(rate));
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::trackByMode(INDI_HO_AXIS axis, uint8_t mode)
{
    AUXCommand command(MC_SET_POS_GUIDERATE, APP, axis == AXIS_AZ ? AZM : ALT);
    switch (mode)
    {
        case TRACK_SOLAR:
            command.setData(AUX_SOLAR, 2);
            break;
        case TRACK_LUNAR:
            command.setData(AUX_LUNAR, 2);
            break;

        case TRACK_SIDEREAL:
        default:
            command.setData(AUX_SIDEREAL, 2);
            break;
    }

    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        TrackState = SCOPE_TRACKING;
        resetTracking();
        m_SkyTrackingTarget.rightascension = EqN[AXIS_RA].value;
        m_SkyTrackingTarget.declination = EqN[AXIS_DE].value;

        if (IUFindOnSwitchIndex(&TrackModeSP) == TRACK_CUSTOM)
            return SetTrackRate(TrackRateN[AXIS_AZ].value, TrackRateN[AXIS_ALT].value);
        else
            return SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
    }
    else
    {
        TrackState = SCOPE_IDLE;
        stopAxis(AXIS_AZ);
        stopAxis(AXIS_ALT);
    }

    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::SetTrackRate(double raRate, double deRate)
{
    m_TrackRates[AXIS_AZ] = raRate;
    m_TrackRates[AXIS_ALT] = deRate;

    if (TrackState == SCOPE_TRACKING)
    {
        // FIXME this is not going to work, need to figure out exact rate
        trackByRate(AXIS_AZ, raRate);
        trackByRate(AXIS_ALT, deRate);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::SetTrackMode(uint8_t mode)
{
    if (mode == TRACK_SIDEREAL)
        m_TrackRates[AXIS_AZ] = TRACKRATE_SIDEREAL;
    else if (mode == TRACK_SOLAR)
        m_TrackRates[AXIS_AZ] = TRACKRATE_SOLAR;
    else if (mode == TRACK_LUNAR)
        m_TrackRates[AXIS_AZ] = TRACKRATE_LUNAR;
    else if (mode == TRACK_CUSTOM)
    {
        m_TrackRates[AXIS_AZ] = TrackRateN[AXIS_RA].value;
        m_TrackRates[AXIS_ALT] = TrackRateN[AXIS_DE].value;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::getStatus(INDI_HO_AXIS axis)
{
    if (m_AxisStatus[axis] == SLEWING && ScopeStatus != SLEWING_MANUAL)
    {
        AUXCommand command(MC_SLEW_DONE, APP, axis == AXIS_AZ ? AZM : ALT);
        sendAUXCommand(command);
        readAUXResponse(command);
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::getEncoder(INDI_HO_AXIS axis)
{
    AUXCommand command(MC_GET_POSITION, APP, axis == AXIS_AZ ? AZM : ALT);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
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
    if (m.destination() != GPS)
        return;

    LOGF_DEBUG("GPS: Got 0x%02x", m.command());
    if (m_GPSEmulation == false)
        return;

    switch (m.command())
    {
        case GET_VER:
        {
            LOGF_DEBUG("GPS: GET_VER from 0x%02x", m.source());
            AUXBuffer dat(2);
            dat[0] = 0x01;
            dat[1] = 0x02;
            AUXCommand cmd(GET_VER, GPS, m.source(), dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_LAT:
        case GPS_GET_LONG:
        {
            LOGF_DEBUG("GPS: Sending LAT/LONG Lat:%f Lon:%f\n", LocationN[LOCATION_LATITUDE].value,
                       LocationN[LOCATION_LONGITUDE].value);
            AUXCommand cmd(m.command(), GPS, m.source());
            if (m.command() == GPS_GET_LAT)
                cmd.setData(LocationN[LOCATION_LATITUDE].value);
            else
                cmd.setData(LocationN[LOCATION_LONGITUDE].value);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_TIME:
        {
            LOGF_DEBUG("GPS: GET_TIME from 0x%02x", m.source());
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(3);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_hour);
            dat[1] = unsigned(ptm->tm_min);
            dat[2] = unsigned(ptm->tm_sec);
            AUXCommand cmd(GPS_GET_TIME, GPS, m.source(), dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_DATE:
        {
            LOGF_DEBUG("GPS: GET_DATE from 0x%02x", m.source());
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_mon + 1);
            dat[1] = unsigned(ptm->tm_mday);
            AUXCommand cmd(GPS_GET_DATE, GPS, m.source(), dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_GET_YEAR:
        {
            LOGF_DEBUG("GPS: GET_YEAR from 0x%02x", m.source());
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_year + 1900) >> 8;
            dat[1] = unsigned(ptm->tm_year + 1900) & 0xFF;
            LOGF_DEBUG("GPS: Sending: %d [%d,%d]", ptm->tm_year, dat[0], dat[1]);
            AUXCommand cmd(GPS_GET_YEAR, GPS, m.source(), dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        case GPS_LINKED:
        {
            LOGF_DEBUG("GPS: LINKED from 0x%02x", m.source());
            AUXBuffer dat(1);

            dat[0] = unsigned(1);
            AUXCommand cmd(GPS_LINKED, GPS, m.source(), dat);
            sendAUXCommand(cmd);
            //readAUXResponse(cmd);
            break;
        }
        default:
            LOGF_DEBUG("GPS: Got 0x%02x", m.command());
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::processResponse(AUXCommand &m)
{
    m.logResponse();

    if (m.destination() == GPS)
        emulateGPS(m);
    else if (m.destination() == APP)
        switch (m.command())
        {
            case MC_GET_POSITION:
                switch (m.source())
                {
                    case ALT:
                        // The Alt encoder value is signed!
                        EncoderNP[AXIS_ALT].setValue(m.getData());
                        break;
                    case AZM:
                        EncoderNP[AXIS_AZ].setValue(m.getData());
                        break;
                    default:
                        break;
                }
                break;
            case MC_SLEW_DONE:
                switch (m.source())
                {
                    case ALT:
                        m_AxisStatus[AXIS_ALT] = (m.getData() == 0xff) ? STOPPED : SLEWING;
                        break;
                    case AZM:
                        m_AxisStatus[AXIS_AZ] = (m.getData() == 0xff) ? STOPPED : SLEWING;
                        break;
                    default:
                        break;
                }
                break;
            case MC_POLL_CORDWRAP:
                if (m.source() == AZM)
                    m_CordWrapActive = m.getData() == 0xff;
                break;
            case MC_GET_CORDWRAP_POS:
                if (m.source() == AZM)
                    m_CordWrapPosition = m.getData();
                break;

            case MC_GET_AUTOGUIDE_RATE:
                switch (m.source())
                {
                    case ALT:
                        GuideRateNP[AXIS_ALT].setValue(m.getData() * 100.0 / 255);
                        break;
                    case AZM:
                        GuideRateNP[AXIS_AZ].setValue(m.getData() * 100.0 / 255);
                        break;
                    default:
                        break;
                }
                break;

            case MC_SET_AUTOGUIDE_RATE:
                // Nothing to do.
                break;

            case MC_AUX_GUIDE:
                // Nothing to do
                break;

            case MC_AUX_GUIDE_ACTIVE:
                switch (m.source())
                {
                    case ALT:
                        if (m.getData() == 0x00)
                            GuideComplete(AXIS_DE);
                        break;
                    case AZM:
                        if (m.getData() == 0x00)
                            GuideComplete(AXIS_RA);
                        break;
                    default:
                        break;
                }
                break;

            case GET_VER:
                uint8_t *verBuf;

                verBuf = 0;
                switch (m.source())
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
                        break;
                    case GPS:
                        verBuf = m_GPSVersion;
                        break;
                    case APP:
                        LOGF_DEBUG("Got echo of GET_VERSION from %s", m.moduleName(m.destination()));
                        break;
                    default:
                        break;
                }
                if (verBuf != 0)
                {
                    for (int i = 0; i < 4; i++)
                        verBuf[i] = m.data()[i];
                }
                break;
            default:
                break;

        }
    else
    {
        DEBUGF(DBG_CAUX, "Got msg not for me (%s). Ignoring.", m.moduleName(m.destination()));
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
            if (aux_tty_read((char*)buf, 1, READ_TIMEOUT, &n) != TTY_OK)
                return false;
        }
        while (buf[0] != 0x3b);

        // packet preamble is found, now read packet length.
        if (aux_tty_read((char*)(buf + 1), 1, READ_TIMEOUT, &n) != TTY_OK)
            return false;

        // now packet length is known, read the rest of the packet.
        if (aux_tty_read((char*)(buf + 2), buf[1] + 1, READ_TIMEOUT, &n)
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
        buf[2] = c.destination();
        buf[3] = c.source();
        buf[4] = c.command();

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
int CelestronAUX::sendBuffer(AUXBuffer buf)
{
    if ( PortFD > 0 )
    {
        int n;

        if (aux_tty_write((char*)buf.data(), buf.size(), CTS_TIMEOUT, &n) != TTY_OK)
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
bool CelestronAUX::sendAUXCommand(AUXCommand &command)
{
    AUXBuffer buf;
    command.logCommand();

    if (m_IsRTSCTS || !m_isHandController || getActiveConnection() != serialConnection)
        // Direct connection (AUX/PC/USB port)
        command.fillBuf(buf);
    else
    {
        // connection is through HC serial and destination is not HC,
        // convert AUX command to a passthrough command

        // fixed len = 8
        buf.resize(8);
        // prefix
        buf[0] = 0x50;
        // length
        buf[1] = 1 + command.dataSize();
        // destination
        buf[2] = command.destination();
        buf[3] = command.command();                // command id
        for (size_t i = 0; i < command.dataSize(); i++) // payload
        {
            buf[i + 4] = command.data()[i];
        }
        buf[7] = response_data_size = command.responseDataSize();
    }

    tcflush(PortFD, TCIOFLUSH);
    return (sendBuffer(buf) == static_cast<int>(buf.size()));
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
    if (ioctl(PortFD, TIOCMGET, &m_ModemControl) == -1)
        LOGF_ERROR("Error getting handshake lines %s(%d).", strerror(errno), errno);
    if (rts)
        m_ModemControl |= TIOCM_RTS;
    else
        m_ModemControl &= ~TIOCM_RTS;
    if (ioctl(PortFD, TIOCMSET, &m_ModemControl) == -1)
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
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(step)));
        if (ioctl(PortFD, TIOCMGET, &m_ModemControl) == -1)
        {
            LOGF_ERROR("Error getting handshake lines %s(%d).", strerror(errno), errno);
            return 0;
        }
        if (m_ModemControl & TIOCM_CTS)
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
    if (sendBuffer(b) != (int)b.size())
        return false;

    // read response
    int n;
    if (aux_tty_read((char*)buf, 3, READ_TIMEOUT, &n) != TTY_OK)
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
int CelestronAUX::aux_tty_read(char *buf, int bufsiz, int timeout, int *n)
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
int CelestronAUX::aux_tty_write(char *buf, int bufsiz, float timeout, int *n)
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
bool CelestronAUX::tty_set_speed(speed_t speed)
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

