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

#define BUFFER_SIZE 10240
//int MAX_CMD_LEN = 32;

#define READ_TIMEOUT 1 		// s
#define CTS_TIMEOUT 100		// ms
#define RTS_DELAY 50		// ms

bool TOUT_DEBUG = false;
bool GPS_DEBUG = false;
bool RD_DEBUG  = false;
bool WR_DEBUG  = false;
bool SEND_DEBUG  = false;
bool PROC_DEBUG  = false;
bool SERIAL_DEBUG  = false;

using namespace INDI::AlignmentSubsystem;

#define MAX_SLEW_RATE       9
#define FIND_SLEW_RATE      7
#define CENTERING_SLEW_RATE 3
#define GUIDE_SLEW_RATE     2

#define MOUNTINFO_TAB "Mount info"

// We declare an auto pointer to CelestronAUX.
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
    telescope_caux->ISSnoopDevice(root);
}

// One definition rule (ODR) constants
// AUX commands use 24bit integer as a representation of angle in units of
// fractional revolutions. Thus 2^24 steps makes full revolution.
const long   CelestronAUX::STEPS_PER_REVOLUTION = 16777216;
const double CelestronAUX::STEPS_PER_DEGREE     = STEPS_PER_REVOLUTION / 360.0;
const double CelestronAUX::DEFAULT_SLEW_RATE    = STEPS_PER_DEGREE * 2.0;
const long   CelestronAUX::MAX_ALT              =  90.0 * STEPS_PER_DEGREE;
const long   CelestronAUX::MIN_ALT              = -90.0 * STEPS_PER_DEGREE;

// The guide rate is probably (???) measured in 1000 arcmin/min
// This is based on experimentation and guesswork.
// The rate is calculated in steps/min - thus conversion is required.
// The best experimental value was 1.315 which is quite close
// to 60000/STEPS_PER_DEGREE = 1.2874603271484375.
const double CelestronAUX::TRACK_SCALE = 60000 / STEPS_PER_DEGREE;

CelestronAUX::CelestronAUX()
    : ScopeStatus(IDLE), AxisStatusALT(STOPPED), AxisDirectionALT(FORWARD), AxisStatusAZ(STOPPED),
      AxisDirectionAZ(FORWARD), TraceThisTickCount(0), TraceThisTick(false),
      DBG_CAUX(INDI::Logger::DBG_SESSION),
      DBG_AUXMOUNT(INDI::Logger::getInstance().addDebugLevel("Celestron AUX Verbose", "CAUX"))
{
    setVersion(CAUX_VERSION_MAJOR, CAUX_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION,
                           4);

    LOG_INFO("Celestron AUX instancing");

    //Both communication available, Serial and network (tcp/ip).
    setTelescopeConnection(CONNECTION_TCP | CONNECTION_SERIAL);

    // Approach from no further then degs away
    Approach = 1.0;

    // Max ticks before we reissue the goto to update position
    maxSlewTicks = 15;
    cordwrap = false;
    cordwrapPos = 0;
    gpsemu = false;
    mb_ver_maj = mb_ver_min = 0;
    alt_ver_maj = alt_ver_min = 0;
    azm_ver_maj = azm_ver_min = 0;
}

CelestronAUX::~CelestronAUX()
{
}


// Private methods

bool CelestronAUX::Abort()
{
    if (MovementNSSP.s == IPS_BUSY)
    {
        IUResetSwitch(&MovementNSSP);
        MovementNSSP.s = IPS_IDLE;
        IDSetSwitch(&MovementNSSP, nullptr);
    }

    if (MovementWESP.s == IPS_BUSY)
    {
        MovementWESP.s = IPS_IDLE;
        IUResetSwitch(&MovementWESP);
        IDSetSwitch(&MovementWESP, nullptr);
    }

    if (EqNP.s == IPS_BUSY)
    {
        EqNP.s = IPS_IDLE;
        IDSetNumber(&EqNP, nullptr);
    }

    TrackState = SCOPE_IDLE;

    AxisStatusAZ = AxisStatusALT = STOPPED;
    ScopeStatus                  = IDLE;

    Track(0, 0);
    buffer b(1);
    b[0] = 0;
    AUXCommand stopAlt(MC_MOVE_POS, APP, ALT, b);
    AUXCommand stopAz(MC_MOVE_POS, APP, AZM, b);
    sendCmd(stopAlt);
    sendCmd(stopAz);

    AbortSP.s = IPS_OK;
    IUResetSwitch(&AbortSP);
    IDSetSwitch(&AbortSP, nullptr);
    LOG_INFO("Telescope movement aborted.");

    return true;
}

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


bool CelestronAUX::detectNetScope()
{
    return detectNetScope(false);
}

bool CelestronAUX::Handshake()
{
    LOGF_DEBUG("CAUX: connect %d (%s)", PortFD, (getActiveConnection() == serialConnection) ? "serial" : "net");
    if (PortFD > 0)
    {
        if (getActiveConnection() == serialConnection)
        {
            if (SERIAL_DEBUG)
                LOGF_DEBUG("detectRTSCTS = %s.",  detectRTSCTS() ? "true" : "false");

            // if serial connection, check if hardware control flow is required.
            // yes for AUX and PC ports, no for HC port.
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
                LOG_INFO("Detected Hand Controller serial connection.");
                serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
                if (!tty_set_speed(PortFD, B9600))
                {
                    LOG_ERROR("Cannot set serial speed to 9600 baud.");
                    return false;
                }

                LOG_INFO("Setting serial speed to 9600 baud.");
            }
        }
        else
        {
            LOG_INFO("Wait for mount connection to settle.");
            msleep(1000);
            return true;
        }

        // read firmware version, if read ok, detected scope
        LOG_INFO("Trying to contact telescope motor controllers.");
        if (getVersion(AZM) && getVersion(ALT))
        {
            LOG_INFO("Got response from target ALT or AZM. Probing all targets.");
            getVersions();
        }
        else
        {
            LOG_ERROR("Got no response from target ALT or AZM.");
            LOG_ERROR("Cannot continue without connection to motor controllers.");
            return false;
        }

        LOG_INFO("Connection ready. Starting Processing.");
        return true;
    }
    else
    {
        return false ;
    }

}

