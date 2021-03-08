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

#include "celestronaux.h"
#include "config.h"

using namespace INDI::AlignmentSubsystem;

#define MOUNTINFO_TAB "Mount info"

std::unique_ptr<CelestronAUX> telescope_caux(new CelestronAUX());

void msleep(unsigned ms)
{
    usleep(ms * 1000);
}

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


void ISGetProperties(const char *dev)
{
    telescope_caux->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    telescope_caux->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    telescope_caux->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    telescope_caux->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    telescope_caux->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}


void ISSnoopDevice(XMLEle *root)
{
    const char *propName = findXMLAttValu(root, "name");

    // update cordwrap position at each init of the alignment subsystem
    if (!strcmp(propName, "ALIGNMENT_SUBSYSTEM_MATH_PLUGIN_INITIALISE"))
    {
        //long cwpos = range360(telescope_caux->requestedCordwrapPos + telescope_caux->getNorthAz()) * CelestronAUX::STEPS_PER_DEGREE;
        long cwpos = range360(telescope_caux->requestedCordwrapPos) * CelestronAUX::STEPS_PER_DEGREE;
        telescope_caux->setCordwrapPos(cwpos);
        telescope_caux->getCordwrapPos();
    }

    telescope_caux->ISSnoopDevice(root);
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
CelestronAUX::CelestronAUX()
    : ScopeStatus(IDLE), AxisStatusALT(STOPPED), AxisDirectionALT(FORWARD), AxisStatusAZ(STOPPED),
      AxisDirectionAZ(FORWARD), TraceThisTickCount(0), TraceThisTick(false),
      DBG_CAUX(INDI::Logger::getInstance().addDebugLevel("AUX", "CAUX")),
      DBG_SERIAL(INDI::Logger::getInstance().addDebugLevel("Serial", "CSER"))
{
    setVersion(CAUX_VERSION_MAJOR, CAUX_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION | TELESCOPE_CAN_CONTROL_TRACK, 4);

    LOG_DEBUG("Celestron AUX instancing.");

    //Both communication available, Serial and network (tcp/ip).
    setTelescopeConnection(CONNECTION_TCP | CONNECTION_SERIAL);

    // Approach from no further then degs away
    Approach = 1.0;

    // Max ticks before we reissue the goto to update position
    maxSlewTicks = 15;
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

    LOG_INFO("Telescope motion aborted.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::detectNetScope(bool set_ip)
{
    struct sockaddr_in myaddr;           /* our address */
    struct sockaddr_in remaddr;          /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    int recvlen;                         /* # bytes received */
    int fd;                              /* our socket */
    int BUFSIZE = 2048;
    int PORT    = 55555;
    unsigned char buf[BUFSIZE]; /* receive buffer */

    /* create a UDP socket */
    LOG_DEBUG("CAUX: Detecting scope IP ... ");
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        LOGF_ERROR("cannot create socket: %s", strerror(errno));
        return false;
    }

    /* Set socket timeout */
    timeval tv;
    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

    /* bind the socket to any valid IP address and a specific port */

    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family      = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port        = htons(PORT);

    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
    {
        LOGF_ERROR("bind failed: %s", strerror(errno));
        return false;
    }

    /* now loop, receiving data and printing what we received
        wait max 20 sec
    */
    for (int n = 0; n < 10; n++)
    {
        recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
        // Scope broadcasts 110b UDP packets from port 2000 to 55555
        // we use it for detection
        if (ntohs(remaddr.sin_port) == 2000 && recvlen == 110)
        {
            LOGF_WARN("%s:%d (%d)", inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port), recvlen);
            //addr.sin_addr.s_addr = remaddr.sin_addr.s_addr;
            //addr.sin_port        = remaddr.sin_port;
            if (set_ip)
            {
                tcpConnection->setDefaultHost(inet_ntoa(remaddr.sin_addr));
                tcpConnection->setDefaultPort(ntohs(remaddr.sin_port));
                if ( getActiveConnection() == tcpConnection )
                    // Refresh connection params
                    tcpConnection->Activated();
            }
            close(fd);
            return true;
        }
    }
    /* never exits */
    close(fd);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::detectNetScope()
{
    return detectNetScope(false);
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
            if ((isRTSCTS = detectRTSCTS()))
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
                msleep(200);

                LOG_INFO("Setting serial speed to 9600 baud.");

                // detect if connectd to HC port or to mount USB port
                // ask for HC version
                char version[10];
                if ((isHC = detectHC(version, (size_t)10)))
                    LOGF_INFO("Detected Hand Controller (v%s) serial connection.", version);
                else
                    LOG_INFO("Detected Mount USB serial connection.");
            }
        }
        else
        {
            LOG_INFO("Waiting for mount connection to settle...");
            msleep(1000);
            return true;
        }

        // read firmware version, if read ok, detected scope
        LOG_DEBUG("Communicating with mount motor controllers...");
        if (getVersion(AZM) && getVersion(ALT))
        {
            LOG_INFO("Got response from target ALT or AZM. Probing all targets.");
        }
        else
        {
            LOG_ERROR("Got no response from target ALT or AZM.");
            LOG_ERROR("Cannot continue without connection to motor controllers.");
            return false;
        }

        LOG_DEBUG("Connection ready. Starting Processing.");

        // set mount type to alignment subsystem
        currentMountType = IUFindOnSwitchIndex(&MountTypeSP) ? ALTAZ : EQUATORIAL;
        SetApproximateMountAlignmentFromMountType(currentMountType);
        // tell the alignment math plugin to reinitialise
        Initialise(this);

        // update cordwrap position at each init of the alignment subsystem
        //long cwpos = range360(requestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
        long cwpos = range360(requestedCordwrapPos) * STEPS_PER_DEGREE;
        setCordwrapPos(cwpos);
        getCordwrapPos();

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
bool CelestronAUX::Park()
{
    // Park at the northern or southern horizon
    // This is a designated by celestron parking position
    Abort();
    LOG_INFO("Telescope park in progress...");
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
ln_hrz_posn CelestronAUX::AltAzFromRaDec(double ra, double dec, double ts)
{
    // Call the alignment subsystem to translate the celestial reference frame coordinate
    // into a telescope reference frame coordinate
    TelescopeDirectionVector TDV;
    ln_hrz_posn AltAz;

    if (TransformCelestialToTelescope(ra, dec, ts, TDV))
        // The alignment subsystem has successfully transformed my coordinate
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    else
    {
        LOG_ERROR("AltAzFromRaDec - TransformCelestialToTelescope failed");
        LOG_WARN("Activate the Alignment Subsystem.");
        // the best I can do
        AltAz.az = ra;
        AltAz.alt = dec;
    }

    return AltAz;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double CelestronAUX::getNorthAz()
{
    ln_lnlat_posn location;
    double northAz;
    if (!GetDatabaseReferencePosition(location))
        northAz = 0.;
    else
        northAz = AltAzFromRaDec(get_local_sidereal_time(location.lng), 0., 0.).az;
    LOGF_DEBUG("North Azimuth = %lf", northAz);
    return northAz;
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
        CurrentTrackingTarget.ra  = ra;
        CurrentTrackingTarget.dec = dec;
        NewTrackingTarget         = CurrentTrackingTarget;
        LOGF_INFO("Slewing to RA: %s DE: %s with Tracking.", RAStr, DecStr);
    }

    GoToTarget.ra  = ra;
    GoToTarget.dec = dec;

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
    ln_hrz_posn AltAz;

    AltAz = AltAzFromRaDec(ra, dec, -timeshift);

    // For high Alt azimuth may change very fast.
    // Let us limit azimuth approach to maxApproach degrees
    if (ScopeStatus != APPROACH)
    {
        ln_hrz_posn trgAltAz = AltAzFromRaDec(ra, dec, 0);
        double d;

        d = anglediff(AltAz.az, trgAltAz.az);
        LOGF_DEBUG("Azimuth approach:  %lf (%lf)", d, Approach);
        AltAz.az = trgAltAz.az + ((d > 0) ? Approach : -Approach);

        d = anglediff(AltAz.alt, trgAltAz.alt);
        LOGF_DEBUG("Altitude approach:  %lf (%lf)", d, Approach);
        AltAz.alt = trgAltAz.alt + ((d > 0) ? Approach : -Approach);
    }

    // Fold Azimuth into 0-360
    AltAz.az = range360(AltAz.az);
    // Altitude encoder runs -90 to +90 there is no point going outside.
    if (AltAz.alt > 90.0)
        AltAz.alt = 90.0;
    if (AltAz.alt < -90.0)
        AltAz.alt = -90.0;

    LOGF_DEBUG("Goto: Scope reference frame target altitude %lf azimuth %lf", AltAz.alt, AltAz.az);

    TrackState = SCOPE_SLEWING;
    if (ScopeStatus == APPROACH)
    {
        // We need to make a slow slew to approach the final position
        ScopeStatus = SLEWING_SLOW;
        GoToSlow(long(AltAz.alt * STEPS_PER_DEGREE),
                 long(AltAz.az * STEPS_PER_DEGREE),
                 ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s);
    }
    else
    {
        // Just make a standard fast slew
        slewTicks   = 0;
        ScopeStatus = SLEWING_FAST;
        GoToFast(long(AltAz.alt * STEPS_PER_DEGREE),
                 long(AltAz.az * STEPS_PER_DEGREE),
                 ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s);
    }

    return true;
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
    pastAlignmentSubsystemStatus = true;

    // Default connection options
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    tcpConnection->setDefaultHost(CAUX_DEFAULT_IP);
    tcpConnection->setDefaultPort(CAUX_DEFAULT_PORT);

    // Firmware
    IUFillText(&FirmwareT[FW_HC], "HC version", "", nullptr);
    IUFillText(&FirmwareT[FW_HCp], "HC+ version", "", nullptr);
    IUFillText(&FirmwareT[FW_MB], "Mother Board version", "", nullptr);
    IUFillText(&FirmwareT[FW_AZM], "Ra/AZM version", "", nullptr);
    IUFillText(&FirmwareT[FW_ALT], "Dec/ALT version", "", nullptr);
    IUFillText(&FirmwareT[FW_WiFi], "WiFi version", "", nullptr);
    IUFillText(&FirmwareT[FW_BAT], "Battery version", "", nullptr);
    IUFillText(&FirmwareT[FW_CHG], "Charger version", "", nullptr);
    IUFillText(&FirmwareT[FW_LIGHT], "Lights version", "", nullptr);
    IUFillText(&FirmwareT[FW_GPS], "GPS version", "", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 10, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    // mount type
    IUFillSwitch(&MountTypeS[EQUATORIAL], "EQUATORIAL", "Equatorial", ISS_OFF);
    IUFillSwitch(&MountTypeS[ALTAZ], "ALTAZ", "AltAz", ISS_ON);
    IUFillSwitchVector(&MountTypeSP, MountTypeS, 2, getDeviceName(),
                       "MOUNT_TYPE", "Mount Type", MOUNTINFO_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE );

    // cord wrap
    IUFillSwitch(&CordWrapS[CORDWRAP_OFF], "CORDWRAP_OFF", "OFF", ISS_OFF);
    IUFillSwitch(&CordWrapS[CORDWRAP_ON], "CORDWRAP_ON", "ON", ISS_ON);
    IUFillSwitchVector(&CordWrapSP, CordWrapS, 2, getDeviceName(), "CORDWRAP", "Cord Wrap", MOTION_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Cord Wrap Position
    IUFillSwitch(&CWPosS[CORDWRAP_N], "CORDWRAP_N", "North", ISS_ON);
    IUFillSwitch(&CWPosS[CORDWRAP_E], "CORDWRAP_E", "East",  ISS_OFF);
    IUFillSwitch(&CWPosS[CORDWRAP_S], "CORDWRAP_S", "South", ISS_OFF);
    IUFillSwitch(&CWPosS[CORDWRAP_W], "CORDWRAP_W", "West",  ISS_OFF);
    IUFillSwitchVector(&CWPosSP, CWPosS, 4, getDeviceName(), "CORDWRAP_POS", "CW Position", MOTION_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // GPS Emulation
    IUFillSwitch(&GPSEmuS[GPSEMU_OFF], "GPSEMU_OFF", "OFF", ISS_OFF);
    IUFillSwitch(&GPSEmuS[GPSEMU_ON], "GPSEMU_ON", "ON", ISS_ON);
    IUFillSwitchVector(&GPSEmuSP, GPSEmuS, 2, getDeviceName(), "GPSEMU", "GPS Emu", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Network Detect
    IUFillSwitch(&NetDetectS[ISS_OFF], "ISS_OFF", "Detect", ISS_OFF);
    IUFillSwitchVector(&NetDetectSP, NetDetectS, 1, getDeviceName(), "NETDETECT", "Network scope", CONNECTION_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    // Guide Properties
    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[AXIS_RA], "GUIDE_RATE_WE", "W/E Rate", "%.f", 10, 100, 1, 50);
    IUFillNumber(&GuideRateN[AXIS_DE], "GUIDE_RATE_NS", "N/S Rate", "%.f", 10, 100, 1, 50);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", GUIDE_TAB, IP_RW, 0,
                       IPS_IDLE);

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
bool CelestronAUX::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(&CordWrapSP);

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);
        loadConfig(true, GuideRateNP.name);

        defineProperty(&MountTypeSP);

        getCordwrap();
        IUResetSwitch(&CordWrapSP);
        CordWrapS[m_CordWrapActive].s = ISS_ON;
        defineProperty(&CordWrapSP);

        getCordwrapPos();
        IUResetSwitch(&CWPosSP);
        LOGF_INFO("Set CordwrapPos index %d", (int(m_CordWrapPosition / STEPS_PER_DEGREE) / 90));
        CWPosS[(int(m_CordWrapPosition / STEPS_PER_DEGREE) / 90)].s = ISS_ON;
        defineProperty(&CWPosSP);

        defineProperty(&GPSEmuSP);
        IUResetSwitch(&GPSEmuSP);
        GPSEmuS[gpsemu].s = ISS_ON;
        IDSetSwitch(&GPSEmuSP, nullptr);

        getVersions();

        // display firmware versions
        char fwText[16] = {0};
        IUSaveText(&FirmwareT[FW_HC], "HC version");
        IUSaveText(&FirmwareT[FW_HCp], "HC+ version");
        //IUSaveText(&FirmwareT[FW_MODEL], fwInfo.Model.c_str());
        //IUSaveText(&FirmwareT[FW_VERSION], fwInfo.Version.c_str());
        IUSaveText(&FirmwareT[FW_MB], "Motherboard version");
        snprintf(fwText, 10, "%d.%02d", m_AzimuthVersionMajor, m_AzimuthVersionMinor);
        IUSaveText(&FirmwareT[FW_AZM], fwText);
        snprintf(fwText, 10, "%d.%02d", m_AltitudeVersionMajor, m_AltitudeVersionMinor);
        IUSaveText(&FirmwareT[FW_ALT], fwText);
        IUSaveText(&FirmwareT[FW_WiFi], "WiFi version");
        IUSaveText(&FirmwareT[FW_BAT], "Battery version");
        IUSaveText(&FirmwareT[FW_CHG], "Charger version");
        IUSaveText(&FirmwareT[FW_LIGHT], "Lights version");
        IUSaveText(&FirmwareT[FW_GPS], "GPS version");
        defineProperty(&FirmwareTP);
    }
    else
    {
        deleteProperty(MountTypeSP.name);
        deleteProperty(CordWrapSP.name);
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
        deleteProperty(CWPosSP.name);
        deleteProperty(GPSEmuSP.name);
        deleteProperty(FirmwareTP.name);
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
    IUSaveConfigSwitch(fp, &CordWrapSP);
    IUSaveConfigSwitch(fp, &CWPosSP);
    IUSaveConfigSwitch(fp, &GPSEmuSP);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::ISGetProperties(const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    defineProperty(&NetDetectSP);
    return;
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
    //  first check if it's for our device

    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        }

        processGuiderProperties(name, values, names, n);

        // Process alignment properties
        ProcessAlignmentNumberProperties(this, name, values, names, n);
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
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
        if (strcmp(name, MountTypeSP.name) == 0)
        {
            if (IUUpdateSwitch(&MountTypeSP, states, names, n) < 0)
                return false;

            MountTypeSP.s = IPS_OK;
            IDSetSwitch(&MountTypeSP, nullptr);

            requestedMountType =
                IUFindOnSwitchIndex(&MountTypeSP) ? ALTAZ : EQUATORIAL;

            if (requestedMountType < 0)
                return false;

            // If nothing changed, return
            if (requestedMountType == currentMountType)
                return true;

            currentMountType = requestedMountType;

            LOGF_INFO("update mount: mount type %d", currentMountType);

            // set mount type to alignment subsystem
            SetApproximateMountAlignmentFromMountType(currentMountType);
            // tell the alignment math plugin to reinitialise
            Initialise(this);

            // update cordwrap position at each init of the alignment subsystem
            //long cwpos = range360(requestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
            long cwpos = range360(requestedCordwrapPos) * STEPS_PER_DEGREE;
            setCordwrapPos(cwpos);
            getCordwrapPos();

            return true;
        }

        // Slew mode
        if (!strcmp(name, SlewRateSP.name))
        {
            if (IUUpdateSwitch(&SlewRateSP, states, names, n) < 0)
                return false;

            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }

        // Cordwrap
        if (!strcmp(name, CordWrapSP.name))
        {
            // Even if the switch is in the requested state
            // perform the action on the mount.
            //int CWIndex = IUFindOnSwitchIndex(&CordWrapSP);
            IUUpdateSwitch(&CordWrapSP, states, names, n);
            int CWIndex = IUFindOnSwitchIndex(&CordWrapSP);
            LOGF_INFO("CordWrap is now %s (%d)", CordWrapS[CWIndex].label, CWIndex);
            CordWrapSP.s = IPS_OK;
            IDSetSwitch(&CordWrapSP, nullptr);
            setCordwrap(CWIndex);
            getCordwrap();
            return true;
        }

        // Cordwrap Pos
        if (!strcmp(name, CWPosSP.name))
        {
            // Even if the switch is in the requested state
            // perform the action on the mount.
            int CWIndex;
            IUUpdateSwitch(&CWPosSP, states, names, n);
            CWIndex = IUFindOnSwitchIndex(&CWPosSP);
            LOGF_DEBUG("CordWrap Position is now %s (%d)", CWPosS[CWIndex].label, CWIndex);
            CWPosSP.s = IPS_OK;
            IDSetSwitch(&CWPosSP, nullptr);
            switch (CWIndex)
            {
                case CORDWRAP_N:
                    requestedCordwrapPos = 0;
                    break;
                case CORDWRAP_E:
                    requestedCordwrapPos = 90;
                    break;
                case CORDWRAP_S:
                    requestedCordwrapPos = 180;
                    break;
                case CORDWRAP_W:
                    requestedCordwrapPos = 270;
                    break;
                default:
                    requestedCordwrapPos = 0;
                    break;
            }
            //long cwpos = range360(requestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
            long cwpos = range360(requestedCordwrapPos) * STEPS_PER_DEGREE;
            setCordwrapPos(cwpos);
            getCordwrapPos();
            return true;
        }


        // GPS Emulation
        if (!strcmp(name, GPSEmuSP.name))
        {
            IUUpdateSwitch(&GPSEmuSP, states, names, n);
            int Index = IUFindOnSwitchIndex(&GPSEmuSP);
            LOGF_INFO("GPSEmu is now %s (%d)", GPSEmuS[Index].label, Index);
            GPSEmuSP.s = IPS_OK;
            IDSetSwitch(&GPSEmuSP, nullptr);
            gpsemu = Index;
            return true;
        }

        // Detect networked mount
        if (!strcmp(name, NetDetectSP.name))
        {
            LOG_INFO("Detecting networked scope...");
            IUUpdateSwitch(&NetDetectSP, states, names, n);
            NetDetectSP.s = IPS_BUSY;
            IDSetSwitch(&NetDetectSP, nullptr);
            if ( detectNetScope(true) )
            {
                NetDetectSP.s = IPS_OK;
                LOG_INFO("Scope detected.");
            }
            else
            {
                LOG_INFO("Detection failed.");
                NetDetectSP.s = IPS_ALERT;
            }

            IUResetSwitch(&NetDetectSP);
            IDSetSwitch(&NetDetectSP, nullptr);
        }
        // Process alignment properties
        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }

    //  Nobody has claimed this, so, ignore it
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
bool CelestronAUX::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP);
    LOGF_DEBUG("MoveNS dir:%d, cmd:%d, rate:%d", dir, command, rate);
    AxisDirectionALT = (dir == DIRECTION_NORTH) ? FORWARD : REVERSE;
    AxisStatusALT    = (command == MOTION_START) ? SLEWING : STOPPED;
    ScopeStatus      = SLEWING_MANUAL;
    TrackState       = SCOPE_SLEWING;
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
    struct ln_equ_posn RaDec;
    struct ln_hrz_posn AltAz;
    double RightAscension, Declination;

    if (MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
    {
        RaDec.ra   = double(GetAZ()) / STEPS_PER_DEGREE;
        RaDec.dec  = double(GetALT()) / STEPS_PER_DEGREE;
        TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(RaDec);

        if (TraceThisTick)
            LOGF_DEBUG("ReadScopeStatus - HA %lf deg ; Dec %lf deg", RaDec.ra, RaDec.dec);
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
        AltAz.az  = double(GetAZ()) / STEPS_PER_DEGREE;
        AltAz.alt = double(GetALT()) / STEPS_PER_DEGREE;
        TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);

        if (TraceThisTick)
            LOGF_DEBUG("ReadScopeStatus - Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);
    }

    if (TransformTelescopeToCelestial(TDV, RightAscension, Declination))
    {
        if (TraceThisTick)
            LOGF_DEBUG("ReadScopeStatus - RA %lf hours DEC %lf degrees", RightAscension, Declination);

        // In case we are slewing while tracking update the potential target
        NewTrackingTarget.ra  = RightAscension;
        NewTrackingTarget.dec = Declination;
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
    struct ln_equ_posn RaDec
    {
        0, 0
    };
    struct ln_hrz_posn AltAz
    {
        0, 0
    };

    if (MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
    {
        RaDec.ra   = double(GetAZ()) / STEPS_PER_DEGREE;
        RaDec.dec  = double(GetALT()) / STEPS_PER_DEGREE;
    }
    else
    {
        AltAz.az  = double(GetAZ()) / STEPS_PER_DEGREE;
        AltAz.alt = double(GetALT()) / STEPS_PER_DEGREE;
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
        //long cwpos = range360(requestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
        long cwpos = range360(requestedCordwrapPos) * STEPS_PER_DEGREE;
        setCordwrapPos(cwpos);
        getCordwrapPos();

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
    TraceThisTickCount++;
    if (60 == TraceThisTickCount)
    {
        TraceThisTick      = true;
        TraceThisTickCount = 0;
    }

    // issue a warning when alignment subsystem is off
    currentAlignmentSubsystemStatus = IsAlignmentSubsystemActive();
    if (pastAlignmentSubsystemStatus && !currentAlignmentSubsystemStatus)
        LOG_WARN("Alignment Subsystem is NOT active: SYNC will be ignored.");
    pastAlignmentSubsystemStatus = currentAlignmentSubsystemStatus;

    // Simulate mount movement

    static struct timeval ltv; // previous system time
    struct timeval tv;         // new system time
    double dt;                 // Elapsed time in seconds since last tick

    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;

    TimerTick(dt);

    // This will call ReadScopeStatus
    INDI::Telescope::TimerHit();

    // OK I have updated the celestial reference frame RA/DEC in ReadScopeStatus
    // Now handle the tracking state
    switch (TrackState)
    {
        case SCOPE_PARKING:
            if (!slewing())
            {
                SetTrackEnabled(false);
                SetParked(true);
                LOG_DEBUG("Telescope parked.");
            }
            break;

        case SCOPE_SLEWING:
            if (slewing())
            {
                // The scope is still slewing
                slewTicks++;
                if ((ScopeStatus == SLEWING_FAST) && (slewTicks > maxSlewTicks))
                {
                    // Slewing too long, reissue GoTo to update target position
                    Goto(GoToTarget.ra, GoToTarget.dec);
                    slewTicks = 0;
                }
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
                    Goto(GoToTarget.ra, GoToTarget.dec);
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
                               CurrentTrackingTarget.ra, CurrentTrackingTarget.dec);
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
            // Continue or start tracking
            // Calculate where the mount needs to be in a minute
            double JulianOffset = 60.0 / (24.0 * 60 * 60);
            TelescopeDirectionVector TDV;
            ln_hrz_posn AltAz, AAzero;

            AltAz  = AltAzFromRaDec(CurrentTrackingTarget.ra, CurrentTrackingTarget.dec, JulianOffset);
            AAzero = AltAzFromRaDec(CurrentTrackingTarget.ra, CurrentTrackingTarget.dec, 0);
            if (TraceThisTick)
                LOGF_DEBUG("Tracking - Calculated Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);
            /*
            TODO
            The tracking should take into account movement of the scope
            by the hand controller (we can hear the commands)
            and the movements made by the joystick.
            Right now when we move the scope by HC it returns to the
            designated target by corrective tracking.
            */

            // Fold Azimuth into 0-360
            AltAz.az = range360(AltAz.az);

            {
                long altRate, azRate;

                // This is in steps per minute
                altRate = long(AltAz.alt * STEPS_PER_DEGREE - GetALT());
                azRate  = long(AltAz.az * STEPS_PER_DEGREE - GetAZ());

                if (TraceThisTick)
                    LOGF_DEBUG("Target (AltAz): %f  %f  Scope  (AltAz)  %f  %f", AltAz.alt, AltAz.az,
                               GetALT() / STEPS_PER_DEGREE, GetAZ() / STEPS_PER_DEGREE);

                if (std::abs(azRate) > STEPS_PER_REVOLUTION / 2)
                {
                    // Crossing the meridian. AZ skips from 350+ to 0+
                    // Correct for wrap-around
                    azRate += STEPS_PER_REVOLUTION;
                    if (azRate > STEPS_PER_REVOLUTION)
                        azRate %= STEPS_PER_REVOLUTION;
                }

                // Track function needs rates in 1000*arcmin/minute
                // Rates here are in steps/minute
                // conv. factor: TRACK_SCALE = 60000/STEPS_PER_DEGREE
                altRate = long(TRACK_SCALE * altRate);
                azRate  = long(TRACK_SCALE * azRate);
                Track(altRate, azRate);

                if (TraceThisTick)
                    LOGF_DEBUG("TimerHit - Tracking AltRate %d AzRate %d ; Pos diff (deg): Alt: %f Az: %f",
                               altRate, azRate, AltAz.alt - AAzero.alt, anglediff(AltAz.az, AAzero.az));
            }
            break;
        }

        default:
            break;
    }

    TraceThisTick = false;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::updateLocation(double latitude, double longitude, double elevation)
{
    // Update INDI Alignment Subsystem Location
    UpdateLocation(latitude, longitude, elevation);

    // check for site non null latitude/longitude
    if (!latitude)
        LOG_ERROR("Missing latitude (INDI panel > Celestron AUX > Site Management)");
    if (!longitude)
        LOG_ERROR("Missing longitude (INDI panel > Celestron AUX > Site Management)");

    // take care of latitude for north or south emisphere
    currentMountType = IUFindOnSwitchIndex(&MountTypeSP) ? ALTAZ : EQUATORIAL;
    SetApproximateMountAlignmentFromMountType(currentMountType);
    // tell the alignment math plugin to reinitialise
    Initialise(this);

    // update cordwrap position at each init of the alignment subsystem
    //long cwpos = range360(requestedCordwrapPos + getNorthAz()) * STEPS_PER_DEGREE;
    long cwpos = range360(requestedCordwrapPos) * STEPS_PER_DEGREE;
    setCordwrapPos(cwpos);
    getCordwrapPos();

    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
long CelestronAUX::GetALT()
{
    // return alt encoder adjusted to -90...90
    if (m_AltSteps > STEPS_PER_REVOLUTION / 2)
    {
        return static_cast<int32_t>(m_AltSteps) - STEPS_PER_REVOLUTION;
    }
    else
    {
        return static_cast<int32_t>(m_AltSteps);
    }
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
long CelestronAUX::GetAZ()
{
    return m_AzSteps % STEPS_PER_REVOLUTION;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::slewing()
{
    return m_SlewingAlt || m_SlewingAz;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Slew(AUXTargets trg, int rate)
{
    AUXCommand cmd((rate < 0) ? MC_MOVE_NEG : MC_MOVE_POS, APP, trg);
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
bool CelestronAUX::GoToFast(long alt, long az, bool track)
{
    targetAlt  = alt;
    targetAz   = az;
    m_Tracking   = track;
    m_SlewingAlt = m_SlewingAz = true;
    Track(0, 0);
    AUXCommand altcmd(MC_GOTO_FAST, APP, ALT);
    AUXCommand azmcmd(MC_GOTO_FAST, APP, AZM);
    altcmd.setPosition(alt);
    // N-based azimuth
    //    az += STEPS_PER_REVOLUTION / 2;
    //    az %= STEPS_PER_REVOLUTION;
    azmcmd.setPosition(az);
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
bool CelestronAUX::GoToSlow(long alt, long az, bool track)
{
    targetAlt  = alt;
    targetAz   = az;
    m_Tracking   = track;
    m_SlewingAlt = m_SlewingAz = true;
    Track(0, 0);
    AUXCommand altcmd(MC_GOTO_SLOW, APP, ALT);
    AUXCommand azmcmd(MC_GOTO_SLOW, APP, AZM);
    altcmd.setPosition(alt);
    // N-based azimuth
    //    az += STEPS_PER_REVOLUTION / 2;
    //    az %= STEPS_PER_REVOLUTION;
    azmcmd.setPosition(az);
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
bool CelestronAUX::getVersion(AUXTargets trg)
{
    AUXCommand firmver(GET_VER, APP, trg);
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
    //getVersion(ANY);
    //getVersion(MB);
    getVersion(HC);
    getVersion(HCP);
    getVersion(AZM);
    getVersion(ALT);
    getVersion(GPS);
    getVersion(WiFi);
    getVersion(BAT);
    //getVersion(CHG);
    //getVersion(LIGHT);
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::setCordwrap(bool enable)
{

    AUXCommand cwcmd(enable ? MC_ENABLE_CORDWRAP : MC_DISABLE_CORDWRAP, APP, AZM);
    LOGF_INFO("setCordWrap before %d", m_CordWrapActive);
    sendAUXCommand(cwcmd);
    readAUXResponse(cwcmd);
    LOGF_INFO("setCordWrap after %d", m_CordWrapActive);
    return true;
};

bool CelestronAUX::getCordwrap()
{
    AUXCommand cwcmd(MC_POLL_CORDWRAP, APP, AZM);
    LOGF_INFO("getCordWrap before %d", m_CordWrapActive);
    sendAUXCommand(cwcmd);
    readAUXResponse(cwcmd);
    LOGF_INFO("getCordWrap after %d", m_CordWrapActive);
    return m_CordWrapActive;
};


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::setCordwrapPos(long pos)
{
    AUXCommand cwcmd(MC_SET_CORDWRAP_POS, APP, AZM);
    cwcmd.setPosition(pos);
    LOGF_INFO("setCordwrapPos %.1f deg", (pos / STEPS_PER_DEGREE) );
    sendAUXCommand(cwcmd);
    readAUXResponse(cwcmd);
    return true;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
long CelestronAUX::getCordwrapPos()
{
    AUXCommand cwcmd(MC_GET_CORDWRAP_POS, APP, AZM);
    sendAUXCommand(cwcmd);
    readAUXResponse(cwcmd);
    LOGF_INFO("getCordwrapPos %.1f deg", (m_CordWrapPosition / STEPS_PER_DEGREE) );
    return m_CordWrapPosition;
};

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::Track(long altRate, long azRate)
{
    // The scope rates are per minute?
    m_AltRate = altRate;
    m_AzRate  = azRate;
    if (m_SlewingAlt || m_SlewingAz)
    {
        m_AltRate = 0;
        m_AzRate  = 0;
    }
    m_Tracking = true;
    //fprintf(stderr,"Set tracking rates: ALT: %ld   AZM: %ld\n", AltRate, AzRate);
    AUXCommand altcmd((altRate < 0) ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, ALT);
    AUXCommand azmcmd((azRate < 0) ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, AZM);
    altcmd.setPosition(long(std::abs(m_AltRate)));
    azmcmd.setPosition(long(std::abs(m_AzRate)));

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
    querryStatus();
    //    if (TOUT_DEBUG)
    //    {
    //        if (debug_timeout < 0)
    //        {
    //            TOUT_DEBUG         = false;
    //            debug_timeout = 30;
    //        }
    //        else
    //            debug_timeout--;
    //    }

    if (isSimulation())
    {
        bool slewing = false;
        long da;
        int dir;

        // update both axis
        if (m_AltSteps != targetAlt)
        {
            da  = targetAlt - m_AltSteps;
            dir = (da > 0) ? 1 : -1;
            m_AltSteps += dir * std::max(std::min(std::abs(da) / 2, slewRate), 1L);
            slewing = true;
        }
        if (m_AzSteps != targetAz)
        {
            da  = targetAz - m_AzSteps;
            dir = (da > 0) ? 1 : -1;
            m_AzSteps += dir * std::max(std::min(long(std::abs(da) / 2), slewRate), 1L);
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
void CelestronAUX::querryStatus()
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

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronAUX::emulateGPS(AUXCommand &m)
{
    if (m.dst != GPS)
        return;

    DEBUGF(DBG_CAUX, "GPS: Got 0x%02x", m.cmd);
    if (gpsemu == false)
        return;

    switch (m.cmd)
    {
        case GET_VER:
        {
            DEBUGF(DBG_CAUX, "GPS: GET_VER from 0x%02x", m.src);
            AUXBuffer dat(2);
            dat[0] = 0x01;
            dat[1] = 0x00;
            AUXCommand cmd(GET_VER, GPS, m.src, dat);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
            break;
        }
        case GPS_GET_LAT:
        case GPS_GET_LONG:
        {
            DEBUGF(DBG_CAUX, "GPS: Sending LAT/LONG Lat:%f Lon:%f\n", LocationN[LOCATION_LATITUDE].value,
                   LocationN[LOCATION_LONGITUDE].value);
            AUXCommand cmd(m.cmd, GPS, m.src);
            if (m.cmd == GPS_GET_LAT)
                cmd.setPosition(LocationN[LOCATION_LATITUDE].value);
            else
                cmd.setPosition(LocationN[LOCATION_LONGITUDE].value);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
            break;
        }
        case GPS_GET_TIME:
        {
            DEBUGF(DBG_CAUX, "GPS: GET_TIME from 0x%02x", m.src);
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(3);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_hour);
            dat[1] = unsigned(ptm->tm_min);
            dat[2] = unsigned(ptm->tm_sec);
            AUXCommand cmd(GPS_GET_TIME, GPS, m.src, dat);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
            break;
        }
        case GPS_GET_DATE:
        {
            DEBUGF(DBG_CAUX, "GPS: GET_DATE from 0x%02x", m.src);
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_mon + 1);
            dat[1] = unsigned(ptm->tm_mday);
            AUXCommand cmd(GPS_GET_DATE, GPS, m.src, dat);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
            break;
        }
        case GPS_GET_YEAR:
        {
            DEBUGF(DBG_CAUX, "GPS: GET_YEAR from 0x%02x", m.src);
            time_t gmt;
            struct tm *ptm;
            AUXBuffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_year + 1900) >> 8;
            dat[1] = unsigned(ptm->tm_year + 1900) & 0xFF;
            DEBUGF(DBG_CAUX, "GPS: Sending: %d [%d,%d]", ptm->tm_year, dat[0], dat[1]);
            AUXCommand cmd(GPS_GET_YEAR, GPS, m.src, dat);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
            break;
        }
        case GPS_LINKED:
        {
            DEBUGF(DBG_CAUX, "GPS: LINKED from 0x%02x", m.src);
            AUXBuffer dat(1);

            dat[0] = unsigned(1);
            AUXCommand cmd(GPS_LINKED, GPS, m.src, dat);
            sendAUXCommand(cmd);
            readAUXResponse(cmd);
            break;
        }
        default:
            DEBUGF(DBG_CAUX, "GPS: Got 0x%02x", m.cmd);
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronAUX::processResponse(AUXCommand &m)
{
    m.logResponse();

    if (m.dst == GPS)
        emulateGPS(m);
    else
        switch (m.cmd)
        {
            case MC_GET_POSITION:
                switch (m.src)
                {
                    case ALT:
                        m_AltSteps = m.getPosition();
                        LOGF_DEBUG("ALT: %ld", m_AltSteps);
                        // if (PROC_DEBUG) IDLog("ALT: %ld", Alt);
                        break;
                    case AZM:
                        m_AzSteps = m.getPosition();
                        // Celestron uses N as zero Azimuth!
                        //                        m_AzSteps += STEPS_PER_REVOLUTION / 2;
                        //                        m_AzSteps %= STEPS_PER_REVOLUTION;
                        LOGF_DEBUG("AZ: %ld", m_AzSteps);
                        break;
                    default:
                        break;
                }
                break;
            case MC_SLEW_DONE:
                switch (m.src)
                {
                    case ALT:
                        m_SlewingAlt = m.data[0] != 0xff;
                        // if (PROC_DEBUG) IDLog("ALT %d ", slewingAlt);
                        break;
                    case AZM:
                        m_SlewingAz = m.data[0] != 0xff;
                        // if (PROC_DEBUG) IDLog("AZM %d ", slewingAz);
                        break;
                    default:
                        break;
                }
                break;
            case MC_POLL_CORDWRAP:
                if (m.src == AZM)
                    m_CordWrapActive = m.data[0] == 0xff;
                break;
            case MC_GET_CORDWRAP_POS:
                if (m.src == AZM)
                {
                    m_CordWrapPosition = m.getPosition();
                    LOGF_DEBUG("Got cordwrap position %.1f", float(m_CordWrapPosition) / STEPS_PER_DEGREE);
                }
                break;

            case MC_GET_AUTOGUIDE_RATE:
                switch (m.src)
                {
                    case ALT:
                        return HandleGetAutoguideRate(AXIS_DE, m.data[0] * 100.0 / 255);
                    case AZM:
                        return HandleGetAutoguideRate(AXIS_RA, m.data[0] * 100.0 / 255);
                    default:
                        IDLog("unknown src 0x%02x\n", m.src);
                }
                break;

            case MC_SET_AUTOGUIDE_RATE:
                switch (m.src)
                {
                    case ALT:
                        return HandleSetAutoguideRate(AXIS_DE);
                    case AZM:
                        return HandleSetAutoguideRate(AXIS_RA);
                    default:
                        IDLog("unknown src 0x%02x\n", m.src);
                }
                break;

            case MC_AUX_GUIDE:
                switch (m.src)
                {
                    case ALT:
                        return HandleGuidePulse(AXIS_DE);
                    case AZM:
                        return HandleGuidePulse(AXIS_RA);
                    default:
                        IDLog("unknown src 0x%02x\n", m.src);
                }
                break;

            case MC_AUX_GUIDE_ACTIVE:
                switch (m.src)
                {
                    case ALT:
                        return HandleGuidePulseDone(AXIS_DE, m.data[0] == 0x00);
                    case AZM:
                        return HandleGuidePulseDone(AXIS_RA, m.data[0] == 0x00);
                    default:
                        IDLog("unknown src 0x%02x\n", m.src);
                }
                break;

            case GET_VER:
                if (m.src != APP)
                    LOGF_INFO("Got GET_VERSION response from %s: %d.%d.%d ", m.node_name(m.src), m.data[0], m.data[1],
                              256 * m.data[2] + m.data[3]);
                switch (m.src)
                {
                    case MB:
                        m_MainBoardVersionMajor = m.data[0];
                        m_MainBoardVersionMinor = m.data[1];
                        break;
                    case ALT:
                        m_AltitudeVersionMajor = m.data[0];
                        m_AltitudeVersionMinor = m.data[1];
                        break;
                    case AZM:
                        m_AzimuthVersionMajor = m.data[0];
                        m_AzimuthVersionMinor = m.data[1];
                        break;
                    case APP:
                        LOGF_DEBUG("Got echo of GET_VERSION from %s", m.node_name(m.dst));
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;

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

    if (isRTSCTS || !isHC)
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
        buf[2] = c.dst;
        buf[3] = c.src;
        buf[4] = c.cmd;

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
    unsigned char buf[BUFFER_SIZE];
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

        msleep(50);
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

    if (isRTSCTS || !isHC || getActiveConnection() != serialConnection)
        // Direct connection (AUX/PC/USB port)
        c.fillBuf(buf);
    else
    {
        // connection is through HC serial and destination is not HC,
        // convert AUX command to a passthrough command
        buf.resize(8);                 // fixed len = 8
        buf[0] = 0x50;                 // prefix
        buf[1] = 1 + c.data.size();    // length
        buf[2] = c.dst;                // destination
        buf[3] = c.cmd;                // command id
        for (unsigned int i = 0; i < c.data.size(); i++) // payload
        {
            buf[i + 4] = c.data[i];
        }
        buf[7] = response_data_size = c.response_data_size();
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
    if (isRTSCTS)
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
    if (isRTSCTS)
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
    if (isRTSCTS)
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

