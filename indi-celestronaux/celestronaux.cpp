/*
    Celestron Aux Mount Driver.

    Copyright (C) 2020 Paweł T. Jochym
    Copyright (C) 2020 Fabrizio Pollastri
    Copyright (C) 2020-2022 Jasem Mutlaq

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

    JM 2022.07.07: Added Wedge support.
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

#define DEBUG_PID

using namespace INDI::AlignmentSubsystem;

static std::unique_ptr<CelestronAUX> telescope_caux(new CelestronAUX());

double anglediff(double a, double b)
{
    // Signed angle difference
    double d;
    b = fmod(b, 360.0);
    a = fmod(a, 360.0);
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
            if (PortTypeSP[PORT_AUX_PC].getState() == ISS_ON)
            {
                serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
                if (!tty_set_speed(B19200))
                    return false;
                m_IsRTSCTS = detectRTSCTS();
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
                if ((m_isHandController = detectHC(version, 10)))
                    LOGF_INFO("Detected Hand Controller (v%s) serial connection.", version);
                else
                    LOG_INFO("Detected Mount USB serial connection.");
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

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
        //SetApproximateMountAlignmentFromMountType(static_cast<MountType>(MountTypeSP.findOnSwitchIndex()));
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

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void CelestronAUX::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);
    defineProperty(&PortTypeSP);
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
    int configMountType = ALTAZ;
    IUGetConfigOnSwitchIndex(getDeviceName(), "MOUNT_TYPE", &configMountType);

    // Detect Equatorial Mounts
    if (strstr(getDeviceName(), "CGX") ||
            strstr(getDeviceName(), "CGEM") ||
            strstr(getDeviceName(), "Advanced VX") ||
            strstr(getDeviceName(), "Wedge"))
    {
        // Force equatorial for such mounts
        configMountType = EQUATORIAL;
        m_IsWedge = (strstr(getDeviceName(), "Wedge") != nullptr);
    }

    if (configMountType == EQUATORIAL)
        SetApproximateMountAlignment(m_Location.latitude >= 0 ? NORTH_CELESTIAL_POLE : SOUTH_CELESTIAL_POLE);
    else
        SetApproximateMountAlignment(ZENITH);

    MountTypeSP[EQUATORIAL].fill("EQUATORIAL", "Equatorial", configMountType == EQUATORIAL ? ISS_ON : ISS_OFF);
    MountTypeSP[ALTAZ].fill("ALTAZ", "AltAz", configMountType == ALTAZ ? ISS_ON : ISS_OFF);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Track Modes for Equatorial Mount
    if (MountTypeSP[EQUATORIAL].getState() == ISS_ON)
    {
        AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
        AddTrackMode("TRACK_SOLAR", "Solar");
        AddTrackMode("TRACK_LUNAR", "Lunar");
        AddTrackMode("TRACK_CUSTOM", "Custom");

        SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE, 8);
    }

    // Horizontal Coords
    HorizontalCoordsNP[AXIS_AZ].fill("AZ", "Az D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    HorizontalCoordsNP[AXIS_ALT].fill("ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
    HorizontalCoordsNP.fill(getDeviceName(), "HORIZONTAL_COORD", "Horizontal Coord", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Homing
    HomeSP[HOME_AXIS1].fill("AXIS1", "AZ/RA", ISS_OFF);
    HomeSP[HOME_AXIS2].fill("AXIS2", "AL/DE", ISS_OFF);
    HomeSP[HOME_ALL].fill("ALL", "All", ISS_OFF);
    HomeSP.fill(getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Cord Wrap Tab
    /////////////////////////////////////////////////////////////////////////////////////

    // Cord wrap Toggle
    CordWrapToggleSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    CordWrapToggleSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
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
    GuideRateNP[AXIS_AZ].fill("GUIDE_RATE_WE", "W/E Rate", "%.f", 0, 1, .1, 0.5);
    GuideRateNP[AXIS_ALT].fill("GUIDE_RATE_NS", "N/S Rate", "%.f", 0, 1, .1, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    /////////////////////////////////////////////////////////////////////////////////////
    /// Connection
    /////////////////////////////////////////////////////////////////////////////////////
    IUGetConfigOnSwitchIndex(getDeviceName(), "PORT_TYPE", &m_ConfigPortType);
    PortTypeSP[PORT_AUX_PC].fill("PORT_AUX_PC", "AUX/PC", m_ConfigPortType == PORT_AUX_PC ? ISS_ON : ISS_OFF);
    PortTypeSP[PORT_HC_USB].fill("PORT_HC_USB", "USB/HC", m_ConfigPortType == PORT_AUX_PC ? ISS_OFF : ISS_ON);
    PortTypeSP.fill(getDeviceName(), "PORT_TYPE", "Port Type", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
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

    // PID Control
    Axis1PIDNP[Propotional].fill("Propotional", "Propotional", "%.2f", 0, 500, 10, GAIN_STEPS);
    Axis1PIDNP[Derivative].fill("Derivative", "Derivative", "%.2f", 0, 500, 10, 0);
    Axis1PIDNP[Integral].fill("Integral", "Integral", "%.2f", 0, 500, 10, 0);
    Axis1PIDNP.fill(getDeviceName(), "AXIS1_PID", "Axis1 PID", MOUNTINFO_TAB, IP_RW, 60, IPS_IDLE);

    Axis2PIDNP[Propotional].fill("Propotional", "Propotional", "%.2f", 0, 500, 10, GAIN_STEPS);
    Axis2PIDNP[Derivative].fill("Derivative", "Derivative", "%.2f", 0, 100, 10, 0);
    Axis2PIDNP[Integral].fill("Integral", "Integral", "%.2f", 0, 100, 10, 1);
    Axis2PIDNP.fill(getDeviceName(), "AXIS2_PID", "Axis2 PID", MOUNTINFO_TAB, IP_RW, 60, IPS_IDLE);

    // Firmware Info
    FirmwareTP[FW_HC].fill("HC version", "", nullptr);
    FirmwareTP[FW_MB].fill("Mother Board version", "", nullptr);
    FirmwareTP[FW_AZM].fill("Ra/AZM version", "", nullptr);
    FirmwareTP[FW_ALT].fill("Dec/ALT version", "", nullptr);
    FirmwareTP[FW_WiFi].fill("WiFi version", "", nullptr);
    FirmwareTP[FW_BAT].fill("Battery version", "", nullptr);
    FirmwareTP[FW_GPS].fill("GPS version", "", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware Info", "Firmware Info", MOUNTINFO_TAB, IP_RO, 0, IPS_IDLE);

    // Gain Rate
    //    GainNP[AXIS_AZ].fill("AXIS_AZ", "Axis1", "%.f", -10000, 10000, 500, 0);
    //    GainNP[AXIS_ALT].fill("AXIS_ALT", "Axis2", "%.f", -10000, 10000, 500, 0);
    //    GainNP.fill(getDeviceName(), "TRACK_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
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

    SetParkDataType(PARK_AZ_ALT_ENCODER);

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
        //defineProperty(&GainNP);
        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
            defineProperty(&HorizontalCoordsNP);
        defineProperty(&HomeSP);

        // Guide
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);

        // Cord wrap Enabled?
        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
        {
            getCordWrapEnabled();
            CordWrapToggleSP[INDI_ENABLED].s   = m_CordWrapActive ? ISS_ON : ISS_OFF;
            CordWrapToggleSP[INDI_DISABLED].s  = m_CordWrapActive ? ISS_OFF : ISS_ON;
            defineProperty(&CordWrapToggleSP);

            // Cord wrap Position?
            getCordWrapPosition();
            double cordWrapAngle = range360(m_CordWrapPosition / STEPS_PER_DEGREE);
            LOGF_INFO("Cord Wrap position angle %.2f", cordWrapAngle);
            CordWrapPositionSP[static_cast<int>(std::floor(cordWrapAngle / 90))].s = ISS_ON;
            defineProperty(&CordWrapPositionSP);
            defineProperty(&CordWrapBaseSP);
        }

        defineProperty(&GPSEmuSP);

        // Encoders
        defineProperty(&EncoderNP);
        defineProperty(&AngleNP);
        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
        {
            defineProperty(&Axis1PIDNP);
            defineProperty(&Axis2PIDNP);
        }

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

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(0);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(0);
            SetAxis2Park(0);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(0);
        }
    }
    else
    {
        deleteProperty(MountTypeSP.getName());
        //deleteProperty(GainNP.getName());
        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
            deleteProperty(HorizontalCoordsNP.getName());
        deleteProperty(HomeSP.getName());

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.getName());

        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
        {
            deleteProperty(CordWrapToggleSP.getName());
            deleteProperty(CordWrapPositionSP.getName());
            deleteProperty(CordWrapBaseSP.getName());
        }

        deleteProperty(GPSEmuSP.getName());

        deleteProperty(EncoderNP.getName());
        deleteProperty(AngleNP.getName());

        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
        {
            deleteProperty(Axis1PIDNP.getName());
            deleteProperty(Axis2PIDNP.getName());
        }

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
    IUSaveConfigSwitch(fp, &PortTypeSP);
    IUSaveConfigSwitch(fp, &CordWrapToggleSP);
    IUSaveConfigSwitch(fp, &CordWrapPositionSP);
    IUSaveConfigSwitch(fp, &CordWrapBaseSP);
    IUSaveConfigSwitch(fp, &GPSEmuSP);

    if (MountTypeSP[ALTAZ].getState() == ISS_ON)
    {
        IUSaveConfigNumber(fp, &Axis1PIDNP);
        IUSaveConfigNumber(fp, &Axis2PIDNP);
    }

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
        // Gain
        //        if (GainNP.isNameMatch(name))
        //        {
        //            GainNP.update(values, names, n);
        //            GainNP.setState(IPS_OK);
        //            GainNP.apply();
        //            return true;
        //        }

        // Axis1 PID
        if (Axis1PIDNP.isNameMatch(name))
        {
            Axis1PIDNP.update(values, names, n);
            Axis1PIDNP.setState(IPS_OK);
            Axis1PIDNP.apply();
            saveConfig(true, Axis1PIDNP.getName());
            return true;
        }

        // Axis2 PID
        if (Axis2PIDNP.isNameMatch(name))
        {
            Axis2PIDNP.update(values, names, n);
            Axis2PIDNP.setState(IPS_OK);
            Axis2PIDNP.apply();
            saveConfig(true, Axis2PIDNP.getName());
            return true;
        }

        // Horizontal Coords
        if (HorizontalCoordsNP.isNameMatch(name))
        {
            double az = 0, alt = 0;
            for (int i = 0; i < 2; i++)
            {
                if (HorizontalCoordsNP[AXIS_AZ].isNameMatch(names[i]))
                    az = values[i];
                else if (HorizontalCoordsNP[AXIS_ALT].isNameMatch(names[i]))
                    alt = values[i];
            }

            // Convert Celestial Alt-Az to Equatorial
            INDI::IHorizontalCoordinates celestialAzAlt = {az, alt};
            INDI::IEquatorialCoordinates celestialEquatorial;
            INDI::HorizontalToEquatorial(&celestialAzAlt, &m_Location, ln_get_julian_from_sys(), &celestialEquatorial);

            if (Goto(celestialEquatorial.rightascension, celestialEquatorial.declination))
            {
                // State already set in Goto so no need to set state here
                char azStr[32], alStr[32];
                fs_sexa(azStr, celestialAzAlt.azimuth, 2, 3600);
                fs_sexa(alStr, celestialAzAlt.altitude, 2, 3600);
                LOGF_INFO("Slewing to AZ: %s AL: %s", azStr, alStr);
            }
            else
            {
                HorizontalCoordsNP.setState(IPS_ALERT);
                HorizontalCoordsNP.apply();
            }

            return true;
        }

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

            slewTo(AXIS_AZ, axisSteps1);
            slewTo(AXIS_ALT, axisSteps2);
            TrackState = SCOPE_SLEWING;
            EncoderNP.setState(IPS_OK);
            EncoderNP.apply();
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

            // Get target type
            MountType targetMountType = IUFindOnSwitchIndex(&MountTypeSP) ? ALTAZ : EQUATORIAL;

            // If different then update
            if (currentMountType != targetMountType)
            {
                LOG_INFO("Mount type updated. You must restart the driver for changes to take effect.");
                saveConfig(true, MountTypeSP.getName());
            }

            return true;
        }

        // Port Type
        if (PortTypeSP.isNameMatch(name))
        {
            PortTypeSP.update(states, names, n);
            PortTypeSP.setState(IPS_OK);
            PortTypeSP.apply();
            if (m_ConfigPortType != IUFindOnSwitchIndex(&PortTypeSP))
            {
                m_ConfigPortType = IUFindOnSwitchIndex(&PortTypeSP);
                saveConfig(true, PortTypeSP.getName());
            }
            return true;
        }

        // Cord Wrap Toggle
        if (CordWrapToggleSP.isNameMatch(name))
        {
            CordWrapToggleSP.update(states, names, n);
            const bool toEnable = CordWrapToggleSP[INDI_ENABLED].s == ISS_ON;
            LOGF_INFO("Cord Wrap is %s.", toEnable ? "enabled" : "disabled");
            setCordWrapEnabled(toEnable);
            getCordWrapEnabled();
            CordWrapToggleSP.setState(IPS_OK);
            CordWrapToggleSP.apply();

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

        // Homing/Leveling
        if (HomeSP.isNameMatch(name))
        {
            HomeSP.update(states, names, n);
            bool rc = false;
            switch (HomeSP.findOnSwitchIndex())
            {
                case HOME_AXIS1:
                    rc = goHome(AXIS_AZ);
                    break;
                case HOME_AXIS2:
                    rc = goHome(AXIS_ALT);
                    break;
                case HOME_ALL:
                    rc = goHome(AXIS_AZ) && goHome(AXIS_ALT);
                    break;
            }

            if (rc)
            {
                HomeSP.setState(IPS_BUSY);
                LOG_INFO("Homing in progress...");
            }
            else
            {
                HomeSP.reset();
                HomeSP.setState(IPS_ALERT);
                LOG_ERROR("Failed to start homing.");
            }

            HomeSP.apply();
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
    slewTo(AXIS_AZ, GetAxis1Park());
    slewTo(AXIS_ALT, GetAxis2Park());
    TrackState = SCOPE_PARKING;
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
        northAz = DegreesToAzimuth(AltAzFromRaDec(get_local_sidereal_time(m_Location.longitude), 0., 0.).azimuth);
    LOGF_DEBUG("North Azimuth = %lf", northAz);
    return northAz;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::syncCoordWrapPosition()
{
    // No coord wrap for equatorial mounts.
    if (MountTypeSP[EQUATORIAL].getState() == ISS_ON)
        return;

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
    if (isNorthHemisphere())
        m_AxisDirection[AXIS_AZ] = (dir == DIRECTION_WEST) ? FORWARD : REVERSE;
    else
        m_AxisDirection[AXIS_AZ] = (dir == DIRECTION_WEST) ? REVERSE : FORWARD;
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
    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_ALT].getValue() * 100);
    guidePulse(AXIS_DE, ms, rate);
    return IPS_BUSY;
}

IPState CelestronAUX::GuideSouth(uint32_t ms)
{
    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_ALT].getValue() * 100);
    guidePulse(AXIS_DE, ms, -rate);
    return IPS_BUSY;
}

IPState CelestronAUX::GuideEast(uint32_t ms)
{
    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_AZ].getValue() * 100);
    guidePulse(AXIS_RA, ms, -rate);
    return IPS_BUSY;
}

IPState CelestronAUX::GuideWest(uint32_t ms)
{
    int8_t rate = static_cast<int8_t>(GuideRateNP[AXIS_AZ].getValue() * 100);
    guidePulse(AXIS_RA, ms, rate);
    return IPS_BUSY;
}

bool CelestronAUX::guidePulse(INDI_EQ_AXIS axis, uint32_t ms, int8_t rate)
{
    // For Equatorial mounts, use regular guiding.
    if (MountTypeSP[EQUATORIAL].getState() == ISS_ON)
    {
        uint8_t ticks = std::min(255u, ms / 10);
        AUXBuffer data(2);
        data[0] = rate;
        data[1] = ticks;
        AUXCommand cmd(MC_AUX_GUIDE, APP, axis == AXIS_DE ? ALT : AZM, data);
        return sendAUXCommand(cmd);
    }
    // For Alt-Az mounts in tracking state, add to guide delta
    else if (TrackState == SCOPE_TRACKING)
    {
        double arcsecs = TRACKRATE_SIDEREAL * ms / 1000.0 * rate / 100.;
        double steps = arcsecs * STEPS_PER_ARCSEC;
        m_GuideOffset[axis] += steps;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::resetTracking()
{
    //    getEncoder(AXIS_AZ);
    //    getEncoder(AXIS_ALT);
    //    m_TrackStartSteps[AXIS_AZ] = EncoderNP[AXIS_AZ].getValue();
    //    m_TrackStartSteps[AXIS_ALT] = EncoderNP[AXIS_ALT].getValue();

    m_Controllers[AXIS_AZ].reset(new PID(1, 100000, -100000, Axis1PIDNP[Propotional].getValue(),
                                         Axis1PIDNP[Derivative].getValue(), Axis1PIDNP[Integral].getValue()));
    m_Controllers[AXIS_AZ]->setIntegratorLimits(-2000, 2000);
    m_Controllers[AXIS_ALT].reset(new PID(1, 100000, -100000, Axis2PIDNP[Propotional].getValue(),
                                          Axis2PIDNP[Derivative].getValue(), Axis2PIDNP[Integral].getValue()));
    m_Controllers[AXIS_ALT]->setIntegratorLimits(-2000, 2000);
    m_TrackingElapsedTimer.restart();
    m_GuideOffset[AXIS_AZ] = m_GuideOffset[AXIS_ALT] = 0;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::isTrackingRequested()
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

    double axis1 = EncoderNP[AXIS_AZ].getValue();
    double axis2 = EncoderNP[AXIS_ALT].getValue();

    if (!getEncoder(AXIS_AZ))
    {
        if (EncoderNP.getState() != IPS_ALERT)
        {
            EncoderNP.setState(IPS_ALERT);
            AngleNP.setState(IPS_ALERT);
            EncoderNP.apply();
            AngleNP.apply();
        }
        return false;
    }
    if (!getEncoder(AXIS_ALT))
    {
        if (EncoderNP.getState() != IPS_ALERT)
        {
            EncoderNP.setState(IPS_ALERT);
            AngleNP.setState(IPS_ALERT);
            EncoderNP.apply();
            AngleNP.apply();
        }
        return false;
    }

    // Mount Alt-Az Coords
    if (MountTypeSP[ALTAZ].getState() == ISS_ON)
    {
        EncodersToAltAz(m_MountCurrentAltAz);
    }
    // Mount RA/DE Coords.
    else
    {
        TelescopePierSide pierSide = currentPierSide;
        EncodersToRADE(m_MountCurrentRADE, pierSide);
        setPierSide(pierSide);
    }

    // Send to client if updated
    if (std::abs(axis1 - EncoderNP[AXIS_AZ].getValue()) > 1 || std::abs(axis2 - EncoderNP[AXIS_ALT].getValue()) > 1)
    {
        EncoderNP.setState(IPS_OK);
        EncoderNP.apply();
        if (MountTypeSP[ALTAZ].getState() == ISS_ON)
        {
            AngleNP[AXIS_AZ].setValue(m_MountCurrentAltAz.azimuth);
            AngleNP[AXIS_ALT].setValue(m_MountCurrentAltAz.altitude);
        }
        else
        {
            AngleNP[AXIS_RA].setValue(range360(range24(get_local_sidereal_time(m_Location.longitude) -
                                               m_MountCurrentRADE.rightascension) * 15));
            AngleNP[AXIS_DE].setValue(rangeDec(m_MountCurrentRADE.declination));
        }
        AngleNP.setState(IPS_OK);
        AngleNP.apply();
    }

    // Get sky coordinates
    mountToSkyCoords();
    char RAStr[32] = {0}, DecStr[32] = {0};
    fs_sexa(RAStr, m_SkyCurrentRADE.rightascension, 2, 3600);
    fs_sexa(DecStr, m_SkyCurrentRADE.declination, 2, 3600);

    INDI::IHorizontalCoordinates celestialAzAlt;
    INDI::EquatorialToHorizontal(&m_SkyCurrentRADE, &m_Location, ln_get_julian_from_sys(), &celestialAzAlt);

    char AzStr[32] = {0}, AltStr[32] = {0}, HAStr[32] = {0};
    fs_sexa(AzStr, celestialAzAlt.azimuth, 2, 3600);
    fs_sexa(AltStr, celestialAzAlt.altitude, 2, 3600);
    double HA = rangeHA(ln_get_julian_from_sys() - m_SkyCurrentRADE.rightascension);
    fs_sexa(HAStr, HA, 2, 3600);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Mount -> Sky RA: %s DE: %s AZ: %s AL: %s HA: %s Pier: %s",
           RAStr,
           DecStr,
           AzStr,
           AltStr,
           HAStr,
           MountTypeSP[ALTAZ].getState() == ISS_ON ? "NA" : getPierSideStr(currentPierSide));

    if (std::abs(celestialAzAlt.azimuth - HorizontalCoordsNP[AXIS_AZ].getValue()) > 0.1 ||
            std::abs(celestialAzAlt.altitude - HorizontalCoordsNP[AXIS_ALT].getValue()) > 0.1)
    {
        HorizontalCoordsNP[AXIS_AZ].setValue(celestialAzAlt.azimuth);
        HorizontalCoordsNP[AXIS_ALT].setValue(celestialAzAlt.altitude);
        HorizontalCoordsNP.apply();
    }

    if (TrackState == SCOPE_SLEWING)
    {
        if (isSlewing() == false)
        {
            // Stages are GOTO --> SLEWING FAST --> APPRAOCH --> SLEWING SLOW --> TRACKING
            if (ScopeStatus == SLEWING_FAST)
            {
                ScopeStatus = APPROACH;
                return Goto(m_SkyGOTOTarget.rightascension, m_SkyGOTOTarget.declination);
            }

            // If tracking was engaged, start it.
            if (isTrackingRequested())
            {
                // Goto has finished start tracking
                ScopeStatus = TRACKING;
                if (HorizontalCoordsNP.getState() == IPS_BUSY)
                {
                    HorizontalCoordsNP.setState(IPS_OK);
                    HorizontalCoordsNP.apply();
                }
                TrackState = SCOPE_TRACKING;
                resetTracking();

                // For equatorial mounts, engage tracking.
                if (MountTypeSP[EQUATORIAL].getState() == ISS_ON)
                    SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
                LOG_INFO("Tracking started.");
            }
            else
            {
                TrackState = SCOPE_IDLE;
                ScopeStatus = IDLE;
            }
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewing() == false)
        {
            SetParked(true);
            SetTrackEnabled(false);
        }
    }

    NewRaDec(m_SkyCurrentRADE.rightascension, m_SkyCurrentRADE.declination);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::EncodersToAltAz(INDI::IHorizontalCoordinates &coords)
{
    coords.azimuth = DegreesToAzimuth(EncodersToDegrees(EncoderNP[AXIS_AZ].getValue()));
    coords.altitude = EncodersToDegrees(EncoderNP[AXIS_ALT].getValue());
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %10.f -> AZ %.4f°", EncoderNP[AXIS_AZ].getValue(),
           coords.azimuth);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %10.f -> AL %.4f°", EncoderNP[AXIS_ALT].getValue(),
           coords.altitude);
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Goto(double ra, double dec)
{
    char RAStr[32] = {0}, DecStr[32] = {0};
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);

    // In case we have appaoch ongoing
    if (ScopeStatus == APPROACH)
    {
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Slow Iterative GOTO RA %s DE %s", RAStr, DecStr);
    }
    // Fast GOTO
    else
    {
        if (TrackState != SCOPE_IDLE)
            Abort();

        m_GuideOffset[AXIS_AZ] = m_GuideOffset[AXIS_ALT] = 0;
        m_SkyGOTOTarget.rightascension = ra;
        m_SkyGOTOTarget.declination = dec;

        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Fast GOTO RA %s DE %s", RAStr, DecStr);

        if (isTrackingRequested())
        {
            m_SkyTrackingTarget = m_SkyGOTOTarget;
        }
    }

    uint32_t axis1Steps {0}, axis2Steps {0};

    TelescopeDirectionVector TDV;
    INDI::IEquatorialCoordinates MountRADE { ra, dec };

    // Transform Celestial to Telescope coordinates.
    // We have no good way to estimate how long will the mount takes to reach target (with deceleration,
    // and not just speed). So we will use iterative GOTO once the first GOTO is complete.
    if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
    {
        // For Alt-Az Mounts, we get the Mount AltAz coords
        if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
        {
            INDI::IHorizontalCoordinates MountAltAz { 0, 0 };
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, MountAltAz);
            // Converts to steps and we're done.
            axis1Steps = DegreesToEncoders(AzimuthToDegrees(MountAltAz.azimuth));
            axis2Steps = DegreesToEncoders(MountAltAz.altitude);

            // For logging purposes
            INDI::HorizontalToEquatorial(&MountAltAz, &m_Location, ln_get_julian_from_sys(), &MountRADE);
        }
        // For Equatorial mounts
        else
        {
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV, MountRADE);
            // Converts to steps and we're done.
            RADEToEncoders(MountRADE, axis1Steps, axis2Steps);
        }
    }
    // Conversion failed, use values as is
    else
    {
        if (MountTypeSP[EQUATORIAL].getState() == ISS_ON)
        {
            RADEToEncoders(MountRADE, axis1Steps, axis2Steps);
        }
        else
        {
            INDI::IHorizontalCoordinates MountAltAz { 0, 0 };
            INDI::EquatorialToHorizontal(&MountRADE, &m_Location, ln_get_julian_from_sys(), &MountAltAz);
            TDV = TelescopeDirectionVectorFromAltitudeAzimuth(MountAltAz);
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
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, MountAltAz);

            // Converts to steps and we're done.
            axis1Steps = DegreesToEncoders(AzimuthToDegrees(MountAltAz.azimuth));
            axis2Steps = DegreesToEncoders(MountAltAz.altitude);
        }
    }

    fs_sexa(RAStr, MountRADE.rightascension, 2, 3600);
    fs_sexa(DecStr, MountRADE.declination, 2, 3600);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Sky -> Mount RA %s (%ld) DE %s (%ld)", RAStr, axis1Steps, DecStr,
           axis2Steps);

    // Slew to physical steps.
    slewTo(AXIS_AZ, axis1Steps, ScopeStatus != APPROACH);
    slewTo(AXIS_ALT, axis2Steps, ScopeStatus != APPROACH);

    ScopeStatus = (ScopeStatus == APPROACH) ? SLEWING_SLOW : SLEWING_FAST;
    TrackState = SCOPE_SLEWING;

    if (MountTypeSP[ALTAZ].getState() == ISS_ON && HorizontalCoordsNP.getState() != IPS_BUSY)
    {
        HorizontalCoordsNP.setState(IPS_BUSY);
        HorizontalCoordsNP.apply();

    }
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


    AlignmentDatabaseEntry NewEntry;
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension     = ra;
    NewEntry.Declination        = dec;


    if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
    {
        INDI::IHorizontalCoordinates MountAltAz { 0, 0 };
        MountAltAz.azimuth = DegreesToAzimuth(EncodersToDegrees(EncoderNP[AXIS_AZ].getValue()));
        MountAltAz.altitude = EncodersToDegrees(EncoderNP[AXIS_ALT].getValue());
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(MountAltAz);
    }
    else
    {
        INDI::IEquatorialCoordinates MountRADE {0, 0};
        TelescopePierSide pierSide;
        EncodersToRADE(MountRADE, pierSide);
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromEquatorialCoordinates(MountRADE);
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
bool CelestronAUX::mountToSkyCoords()
{
    double RightAscension, Declination;

    // TODO for Alt-Az Mounts on a Wedge, we need a watch to set this.
    if (MountTypeSP[ALTAZ].getState() == ISS_ON)
    {
        INDI::IHorizontalCoordinates AltAz = m_MountCurrentAltAz;
        TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
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
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                    RotatedTDV.RotateAroundY(-90.0 - m_Location.latitude);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;
            }

            INDI::IEquatorialCoordinates EquatorialCoordinates;
            INDI::HorizontalToEquatorial(&AltAz, &m_Location, ln_get_julian_from_sys(), &EquatorialCoordinates);
            RightAscension = EquatorialCoordinates.rightascension;
            Declination = EquatorialCoordinates.declination;
        }
    }
    else
    {
        INDI::IEquatorialCoordinates EquatorialCoordinates = m_MountCurrentRADE;
        TelescopeDirectionVector TDV = TelescopeDirectionVectorFromEquatorialCoordinates(EquatorialCoordinates);
        if (!TransformTelescopeToCelestial(TDV, RightAscension, Declination))
        {
            RightAscension = EquatorialCoordinates.rightascension;
            Declination = EquatorialCoordinates.declination;
        }
    }

    m_SkyCurrentRADE.rightascension = RightAscension;
    m_SkyCurrentRADE.declination = Declination;
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
            // We only engage ACTIVE tracking if the mount is Alt-Az.
            // For Equatorial mount, we simply use user-selected tracking mode and let it passively track.
            else if (MountTypeSP[MOUNT_ALTAZ].getState() == ISS_ON)
            {
                TelescopeDirectionVector TDV;
                INDI::IHorizontalCoordinates targetMountAxisCoordinates { 0, 0 };

                // Start by transforming tracking target celestial coordinates to telescope coordinates.
                if (TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination,
                                                  0, TDV))
                {
                    // If mount is Alt-Az then that's all we need to do
                    AltitudeAzimuthFromTelescopeDirectionVector(TDV, targetMountAxisCoordinates);
                }
                // If transformation failed.
                else
                {
                    INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
                    EquatorialCoordinates.rightascension  = m_SkyTrackingTarget.rightascension;
                    EquatorialCoordinates.declination = m_SkyTrackingTarget.declination;
                    INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, ln_get_julian_from_sys(), &targetMountAxisCoordinates);
                }

                // Now add the guiding offsets.
                targetMountAxisCoordinates.azimuth += m_GuideOffset[AXIS_AZ];
                targetMountAxisCoordinates.altitude += m_GuideOffset[AXIS_ALT];

                // If we had guiding pulses active, mark them as complete
                if (GuideWENP.s == IPS_BUSY)
                    GuideComplete(AXIS_RA);
                if (GuideNSNP.s == IPS_BUSY)
                    GuideComplete(AXIS_DE);

                // Next get current alt-az
                INDI::IHorizontalCoordinates currentAltAz { 0, 0 };
                currentAltAz.azimuth = DegreesToAzimuth(EncodersToDegrees(EncoderNP[AXIS_AZ].getValue()));
                currentAltAz.altitude = EncodersToDegrees(EncoderNP[AXIS_ALT].getValue());

                // Offset in degrees
                double offsetAngle[2] = {0, 0};
                offsetAngle[AXIS_AZ] = (targetMountAxisCoordinates.azimuth - currentAltAz.azimuth);
                offsetAngle[AXIS_ALT] = (targetMountAxisCoordinates.altitude - currentAltAz.altitude);

                int32_t offsetSteps[2] = {0, 0};
                int32_t targetSteps[2] = {0, 0};
                double trackRates[2] = {0, 0};

                offsetSteps[AXIS_AZ] = offsetAngle[AXIS_AZ] * STEPS_PER_DEGREE;
                offsetSteps[AXIS_ALT] = offsetAngle[AXIS_ALT] * STEPS_PER_DEGREE;

                // Only apply trackinf IF we're still on the same side of the curve
                // If we switch over, let's settle for a bit
                if (m_LastOffset[AXIS_AZ] * offsetSteps[AXIS_AZ] >= 0 || m_OffsetSwitchSettle[AXIS_AZ]++ > 3)
                {
                    m_OffsetSwitchSettle[AXIS_AZ] = 0;
                    m_LastOffset[AXIS_AZ] = offsetSteps[AXIS_AZ];
                    targetSteps[AXIS_AZ] = DegreesToEncoders(AzimuthToDegrees(targetMountAxisCoordinates.azimuth));
                    trackRates[AXIS_AZ] = m_Controllers[AXIS_AZ]->calculate(targetSteps[AXIS_AZ], EncoderNP[AXIS_AZ].getValue());

                    LOGF_DEBUG("Tracking AZ Now: %.f Target: %d Offset: %d Rate: %.2f", EncoderNP[AXIS_AZ].getValue(), targetSteps[AXIS_AZ],
                               offsetSteps[AXIS_AZ], trackRates[AXIS_AZ]);
#ifdef DEBUG_PID
                    LOGF_DEBUG("Tracking AZ P: %f I: %f D: %f",
                               m_Controllers[AXIS_AZ]->propotionalTerm(),
                               m_Controllers[AXIS_AZ]->integralTerm(),
                               m_Controllers[AXIS_AZ]->derivativeTerm());
#endif

                    // Use PID to determine appropiate tracking rate
                    trackByRate(AXIS_AZ, trackRates[AXIS_AZ]);
                }

                // Only apply trackinf IF we're still on the same side of the curve
                // If we switch over, let's settle for a bit
                if (m_LastOffset[AXIS_ALT] * offsetSteps[AXIS_ALT] >= 0 || m_OffsetSwitchSettle[AXIS_ALT]++ > 3)
                {
                    m_OffsetSwitchSettle[AXIS_ALT] = 0;
                    m_LastOffset[AXIS_ALT] = offsetSteps[AXIS_ALT];
                    targetSteps[AXIS_ALT]  = DegreesToEncoders(targetMountAxisCoordinates.altitude);
                    trackRates[AXIS_ALT] = m_Controllers[AXIS_ALT]->calculate(targetSteps[AXIS_ALT], EncoderNP[AXIS_ALT].getValue());

                    LOGF_DEBUG("Tracking AL Now: %.f Target: %d Offset: %d Rate: %.2f", EncoderNP[AXIS_ALT].getValue(), targetSteps[AXIS_ALT],
                               offsetSteps[AXIS_ALT], trackRates[AXIS_ALT]);
#ifdef DEBUG_PID
                    LOGF_DEBUG("Tracking AL P: %f I: %f D: %f",
                               m_Controllers[AXIS_ALT]->propotionalTerm(),
                               m_Controllers[AXIS_ALT]->integralTerm(),
                               m_Controllers[AXIS_ALT]->derivativeTerm());
#endif
                    trackByRate(AXIS_ALT, trackRates[AXIS_ALT]);
                }
                break;
            }
            break;

            default:
                break;
            }
    }

    // Check if seeking index or leveling is done
    if (HomeSP.getState() == IPS_BUSY)
    {
        isHomingDone(AXIS_AZ);
        isHomingDone(AXIS_ALT);

        if (!m_HomingProgress[AXIS_AZ] && !m_HomingProgress[AXIS_ALT])
        {
            LOG_INFO("Homing complete.");
            HomeSP.reset();
            HomeSP.setState(IPS_OK);
            HomeSP.apply();
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
/// 0 to 360 degrees
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::EncodersToDegrees(uint32_t steps)
{
    return range360(steps * DEGREES_PER_STEP);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
uint32_t CelestronAUX::DegreesToEncoders(double degree)
{
    return round(range360(degree) * STEPS_PER_DEGREE);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::DegreesToAzimuth(double degree)
{
    if (isNorthHemisphere())
        return degree;
    else
        return range360(degree + 180);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::AzimuthToDegrees(double degree)
{
    if (isNorthHemisphere())
        return degree;
    else
        return range360(degree + 180);
}

/////////////////////////////////////////////////////////////////////////////////////
/// 0 to 24 hours
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::EncodersToHours(uint32_t steps)
{
    double value = steps * HOURS_PER_STEP;
    // North hemisphere
    if (isNorthHemisphere())
        return range24(value);
    else
        return range24(24 - value);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
uint32_t CelestronAUX::HoursToEncoders(double hour)
{
    return DegreesToEncoders(hour * 15);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::EncodersToRADE(INDI::IEquatorialCoordinates &coords, TelescopePierSide &pierSide)
{
    auto haEncoder = (EncoderNP[AXIS_RA].getValue() / STEPS_PER_REVOLUTION) * 360.0;
    auto deEncoder = 360.0 - (EncoderNP[AXIS_DE].getValue() / STEPS_PER_REVOLUTION) * 360.0;

    double de = 0, ha = 0;
    // Northern Hemisphere
    if (LocationN[LOCATION_LATITUDE].value >= 0)
    {
        if (m_IsWedge)
        {
            pierSide = PIER_UNKNOWN;
            if (deEncoder >= 270)
                de = 360 - deEncoder;
            else if (deEncoder >= 90)
                de = deEncoder - 180;
            else
                de = -deEncoder;

            if (haEncoder >= 180)
                ha = -((360 - haEncoder) / 360.0) * 24.0 ;
            else
                ha = (haEncoder / 360.0) * 24.0 ;
        }
        // "Normal" Pointing State (East, looking West)
        else if (deEncoder >= 0)
        {
            de = std::min(90 - deEncoder, 90.0);
            ha = -6.0 + (haEncoder / 360.0) * 24.0 ;
            pierSide = PIER_EAST;
        }
        // "Reversed" Pointing State (West, looking East)
        else
        {
            de = 90 + deEncoder;
            ha = 6.0 + (haEncoder / 360.0) * 24.0 ;
            pierSide = PIER_WEST;
        }
    }
    else
    {
        if (m_IsWedge)
        {
            pierSide = PIER_UNKNOWN;
            if (deEncoder >= 270)
                de = deEncoder - 360;
            else if (deEncoder >= 90)
                de = 180 - deEncoder;
            else
                de = deEncoder;

            if (haEncoder >= 180)
                ha = -((360 - haEncoder) / 360.0) * 24.0 ;
            else
                ha = (haEncoder / 360.0) * 24.0 ;
        }
        // East
        else if (deEncoder <= 0)
        {
            de = std::max(-90 - deEncoder, -90.0);
            ha = -6.0 - (haEncoder / 360.0) * 24.0 ;
            pierSide = PIER_WEST;
        }
        // West
        else
        {
            de = -90 + deEncoder;
            ha = 6.0 - (haEncoder / 360.0) * 24.0 ;
            pierSide = PIER_EAST;
        }
    }

    double lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
    double ra = range24(lst - ha);

    coords.rightascension = ra;
    coords.declination = de;
}

/////////////////////////////////////////////////////////////////////////////////////
/// HA encoders at 0 (HA = LST). When going WEST, steps increase from 0 to STEPS_PER_REVOLUTION {16777216}
/// counter clock-wise.
/// HA 0 to 6 hours range: 0 to 4194304
/// HA 0 to -6 hours range: 16777216 to 12582912
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::RADEToEncoders(const INDI::IEquatorialCoordinates &coords, uint32_t &haEncoder, uint32_t &deEncoder)
{
    double lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
    double dHA = rangeHA(lst - coords.rightascension);
    double de = 0, ha = 0;
    // Northern Hemisphere
    if (LocationN[LOCATION_LATITUDE].value >= 0)
    {
        if (m_IsWedge)
        {
            if (coords.declination < 0)
                de = -coords.declination;
            else
                de = 360 - coords.declination;

            if (dHA < 0)
                ha = 360 - ((dHA / -24.0) * 360.0);
            else
                ha = (dHA / 24.0) * 360.0;

        }
        // "Normal" Pointing State (East, looking West)
        else if (dHA <= 0)
        {
            de = -(coords.declination - 90.0);
            ha = (dHA + 6.0) * 360.0 / 24.0;
        }
        // "Reversed" Pointing State (West, looking East)
        else
        {
            de = coords.declination - 90.0;
            ha = (dHA - 6.0) * 360.0 / 24.0;
        }
    }
    else
    {
        if (m_IsWedge)
        {
            if (coords.declination >= 0)
                de = coords.declination;
            else
                de = 360 + coords.declination;

            if (dHA < 0)
                ha = 360 - ((dHA / -24.0) * 360.0);
            else
                ha = (dHA / 24.0) * 360.0;

        }
        // "Normal" Pointing State (East, looking West)
        else if (dHA <= 0)
        {
            de = -(coords.declination + 90.0);
            ha = -(dHA + 6.0) * 360.0 / 24.0;
        }
        // "Reversed" Pointing State (West, looking East)
        else
        {
            de = (coords.declination + 90.0);
            ha = -(dHA - 6.0) * 360 / 24.0;
        }
    }

    haEncoder =  (range360(ha) / 360.0) * STEPS_PER_REVOLUTION;
    deEncoder  = (360.0 - range360(de)) / 360.0 * STEPS_PER_REVOLUTION;
}

/////////////////////////////////////////////////////////////////////////////////////
/// -90 to +90
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::EncodersToDE(uint32_t steps, TelescopePierSide pierSide)
{
    double degrees = EncodersToDegrees(steps);
    double de = 0;
    if (m_IsWedge)
        de = degrees;
    else if ((isNorthHemisphere() && pierSide == PIER_WEST) || (!isNorthHemisphere() && pierSide == PIER_EAST))
        de = degrees - 270;
    else
        de = 90 - degrees;

    return rangeDec(de);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::DEToEncoders(double de)
{
    double degrees = 0;
    if (m_IsWedge)
        degrees = de;
    else if ((isNorthHemisphere() && m_TargetPierSide == PIER_WEST) || (!isNorthHemisphere() && m_TargetPierSide == PIER_EAST))
        degrees = 270 + de;
    else
        degrees = 90 - de;
    return DegreesToEncoders(degrees);
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
    trackByRate(axis, 0);
    AUXCommand command(fast ? MC_GOTO_FAST : MC_GOTO_SLOW, APP, axis == AXIS_AZ ? AZM : ALT);
    m_AxisStatus[axis] = SLEWING;
    command.setData(steps, 3);
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
    trackByRate(axis, 0);
    AUXCommand command(rate >= 0 ? MC_MOVE_POS : MC_MOVE_NEG, APP, axis == AXIS_AZ ? AZM : ALT);
    command.setData(std::abs(rate), 1);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::goHome(INDI_HO_AXIS axis)
{
    AUXCommand command(axis == AXIS_AZ ? MC_SEEK_INDEX : MC_LEVEL_START, APP, axis == AXIS_AZ ? AZM : ALT);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::isHomingDone(INDI_HO_AXIS axis)
{
    AUXCommand command(axis == AXIS_AZ ? MC_SEEK_DONE : MC_LEVEL_DONE, APP, axis == AXIS_AZ ? AZM : ALT);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
}

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
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::setCordWrapEnabled(bool enable)
{

    AUXCommand command(enable ? MC_ENABLE_CORDWRAP : MC_DISABLE_CORDWRAP, APP, AZM);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
};

bool CelestronAUX::getCordWrapEnabled()
{
    AUXCommand command(MC_POLL_CORDWRAP, APP, AZM);
    sendAUXCommand(command);
    readAUXResponse(command);
    return m_CordWrapActive;
};


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::setCordWrapPosition(uint32_t steps)
{
    AUXCommand command(MC_SET_CORDWRAP_POS, APP, AZM);
    command.setData(steps, 3);
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
    AUXCommand command(MC_MOVE_POS, APP, (axis == AXIS_ALT) ? ALT : AZM);
    command.setData(0, 1);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;

}
/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Abort()
{
    //    getEncoder(AXIS_AZ);
    //    getEncoder(AXIS_ALT);
    //    double ms = m_TrackingElapsedTimer.elapsed();
    //    LOGF_INFO("*** Elapsed %.f ms", ms);
    //    LOGF_INFO("*** Axis1 start: %.f finish: %.f steps/s: %.4f", m_TrackStartSteps[AXIS_AZ], EncoderNP[AXIS_AZ].getValue(), std::abs(EncoderNP[AXIS_AZ].getValue() - m_TrackStartSteps[AXIS_AZ]) / (ms/1000.));
    //    LOGF_INFO("*** Axis2 start: %.f finish: %.f steps/s: %.4f", m_TrackStartSteps[AXIS_ALT], EncoderNP[AXIS_ALT].getValue(), std::abs(EncoderNP[AXIS_ALT].getValue() - m_TrackStartSteps[AXIS_ALT]) / (ms/1000.));
    stopAxis(AXIS_AZ);
    stopAxis(AXIS_ALT);
    m_GuideOffset[AXIS_AZ] = m_GuideOffset[AXIS_ALT] = 0;
    TrackState = SCOPE_IDLE;

    if (HorizontalCoordsNP.getState() != IPS_IDLE)
    {
        HorizontalCoordsNP.setState(IPS_IDLE);
        HorizontalCoordsNP.apply();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
/// Rate is Celestron specific and roughly equals 80 ticks per 1 motor step
/// rate = 80 would cause the motor to spin at a rate of 1 step/s
/// Have to check if 80 is specific to my Evolution 6" or not.
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::trackByRate(INDI_HO_AXIS axis, int32_t rate)
{
    if (std::abs(rate) > 0 && rate == m_LastTrackRate[axis])
        return true;

    m_LastTrackRate[axis] = rate;
    AUXCommand command(rate < 0 ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, axis == AXIS_AZ ? AZM : ALT);
    // 24bit rate
    command.setData(std::abs(rate), 3);
    sendAUXCommand(command);
    readAUXResponse(command);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::trackByMode(INDI_HO_AXIS axis, uint8_t mode)
{
    AUXCommand command(isNorthHemisphere() ? MC_SET_POS_GUIDERATE : MC_SET_NEG_GUIDERATE, APP, axis == AXIS_AZ ? AZM : ALT);
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
        trackByRate(AXIS_AZ, 0);
        trackByRate(AXIS_ALT, 0);

        if (HorizontalCoordsNP.getState() != IPS_IDLE)
        {
            HorizontalCoordsNP.setState(IPS_IDLE);
            HorizontalCoordsNP.apply();
        }
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

        double steps[2] = {0, 0};
        // rate = (steps) * gain
        steps[AXIS_AZ] = raRate * STEPS_PER_ARCSEC * GAIN_STEPS;
        steps[AXIS_ALT] = deRate * STEPS_PER_ARCSEC * GAIN_STEPS;
        trackByRate(AXIS_AZ, steps[AXIS_AZ]);
        trackByRate(AXIS_ALT, steps[AXIS_ALT]);
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

    if (TrackState == SCOPE_TRACKING)
    {
        if (mode == TRACK_CUSTOM)
        {
            double steps[2] = {0, 0};
            // rate = (steps) * gain
            steps[AXIS_AZ] = m_TrackRates[AXIS_AZ] * STEPS_PER_ARCSEC * GAIN_STEPS;
            steps[AXIS_ALT] = m_TrackRates[AXIS_ALT] * STEPS_PER_ARCSEC * GAIN_STEPS;
            trackByRate(AXIS_AZ, steps[AXIS_AZ]);
            trackByRate(AXIS_ALT, steps[AXIS_ALT]);
        }
        else
            trackByMode(AXIS_AZ, mode);
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
            LOGF_DEBUG("GPS: Sending LAT/LONG Lat:%f Lon:%f", LocationN[LOCATION_LATITUDE].value,
                       LocationN[LOCATION_LONGITUDE].value);
            AUXCommand cmd(m.command(), GPS, m.source());
            if (m.command() == GPS_GET_LAT)
                cmd.setData(STEPS_PER_DEGREE * LocationN[LOCATION_LATITUDE].value);
            else
                cmd.setData(STEPS_PER_DEGREE * LocationN[LOCATION_LONGITUDE].value);
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

    if ((m.destination() == GPS) && (m.source() != APP))
    {
        // Avoid infinite loop by not responding to ourselves
        emulateGPS(m);
    }
    else if (m.destination() == APP)
        switch (m.command())
        {
            case MC_GET_POSITION:
                switch (m.source())
                {
                    case ALT:
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
                        GuideRateNP[AXIS_ALT].setValue(m.getData() / 255.);
                        break;
                    case AZM:
                        GuideRateNP[AXIS_AZ].setValue(m.getData() * 255.);
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

            case MC_LEVEL_DONE:
                m_HomingProgress[AXIS_ALT] = m.getData() == 0x00;
                break;

            case MC_SEEK_DONE:
                m_HomingProgress[AXIS_AZ] = m.getData() == 0x00;
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
            {
                uint8_t *verBuf = nullptr;
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
                if (verBuf != nullptr)
                {
                    size_t verBuflen = 4;
                    if ( verBuflen != m.dataSize())
                    {
                        LOGF_DEBUG("Data and buffer size mismatch for GET_VER: buf[%d] vs data[%d]", verBuflen, m.dataSize());
                    }
                    for (int i = 0; i < (int)std::min(verBuflen, m.dataSize()); i++)
                        verBuf[i] = m.data()[i];
                }
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