bool CelestronAUX::Disconnect()
{
    Abort();
    return INDI::Telescope::Disconnect();
}

const char *CelestronAUX::getDefaultName()
{
    return "Celestron AUX";
}

bool CelestronAUX::Park()
{
    // Park at the northern horizon
    // This is a designated by celestron parking position
    Abort();
    TrackState = SCOPE_PARKING;
    ParkSP.s   = IPS_BUSY;
    IDSetSwitch(&ParkSP, nullptr);
    LOG_INFO("Telescope park in progress...");
    GoToFast(long(0), STEPS_PER_REVOLUTION / 2, false);
    return true;
}

bool CelestronAUX::UnPark()
{
    SetParked(false);
    return true;
}

ln_hrz_posn CelestronAUX::AltAzFromRaDec(double ra, double dec, double ts)
{
    // Call the alignment subsystem to translate the celestial reference frame coordinate
    // into a telescope reference frame coordinate
    TelescopeDirectionVector TDV;
    ln_hrz_posn AltAz;

    if (TransformCelestialToTelescope(ra, dec, ts, TDV))
    {
        // The alignment subsystem has successfully transformed my coordinate
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    }
    else
    {
        // The alignment subsystem cannot transform the coordinate.
        // Try some simple rotations using the stored observatory position if any
        bool HavePosition = false;
        ln_lnlat_posn Position;
        if ((nullptr != IUFindNumber(&LocationNP, "LAT")) && (0 != IUFindNumber(&LocationNP, "LAT")->value) &&
                (nullptr != IUFindNumber(&LocationNP, "LONG")) && (0 != IUFindNumber(&LocationNP, "LONG")->value))
        {
            // I assume that being on the equator and exactly on the prime meridian is unlikely
            Position.lat = IUFindNumber(&LocationNP, "LAT")->value;
            Position.lng = IUFindNumber(&LocationNP, "LONG")->value;
            HavePosition = true;
        }
        struct ln_equ_posn EquatorialCoordinates;
        // libnova works in decimal degrees
        EquatorialCoordinates.ra  = ra * 360.0 / 24.0;
        EquatorialCoordinates.dec = dec;
        if (HavePosition)
        {
            ln_get_hrz_from_equ(&EquatorialCoordinates, &Position, ln_get_julian_from_sys() + ts, &AltAz);
            TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    break;

                case NORTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated anticlockwise
                    TDV.RotateAroundY(Position.lat - 90.0);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated clockwise
                    TDV.RotateAroundY(Position.lat + 90.0);
                    break;
            }
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        }
        else
        {
            // The best I can do is just do a direct conversion to Alt/Az
            TDV = TelescopeDirectionVectorFromEquatorialCoordinates(EquatorialCoordinates);
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        }
    }
    return AltAz;
}


// TODO: Make adjustment for the approx time it takes to slew to the given pos.
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
        LOG_INFO("Goto - tracking requested");
    }

    GoToTarget.ra  = ra;
    GoToTarget.dec = dec;

    double timeshift = 0.0;
    if (ScopeStatus != APPROACH)
    {
        // The scope is not in slow approach mode - target should be modified
        // for precission approach. We go to the position from some time ago,
        // to keep the motors going in the same direction as in tracking
        timeshift = 3.0 / (24.0 * 60.0); // Three minutes worth of tracking
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
    if (AltAz.az < 0)
        AltAz.az += 360.0;
    if (AltAz.az > 360.0)
        AltAz.az -= 360.0;
    // AltAz.az = fmod(AltAz.az, 360.0);

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

    //EqNP.s = IPS_BUSY;

    return true;
}

bool CelestronAUX::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    // Switch to slew rate switch name as defined in telescope_simulator
    // IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "SLEWMODE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    TrackState = SCOPE_IDLE;

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    /* Add alignment properties */
    InitAlignmentProperties(this);

    // Default connection options
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    tcpConnection->setDefaultHost(CAUX_DEFAULT_IP);
    tcpConnection->setDefaultPort(CAUX_DEFAULT_PORT);

    // Firmware
    IUFillText(&FirmwareT[FW_HC], "HC version", "", nullptr);
    IUFillText(&FirmwareT[FW_HCp], "HC+ version", "", nullptr);
    IUFillText(&FirmwareT[FW_AZM], "Ra/AZM version", "", nullptr);
    IUFillText(&FirmwareT[FW_ALT], "Dec/ALT version", "", nullptr);
    IUFillText(&FirmwareT[FW_WiFi], "WiFi version", "", nullptr);
    IUFillText(&FirmwareT[FW_BAT], "Battery version", "", nullptr);
    IUFillText(&FirmwareT[FW_CHG], "Charger version", "", nullptr);
    IUFillText(&FirmwareT[FW_LIGHT], "Ligts version", "", nullptr);
    IUFillText(&FirmwareT[FW_GPS], "GPS version", "", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 9, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    IUFillSwitch(&CordWrapS[CORDWRAP_OFF], "CORDWRAP_OFF", "OFF", ISS_OFF);
    IUFillSwitch(&CordWrapS[CORDWRAP_ON], "CORDWRAP_ON", "ON", ISS_ON);
    IUFillSwitchVector(&CordWrapSP, CordWrapS, 2, getDeviceName(), "CORDWRAP", "Cord Wrap", MOTION_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    IUFillSwitch(&CWPosS[CORDWRAP_N], "CORDWRAP_N", "North", ISS_ON);
    IUFillSwitch(&CWPosS[CORDWRAP_E], "CORDWRAP_E", "East",  ISS_OFF);
    IUFillSwitch(&CWPosS[CORDWRAP_S], "CORDWRAP_S", "South", ISS_OFF);
    IUFillSwitch(&CWPosS[CORDWRAP_W], "CORDWRAP_W", "West",  ISS_OFF);
    IUFillSwitchVector(&CWPosSP, CWPosS, 4, getDeviceName(), "CORDWRAP_POS", "CW Position", MOTION_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    IUFillSwitch(&GPSEmuS[GPSEMU_OFF], "GPSEMU_OFF", "OFF", ISS_OFF);
    IUFillSwitch(&GPSEmuS[GPSEMU_ON], "GPSEMU_ON", "ON", ISS_ON);
    IUFillSwitchVector(&GPSEmuSP, GPSEmuS, 2, getDeviceName(), "GPSEMU", "GPS Emu", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    IUFillSwitch(&NetDetectS[ISS_OFF], "ISS_OFF", "Detect", ISS_OFF);
    IUFillSwitchVector(&NetDetectSP, NetDetectS, 1, getDeviceName(), "NETDETECT", "Network scope", CONNECTION_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);
    return true;
}

bool CelestronAUX::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&CordWrapSP);
        getCordwrap();
        IUResetSwitch(&CordWrapSP);
        CordWrapS[cordwrap].s = ISS_ON;
        IDSetSwitch(&CordWrapSP, nullptr);

        defineSwitch(&CWPosSP);
        getCordwrapPos();
        IUResetSwitch(&CWPosSP);
        CordWrapS[int(cordwrapPos % 90)].s = ISS_ON;
        IDSetSwitch(&CWPosSP, nullptr);

        defineSwitch(&GPSEmuSP);
        IUResetSwitch(&GPSEmuSP);
        GPSEmuS[gpsemu].s = ISS_ON;
        IDSetSwitch(&GPSEmuSP, nullptr);

        IUSaveText(&FirmwareT[FW_HC], "HC version");
        IUSaveText(&FirmwareT[FW_HCp], "HC+ version");
        IUSaveText(&FirmwareT[FW_AZM], "Ra/AZM version");
        IUSaveText(&FirmwareT[FW_ALT], "Dec/ALT version");
        IUSaveText(&FirmwareT[FW_WiFi], "WiFi version");
        IUSaveText(&FirmwareT[FW_BAT], "Battery version");
        IUSaveText(&FirmwareT[FW_CHG], "Charger version");
        IUSaveText(&FirmwareT[FW_LIGHT], "Ligts version");
        IUSaveText(&FirmwareT[FW_GPS], "GPS version");
        defineText(&FirmwareTP);
    }
    else
    {
        deleteProperty(CordWrapSP.name);
        deleteProperty(CWPosSP.name);
        deleteProperty(GPSEmuSP.name);
        deleteProperty(FirmwareTP.name);
    }
    return true;
}

bool CelestronAUX::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    SaveAlignmentConfigProperties(fp);

    IUSaveConfigSwitch(fp, &CordWrapSP);
    IUSaveConfigSwitch(fp, &CWPosSP);
    IUSaveConfigSwitch(fp, &GPSEmuSP);
    return true;
}

void CelestronAUX::ISGetProperties(const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
    }

    defineSwitch(&NetDetectSP);
    IUResetSwitch(&NetDetectSP);
    IDSetSwitch(&NetDetectSP, nullptr);

    return;
}

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

bool CelestronAUX::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Process alignment properties
        ProcessAlignmentNumberProperties(this, name, values, names, n);
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool CelestronAUX::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
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
            int CWIndex = IUFindOnSwitchIndex(&CordWrapSP);
            IUUpdateSwitch(&CordWrapSP, states, names, n);
            CWIndex = IUFindOnSwitchIndex(&CordWrapSP);
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
            DEBUGF(DBG_CAUX, "CordWrap Position is now %s (%d)", CWPosS[CWIndex].label, CWIndex);
            CWPosSP.s = IPS_OK;
            IDSetSwitch(&CWPosSP, nullptr);
            long cwpos = 0;
            switch (CWIndex)
            {
                case CORDWRAP_N:
                    cwpos = 0;
                    break;
                case CORDWRAP_E:
                    cwpos = 90 * STEPS_PER_DEGREE;
                    break;
                case CORDWRAP_S:
                    cwpos = 180 * STEPS_PER_DEGREE;
                    break;
                case CORDWRAP_W:
                    cwpos = 270 * STEPS_PER_DEGREE;
                    break;
                default:
                    cwpos = 0;
                    break;
            }
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

bool CelestronAUX::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP);
    DEBUGF(DBG_CAUX, "MoveNS dir:%d, cmd:%d, rate:%d", dir, command, rate);
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

bool CelestronAUX::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    int rate = IUFindOnSwitchIndex(&SlewRateSP);
    DEBUGF(DBG_CAUX, "MoveWE dir:%d, cmd:%d, rate:%d", dir, command, rate);
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

bool CelestronAUX::trackingRequested()
{
    return (ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s);
}

bool CelestronAUX::ReadScopeStatus()
{
    struct ln_hrz_posn AltAz;
    double RightAscension, Declination;

    AltAz.alt = double(GetALT()) / STEPS_PER_DEGREE;
    // libnova indexes Az from south while Celestron controllers index from north
    // Never mix two controllers/drivers they will never agree perfectly.
    // Furthermore the celestron hand controler resets the position encoders
    // on alignment and this will mess-up all orientation in the driver.
    // Here we are not attempting to make the driver agree with the hand
    // controller (That would involve adding 180deg here to the azimuth -
    // this way the celestron nexstar driver and this would agree in some
    // situations but not in other - better not to attepmpt impossible!).
    AltAz.az                     = double(GetAZ()) / STEPS_PER_DEGREE;
    TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);

    if (TraceThisTick)
        DEBUGF(DBG_CAUX, "ReadScopeStatus - Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);

    if (!TransformTelescopeToCelestial(TDV, RightAscension, Declination))
    {
        if (TraceThisTick)
            DEBUG(DBG_CAUX, "ReadScopeStatus - TransformTelescopeToCelestial failed");

        bool HavePosition = false;
        ln_lnlat_posn Position;
        if ((nullptr != IUFindNumber(&LocationNP, "LAT")) && (0 != IUFindNumber(&LocationNP, "LAT")->value) &&
                (nullptr != IUFindNumber(&LocationNP, "LONG")) && (0 != IUFindNumber(&LocationNP, "LONG")->value))
        {
            // I assume that being on the equator and exactly on the prime meridian is unlikely
            Position.lat = IUFindNumber(&LocationNP, "LAT")->value;
            Position.lng = IUFindNumber(&LocationNP, "LONG")->value;
            HavePosition = true;
        }
        struct ln_equ_posn EquatorialCoordinates;
        if (HavePosition)
        {
            if (TraceThisTick)
                DEBUG(DBG_CAUX, "ReadScopeStatus - HavePosition true");
            TelescopeDirectionVector RotatedTDV(TDV);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    if (TraceThisTick)
                        DEBUG(DBG_CAUX, "ReadScopeStatus - ApproximateMountAlignment ZENITH");
                    break;

                case NORTH_CELESTIAL_POLE:
                    if (TraceThisTick)
                        DEBUG(DBG_CAUX, "ReadScopeStatus - ApproximateMountAlignment NORTH_CELESTIAL_POLE");
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated clockwise
                    RotatedTDV.RotateAroundY(90.0 - Position.lat);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    if (TraceThisTick)
                        DEBUG(DBG_CAUX, "ReadScopeStatus - ApproximateMountAlignment SOUTH_CELESTIAL_POLE");
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                    RotatedTDV.RotateAroundY(-90.0 - Position.lat);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;
            }
            if (TraceThisTick)
                DEBUGF(DBG_CAUX, "After rotations: Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);

            ln_get_equ_from_hrz(&AltAz, &Position, ln_get_julian_from_sys(), &EquatorialCoordinates);
        }
        else
        {
            if (TraceThisTick)
                DEBUG(DBG_CAUX, "ReadScopeStatus - HavePosition false");

            // The best I can do is just do a direct conversion to RA/DEC
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV, EquatorialCoordinates);
        }
        // libnova works in decimal degrees
        RightAscension = EquatorialCoordinates.ra * 24.0 / 360.0;
        Declination    = EquatorialCoordinates.dec;
    }

    if (TraceThisTick)
        DEBUGF(DBG_CAUX, "ReadScopeStatus - RA %lf hours DEC %lf degrees", RightAscension, Declination);

    // In case we are slewing while tracking update the potential target
    NewTrackingTarget.ra  = RightAscension;
    NewTrackingTarget.dec = Declination;
    NewRaDec(RightAscension, Declination);

    return true;
}

bool CelestronAUX::Sync(double ra, double dec)
{
    struct ln_hrz_posn AltAz;
    AltAz.alt = double(GetALT()) / STEPS_PER_DEGREE;
    AltAz.az  = double(GetAZ()) / STEPS_PER_DEGREE;

    AlignmentDatabaseEntry NewEntry;
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension        = ra;
    NewEntry.Declination           = dec;
    NewEntry.TelescopeDirection    = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    NewEntry.PrivateDataSize       = 0;

    DEBUGF(DBG_CAUX, "Sync - Celestial reference frame target right ascension %lf(%lf) declination %lf",
           ra * 360.0 / 24.0, ra, dec);

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);

        // Tell the client about size change
        UpdateSize();

        // Tell the math plugin to reinitialise
        Initialise(this);
        DEBUGF(DBG_CAUX, "Sync - new entry added RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);
        ReadScopeStatus();
        return true;
    }
    DEBUGF(DBG_CAUX, "Sync - duplicate entry RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);
    return false;
}

void CelestronAUX::TimerHit()
{
    TraceThisTickCount++;
    if (60 == TraceThisTickCount)
    {
        TraceThisTick      = true;
        TraceThisTickCount = 0;
    }
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

    INDI::Telescope::TimerHit(); // This will call ReadScopeStatus

    // OK I have updated the celestial reference frame RA/DEC in ReadScopeStatus
    // Now handle the tracking state
    switch (TrackState)
    {
        case SCOPE_PARKING:
            if (!slewing())
            {
                SetParked(true);
                DEBUG(DBG_CAUX, "Telescope parked.");
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
                    DEBUGF(DBG_CAUX, "Goto finished start tracking TargetRA: %f TargetDEC: %f",
                           CurrentTrackingTarget.ra, CurrentTrackingTarget.dec);
                    TrackState = SCOPE_TRACKING;
                    // Fall through to tracking case
                }
                else
                {
                    DEBUG(DBG_CAUX, "Goto finished. No tracking requested");
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
                DEBUGF(DBG_CAUX, "Tracking - Calculated Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);
            /*
            TODO
            The tracking should take into account movement of the scope
            by the hand controller (we can hear the commands)
            and the movements made by the joystick.
            Right now when we move the scope by HC it returns to the
            designated target by corrective tracking.
            */

            // Fold Azimuth into 0-360
            if (AltAz.az < 0)
                AltAz.az += 360.0;
            if (AltAz.az > 360.0)
                AltAz.az -= 360.0;
            //AltAz.az = fmod(AltAz.az, 360.0);

            {
                long altRate, azRate;


                // This is in steps per minute
                altRate = long(AltAz.alt * STEPS_PER_DEGREE - GetALT());
                azRate  = long(AltAz.az * STEPS_PER_DEGREE - GetAZ());

                if (TraceThisTick)
                    DEBUGF(DBG_CAUX, "Target (AltAz): %f  %f  Scope  (AltAz)  %f  %f", AltAz.alt, AltAz.az,
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
                    DEBUGF(DBG_CAUX, "TimerHit - Tracking AltRate %d AzRate %d ; Pos diff (deg): Alt: %f Az: %f",
                           altRate, azRate, AltAz.alt - AAzero.alt, anglediff(AltAz.az, AAzero.az));
            }
            break;
        }

        default:
            break;
    }

    TraceThisTick = false;
}

bool CelestronAUX::updateLocation(double latitude, double longitude, double elevation)
{
    UpdateLocation(latitude, longitude, elevation);
    Lat = latitude;
    Lon = longitude;
    Elv = elevation;
    return true;
}


long CelestronAUX::GetALT()
{
    // return alt encoder adjusted to -90...90
    if (Alt > STEPS_PER_REVOLUTION / 2)
    {
        return Alt - STEPS_PER_REVOLUTION;
    }
    else
    {
        return Alt;
    }
};

long CelestronAUX::GetAZ()
{
    return Az % STEPS_PER_REVOLUTION;
};

bool CelestronAUX::slewing()
{
    return slewingAlt || slewingAz;
}

bool CelestronAUX::Slew(AUXtargets trg, int rate)
{
    AUXCommand cmd((rate < 0) ? MC_MOVE_NEG : MC_MOVE_POS, APP, trg);
    cmd.setRate((unsigned char)(std::abs(rate) & 0xFF));

    sendCmd(cmd);
    readMsgs(cmd);
    return true;
}

bool CelestronAUX::SlewALT(int rate)
{
    slewingAlt = (rate != 0);
    return Slew(ALT, rate);
}

bool CelestronAUX::SlewAZ(int rate)
{
    slewingAz = (rate != 0);
    return Slew(AZM, rate);
}

bool CelestronAUX::GoToFast(long alt, long az, bool track)
{
    //DEBUG=true;
    targetAlt  = alt;
    targetAz   = az;
    tracking   = track;
    slewingAlt = slewingAz = true;
    Track(0, 0);
    AUXCommand altcmd(MC_GOTO_FAST, APP, ALT);
    AUXCommand azmcmd(MC_GOTO_FAST, APP, AZM);
    altcmd.setPosition(alt);
    // N-based azimuth
    az += STEPS_PER_REVOLUTION / 2;
    az %= STEPS_PER_REVOLUTION;
    azmcmd.setPosition(az);
    sendCmd(altcmd);
    readMsgs(altcmd);
    sendCmd(azmcmd);
    readMsgs(azmcmd);
    //DEBUG=false;
    return true;
};


bool CelestronAUX::GoToSlow(long alt, long az, bool track)
{
    //DEBUG=false;
    targetAlt  = alt;
    targetAz   = az;
    tracking   = track;
    slewingAlt = slewingAz = true;
    Track(0, 0);
    AUXCommand altcmd(MC_GOTO_SLOW, APP, ALT);
    AUXCommand azmcmd(MC_GOTO_SLOW, APP, AZM);
    altcmd.setPosition(alt);
    // N-based azimuth
    az += STEPS_PER_REVOLUTION / 2;
    az %= STEPS_PER_REVOLUTION;
    azmcmd.setPosition(az);
    sendCmd(altcmd);
    readMsgs(altcmd);
    sendCmd(azmcmd);
    readMsgs(azmcmd);
    //DEBUG=false;
    return true;
};

bool CelestronAUX::getVersion(AUXtargets trg)
{
    AUXCommand firmver(GET_VER, APP, trg);
    if (! sendCmd(firmver))
        return false;
    if (! readMsgs(firmver))
        return false;
    return true;
};

void CelestronAUX::getVersions()
{
    getVersion(ANY);
    getVersion(MB);
    getVersion(HC);
    getVersion(HCP);
    getVersion(AZM);
    getVersion(ALT);
    getVersion(GPS);
    getVersion(WiFi);
    getVersion(BAT);
    getVersion(CHG);
    getVersion(LIGHT);
};


bool CelestronAUX::setCordwrap(bool enable)
{

    AUXCommand cwcmd((enable) ? MC_ENABLE_CORDWRAP : MC_DISABLE_CORDWRAP, APP, AZM);
    LOGF_INFO("setCordWrap before %d", cordwrap);
    sendCmd(cwcmd);
    readMsgs(cwcmd);
    LOGF_INFO("setCordWrap after %d", cordwrap);
    return true;
};

bool CelestronAUX::getCordwrap()
{
    AUXCommand cwcmd(MC_POLL_CORDWRAP, APP, AZM);
    LOGF_INFO("getCordWrap before %d", cordwrap);
    sendCmd(cwcmd);
    readMsgs(cwcmd);
    LOGF_INFO("getCordWrap after %d", cordwrap);
    return cordwrap;
};

bool CelestronAUX::setCordwrapPos(long pos)
{
    AUXCommand cwcmd(MC_SET_CORDWRAP_POS, APP, AZM);
    cwcmd.setPosition(pos);
    sendCmd(cwcmd);
    readMsgs(cwcmd);
    return true;
};

long CelestronAUX::getCordwrapPos()
{
    AUXCommand cwcmd(MC_GET_CORDWRAP_POS, APP, AZM);
    sendCmd(cwcmd);
    readMsgs(cwcmd);
    return cordwrapPos;
};


bool CelestronAUX::Track(long altRate, long azRate)
{
    // The scope rates are per minute?
    AltRate = altRate;
    AzRate  = azRate;
    if (slewingAlt || slewingAz)
    {
        AltRate = 0;
        AzRate  = 0;
    }
    tracking = true;
    //fprintf(stderr,"Set tracking rates: ALT: %ld   AZM: %ld\n", AltRate, AzRate);
    AUXCommand altcmd((altRate < 0) ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, ALT);
    AUXCommand azmcmd((azRate < 0) ? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE, APP, AZM);
    altcmd.setPosition(long(std::abs(AltRate)));
    azmcmd.setPosition(long(std::abs(AzRate)));

    sendCmd(altcmd);
    readMsgs(altcmd);
    sendCmd(azmcmd);
    readMsgs(azmcmd);
    return true;
};

int debug_timeout = 30;


bool CelestronAUX::TimerTick(double dt)
{
    querryStatus();
    if (TOUT_DEBUG)
    {
        if (debug_timeout < 0)
        {
            TOUT_DEBUG         = false;
            debug_timeout = 30;
        }
        else
            debug_timeout--;
    }

    if (simulator)
    {
        bool slewing = false;
        long da;
        int dir;

        // update both axis
        if (Alt != targetAlt)
        {
            da  = targetAlt - Alt;
            dir = (da > 0) ? 1 : -1;
            Alt += dir * std::max(std::min(long(std::abs(da) / 2), slewRate), 1L);
            slewing = true;
        }
        if (Az != targetAz)
        {
            da  = targetAz - Az;
            dir = (da > 0) ? 1 : -1;
            Az += dir * std::max(std::min(long(std::abs(da) / 2), slewRate), 1L);
            slewing = true;
        }

        // if we reach the target at previous tick start tracking if tracking requested
        if (tracking && !slewing && Alt == targetAlt && Az == targetAz)
        {
            targetAlt = (Alt += AltRate * dt);
            targetAz  = (Az += AzRate * dt);
        }
    }
    return true;
};


void CelestronAUX::querryStatus()
{
    AUXtargets trg[2] = { ALT, AZM };
    for (int i = 0; i < 2; i++)
    {
        AUXCommand cmd(MC_GET_POSITION, APP, trg[i]);
        sendCmd(cmd);
        readMsgs(cmd);
    }
    if (slewingAlt)
    {
        AUXCommand cmd(MC_SLEW_DONE, APP, ALT);
        sendCmd(cmd);
        readMsgs(cmd);
    }
    if (slewingAz)
    {
        AUXCommand cmd(MC_SLEW_DONE, APP, AZM);
        sendCmd(cmd);
        readMsgs(cmd);
    }
}

void CelestronAUX::emulateGPS(AUXCommand &m)
{
    if (m.dst != GPS)
        return;
    if (GPS_DEBUG)
        IDLog("Got 0x%02x for GPS\n", m.cmd);
    if (gpsemu == false)
        return;

    switch (m.cmd)
    {
        case GET_VER:
        {
            if (GPS_DEBUG)
                IDLog("GPS: GET_VER from 0x%02x\n", m.src);
            buffer dat(2);
            dat[0] = 0x01;
            dat[1] = 0x00;
            AUXCommand cmd(GET_VER, GPS, m.src, dat);
            sendCmd(cmd);
            //readMsgs();
            break;
        }
        case GPS_GET_LAT:
        case GPS_GET_LONG:
        {
            if (GPS_DEBUG)
                IDLog("GPS: Sending LAT/LONG Lat:%f Lon:%f\n", Lat, Lon);
            AUXCommand cmd(m.cmd, GPS, m.src);
            if (m.cmd == GPS_GET_LAT)
                cmd.setPosition(Lat);
            else
                cmd.setPosition(Lon);
            sendCmd(cmd);
            //readMsgs();
            break;
        }
        case GPS_GET_TIME:
        {
            if (GPS_DEBUG)
                IDLog("GPS: GET_TIME from 0x%02x\n", m.src);
            time_t gmt;
            struct tm *ptm;
            buffer dat(3);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_hour);
            dat[1] = unsigned(ptm->tm_min);
            dat[2] = unsigned(ptm->tm_sec);
            AUXCommand cmd(GPS_GET_TIME, GPS, m.src, dat);
            sendCmd(cmd);
            //readMsgs();
            break;
        }
        case GPS_GET_DATE:
        {
            if (GPS_DEBUG)
                IDLog("GPS: GET_DATE from 0x%02x\n", m.src);
            time_t gmt;
            struct tm *ptm;
            buffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_mon + 1);
            dat[1] = unsigned(ptm->tm_mday);
            AUXCommand cmd(GPS_GET_DATE, GPS, m.src, dat);
            sendCmd(cmd);
            //readMsgs();
            break;
        }
        case GPS_GET_YEAR:
        {
            if (GPS_DEBUG)
                IDLog("GPS: GET_YEAR from 0x%02x", m.src);
            time_t gmt;
            struct tm *ptm;
            buffer dat(2);

            time(&gmt);
            ptm    = gmtime(&gmt);
            dat[0] = unsigned(ptm->tm_year + 1900) >> 8;
            dat[1] = unsigned(ptm->tm_year + 1900) & 0xFF;
            if (GPS_DEBUG)
                IDLog(" Sending: %d [%d,%d]\n", ptm->tm_year, dat[0], dat[1]);
            AUXCommand cmd(GPS_GET_YEAR, GPS, m.src, dat);
            sendCmd(cmd);
            //readMsgs();
            break;
        }
        case GPS_LINKED:
        {
            if (GPS_DEBUG)
                IDLog("GPS: LINKED from 0x%02x\n", m.src);
            buffer dat(1);

            dat[0] = unsigned(1);
            AUXCommand cmd(GPS_LINKED, GPS, m.src, dat);
            sendCmd(cmd);
            //readMsgs();
            break;
        }
        default:
            IDLog("Got 0x%02x for GPS\n", m.cmd);
            break;
    }
}

void CelestronAUX::processCmd(AUXCommand &m)
{
    if (PROC_DEBUG)
    {
        m.pprint();
    }
    if (m.dst == GPS)
        emulateGPS(m);
    else
        switch (m.cmd)
        {
            case MC_GET_POSITION:
                switch (m.src)
                {
                    case ALT:
                        Alt = m.getPosition();
                        // if (PROC_DEBUG) IDLog("ALT: %ld", Alt);
                        break;
                    case AZM:
                        Az = m.getPosition();
                        // Celestron uses N as zero Azimuth!
                        Az += STEPS_PER_REVOLUTION / 2;
                        Az %= STEPS_PER_REVOLUTION;
                        // if (PROC_DEBUG) IDLog("AZM: %ld", Az);
                        break;
                    default:
                        break;
                }
                break;
            case MC_SLEW_DONE:
                switch (m.src)
                {
                    case ALT:
                        slewingAlt = m.data[0] != 0xff;
                        // if (PROC_DEBUG) IDLog("ALT %d ", slewingAlt);
                        break;
                    case AZM:
                        slewingAz = m.data[0] != 0xff;
                        // if (PROC_DEBUG) IDLog("AZM %d ", slewingAz);
                        break;
                    default:
                        break;
                }
                break;
            case MC_POLL_CORDWRAP:
                if (m.src == AZM)
                    cordwrap = m.data[0] == 0xff;
                break;
            case MC_GET_CORDWRAP_POS:
                if (m.src == AZM)
                {
                    cordwrapPos = m.getPosition();
                    DEBUGF(DBG_CAUX, "Got cordwrap position %.1f", float(cordwrapPos) / STEPS_PER_DEGREE);
                }
                break;
            case GET_VER:
                if (m.src != APP)
                    LOGF_INFO("Got GET_VERSION response from %s: %d.%d.%d ", m.node_name(m.src), m.data[0], m.data[1],
                              256 * m.data[2] + m.data[3]);
                switch (m.src)
                {
                    case MB:
                        mb_ver_maj = m.data[0];
                        mb_ver_min = m.data[1];
                        break;
                    case ALT:
                        alt_ver_maj = m.data[0];
                        alt_ver_min = m.data[1];
                        break;
                    case AZM:
                        azm_ver_maj = m.data[0];
                        azm_ver_min = m.data[1];
                        break;
                    case APP:
                        DEBUGF(DBG_CAUX, "Got echo of GET_VERSION from %s", m.node_name(m.dst));
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    // if (PROC_DEBUG) IDLog("\n");
}

bool CelestronAUX::serial_readMsgs(AUXCommand c)
{
    int n;
    unsigned char buf[32];
    char hexbuf[24];
    AUXCommand cmd;

    // We are not connected. Nothing to do.
    if ( PortFD <= 0 )
        return false;

    if (isRTSCTS)
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
            DEBUG(DBG_CAUX, "Did not got whole packet. Dropping out.");
            return false;
        }

        buffer b(buf, buf + (n + 2));

        if (SERIAL_DEBUG)
        {
            hex_dump(hexbuf, b, b.size());
            IDLog("Receive packet: <%s>\n", hexbuf);
        }

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
            buffer b(buf, buf + (response_data_size + 5));
            hex_dump(hexbuf, b, b.size());
            LOGF_ERROR("Receive packet: %s", hexbuf);
            return false;
        }

        buf[0] = 0x3b;
        buf[1] = response_data_size + 1;
        buf[2] = c.dst;
        buf[3] = c.src;
        buf[4] = c.cmd;

        buffer b(buf, buf + (response_data_size + 5));

        if (SERIAL_DEBUG)
        {
            hex_dump(hexbuf, b, b.size());
            IDLog("Receive packet (%d B): <%s>\n", (int)b.size(), hexbuf);
        }

        cmd.parseBuf(b, false);
    }

    // Got the packet, process it
    // n:length field >=3
    // The buffer of n+2>=5 bytes contains:
    // 0x3b <n>=3> <from> <to> <type> <n-3 bytes> <xsum>

    if (RD_DEBUG)
    {
        IDLog("Got %d bytes:  ; payload length field: %d ; MSG:", n, buf[1]);
        prnBytes(buf, n + 2);
    }
    processCmd(cmd);
    return true;
}

bool CelestronAUX::tcp_readMsgs()
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
        if (RD_DEBUG)
        {
            IDLog("Got %d bytes: ", n);
            for (i = 0; i < n; i++)
                IDLog("%02x ", buf[i]);
            IDLog("\n");
        }
        for (i = 0; i < n;)
        {
            //if (RD_DEBUG) fprintf(stderr,"%d ",i);
            if (buf[i] == 0x3b)
            {
                int shft;
                shft = i + buf[i + 1] + 3;
                if (shft <= n)
                {
                    //if (RD_DEBUG) prnBytes(buf+i,shft-i);
                    buffer b(buf + i, buf + shft);
                    //if (RD_DEBUG) dumpMsg(b);
                    cmd.parseBuf(b);
                    processCmd(cmd);
                }
                else
                {
                    IDLog("Partial message recv. dropping (i=%d %d/%d)\n", i, shft, n);
                    prnBytes(buf + i, n - i);
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
            if (RD_DEBUG)
                IDLog("Consumed %d/%d bytes \n", n, i);
        }
    }
    //fprintf(stderr,"Nothing more to read\n");
    return true;
}



bool CelestronAUX::readMsgs(AUXCommand c)
{
    if (getActiveConnection() == serialConnection)
        return serial_readMsgs(c);
    else
        return tcp_readMsgs();
}


int CelestronAUX::sendBuffer(int PortFD, buffer buf)
{

    if ( PortFD > 0 )
    {
        int n;

        if (aux_tty_write(PortFD, (char*)buf.data(), buf.size(), CTS_TIMEOUT, &n) != TTY_OK)
            return 0;

        msleep(50);
        if (n == -1)
            perror("CAUX::sendBuffer");
        if ((unsigned)n != buf.size())
            IDLog("sendBuffer: incomplete send n=%d size=%d\n", n, (int)buf.size());
        return n;
    }
    else
        return 0;
}


bool CelestronAUX::sendCmd(AUXCommand &c)
{
    buffer buf;

    if (SEND_DEBUG)
    {
        IDLog("Send: ");
        c.dumpCmd();
    }

    if (isRTSCTS || getActiveConnection() != serialConnection)
        // Direct connection (AUX/PC port)
        c.fillBuf(buf);
    else
    {
        // connection is through HC serial, convert AUX command to a
        // passthrough command
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

    if (SERIAL_DEBUG)
    {
        char hexbuf[32 * 3] = {0};
        hex_dump(hexbuf, buf, buf.size());
        IDLog("Send packet: <%s>\n", hexbuf);
    }

    tcflush(PortFD, TCIOFLUSH);
    return sendBuffer(PortFD, buf) == (int)buf.size();
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
        LOGF_ERROR("Error getting handshake lines %s(%d).\n", strerror(errno), errno);
    if (rts)
        modem_ctrl |= TIOCM_RTS;
    else
        modem_ctrl &= ~TIOCM_RTS;
    if (ioctl(PortFD, TIOCMSET, &modem_ctrl) == -1)
        LOGF_ERROR("Error setting handshake lines %s(%d).\n", strerror(errno), errno);
}


bool CelestronAUX::waitCTS(float timeout)
{
    float step = timeout / 20.;
    for (; timeout >= 0; timeout -= step)
    {
        msleep(step);
        if (ioctl(PortFD, TIOCMGET, &modem_ctrl) == -1)
        {
            LOGF_ERROR("Error getting handshake lines %s(%d).\n", strerror(errno), errno);
            return 0;
        }
        if (modem_ctrl & TIOCM_CTS)
            return 1;
    }
    return 0;
}


bool CelestronAUX::detectRTSCTS()
{
    bool retval;
    setRTS(1);
    retval = waitCTS(300.);
    setRTS(0);
    return retval;
}


int CelestronAUX::aux_tty_read(int PortFD, char *buf, int bufsiz, int timeout, int *n)
{
    int errcode;

    if (RD_DEBUG)
        IDLog("aux_tty_read: %d\n", PortFD);

    // if hardware flow control is required, set RTS to off to receive: PC port
    // bahaves as half duplex.
    if (isRTSCTS)
        setRTS(0);

    if((errcode = tty_read(PortFD, buf, bufsiz, timeout, n)) != TTY_OK)
    {
        char errmsg[MAXRBUF] = {0};
        tty_error_msg(errcode, errmsg, MAXRBUF);
        IDLog("%s\n", errmsg);
    }

    return errcode;
}


int CelestronAUX::aux_tty_write(int PortFD, char *buf, int bufsiz, float timeout, int *n)
{
    int errcode, ne;
    char errmsg[MAXRBUF];

    if (WR_DEBUG)
        IDLog("aux_tty_write: %d\n", PortFD);

    // if hardware flow control is required, set RTS to on then wait for CTS
    // on to write: PC port bahaves as half duplex. RTS may be already on.
    if (isRTSCTS)
    {
        if (WR_DEBUG) IDLog("aux_tty_write: set RTS \n");
        setRTS(1);
        if (WR_DEBUG) IDLog("aux_tty_write: wait CTS \n");
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
        if (WR_DEBUG) IDLog("aux_tty_write: clear RTS\n");
        msleep(RTS_DELAY);
        setRTS(0);
    }

    // ports requiring hardware flow control echo all sent characters,
    // verify them.
    if (isRTSCTS)
    {
        if (WR_DEBUG) IDLog("aux_tty_write: verify echo\n");
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


bool CelestronAUX::tty_set_speed(int PortFD, speed_t speed)
{
    struct termios tty_setting;

    if (tcgetattr(PortFD, &tty_setting))
    {
        LOGF_ERROR("Error getting tty attributes %s(%d).\n", strerror(errno), errno);
        return false;
    }

    if (cfsetspeed(&tty_setting, speed))
    {
        LOGF_ERROR("Error setting serial speed %s(%d).\n", strerror(errno), errno);
        return false;
    }

    if (tcsetattr(PortFD, TCSANOW, &tty_setting))
    {
        LOGF_ERROR("Error setting tty attributes %s(%d).\n", strerror(errno), errno);
        return false;
    }


    return true;
}


void CelestronAUX::hex_dump(char *buf, buffer data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

