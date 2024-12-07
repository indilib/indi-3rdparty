/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
/* This file is part of the Skywatcher Protocol INDI driver.

    The Skywatcher Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Skywatcher Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Skywatcher Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.

    2013-10-20: Fixed a few bugs and init/update properties issue (Jasem Mutlaq)
    2013-10-24: Use updateTime from new INDI framework (Jasem Mutlaq)
    2013-10-31: Added support for joysticks (Jasem Mutlaq)
    2013-11-01: Fixed issues with logger and Skywatcher's readout for InquireHighSpeedRatio.
    2018-04-27: Added abnormalDisconnect to properly disconnect the driver in case of unrecoverable errors (Jasem Mutlaq)
    2018-04-27: Since all dispatch_command are always followed by read_eqmod, we decided to include it inside
                and on failure, we retries up to the EQMOD_MAX_RETRY before giving up in case of occasional traient errors. (Jasem Mutlaq)
*/

/* TODO */
/* HORIZONTAL_COORDS -> HORIZONTAL_COORD - OK */
/* DATE -> TIME_LST/LST and TIME_UTC/UTC - OK */
/*  Problem in time initialization using gettimeofday/gmtime: 1h after UTC on summer, because of DST ?? */
/* TELESCOPE_MOTION_RATE in arcmin/s */
/* use/snoop a GPS ??*/

#include "eqmodbase.h"

#include "mach_gettime.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include <indicom.h>

#include <connectionplugins/connectiontcp.h>

#ifdef WITH_ALIGN
#include <alignment/DriverCommon.h> // For DBG_ALIGNMENT
using namespace INDI::AlignmentSubsystem;
#endif

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>
#include <libnova/utility.h>

#define GOTO_RATE      2        /* slew rate, degrees/s */
#define SLEW_RATE      0.5      /* slew rate, degrees/s */
#define FINE_SLEW_RATE 0.1      /* slew rate, degrees/s */
#define SID_RATE       0.004178 /* sidereal rate, degrees/s */

#define GOTO_LIMIT      5   /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      2   /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5 /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define GOTO_ITERATIVE_LIMIT 5 /* Max GOTO Iterations */
#define RAGOTORESOLUTION     5 /* GOTO Resolution in arcsecs */
#define DEGOTORESOLUTION     5 /* GOTO Resolution in arcsecs */

/* Preset Slew Speeds */
#define SLEWMODES 11
int slewspeeds[SLEWMODES - 1] = { 1, 2, 4, 8, 32, 64, 128, 600, 700, 800 };

#define RA_AXIS     0
#define DEC_AXIS    1
#define GUIDE_NORTH 0
#define GUIDE_SOUTH 1
#define GUIDE_WEST  0
#define GUIDE_EAST  1

#if 0
int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec)
    {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000)
    {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
    result->tv_sec  = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}
#endif

EQMod::EQMod(): GI(this)
{
    //ctor
    setVersion(EQMOD_VERSION_MAJOR, EQMOD_VERSION_MINOR);
    // Do not define dynamic properties on startup, and do not delete them from memory
    setDynamicPropertiesBehavior(false, false);
    currentRA            = 0;
    currentDEC           = 90;
    gotoparams.completed = true;
    last_motion_ns       = -1;
    last_motion_ew       = -1;
    pulseInProgress      = 0;

    DBG_SCOPE_STATUS = INDI::Logger::getInstance().addDebugLevel("Scope Status", "SCOPE");
    DBG_COMM         = INDI::Logger::getInstance().addDebugLevel("Serial Port", "COMM");
    DBG_MOUNT        = INDI::Logger::getInstance().addDebugLevel("Verbose Mount", "MOUNT");

    mount = new Skywatcher(this);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION
                           | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_RATE | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK,
                           SLEWMODES);

    RAInverted = DEInverted = false;
    bzero(&syncdata, sizeof(syncdata));
    bzero(&syncdata2, sizeof(syncdata2));

#ifdef WITH_ALIGN_GEEHALEL
    align = new Align(this);
#endif

    simulator = new EQModSimulator(this);

#ifdef WITH_SCOPE_LIMITS
    horizon = new HorizonLimits(this);
#endif

    /* initialize time */
    tzset();
    gettimeofday(&lasttimeupdate, nullptr); // takes care of DST
    gmtime_r(&lasttimeupdate.tv_sec, &utc);
    lndate.seconds = utc.tm_sec + ((double)lasttimeupdate.tv_usec / 1000000);
    lndate.minutes = utc.tm_min;
    lndate.hours   = utc.tm_hour;
    lndate.days    = utc.tm_mday;
    lndate.months  = utc.tm_mon + 1;
    lndate.years   = utc.tm_year + 1900;
    get_utc_time(&lastclockupdate);
    /* initialize random seed: */
    srand(time(nullptr));
    // Others
    AutohomeState      = AUTO_HOME_IDLE;
    restartguidePPEC   = false;
}

EQMod::~EQMod()
{
    //dtor
    delete mount;
    mount = nullptr;
}

#if defined WITH_ALIGN || defined WITH_ALIGN_GEEHALEL
bool EQMod::isStandardSync()
{
    return AlignSyncModeSP.findOnSwitch()->isNameMatch("ALIGNSTANDARDSYNC");
}
#endif

void EQMod::setStepperSimulation(bool enable)
{
    mount->setSimulation(enable);
    if (!simulator->updateProperties(enable))
        LOG_WARN("setStepperSimulator: Disable/Enable error");
    INDI::Telescope::setSimulation(enable);
}

const char *EQMod::getDefaultName()
{
    return "EQMod Mount";
}

double EQMod::getLongitude()
{
    auto number = LocationNP.findWidgetByName("LONG");
    if (number)
        return number->getValue();
    else
        return 0;
}

double EQMod::getLatitude()
{
    auto number = LocationNP.findWidgetByName("LAT");
    if(number)
        return number->getValue();
    else
        return 0;
}

double EQMod::getJulianDate()
{
    /*
    struct timeval currenttime, difftime;
    double usecs;
    gettimeofday(&currenttime, nullptr);
    if (timeval_subtract(&difftime, &currenttime, &lasttimeupdate) == -1)
    return juliandate;
    */
    struct timespec currentclock, diffclock;
    double nsecs;
    get_utc_time(&currentclock);
    diffclock.tv_sec  = currentclock.tv_sec - lastclockupdate.tv_sec;
    diffclock.tv_nsec = currentclock.tv_nsec - lastclockupdate.tv_nsec;
    while (diffclock.tv_nsec > 1000000000)
    {
        diffclock.tv_sec++;
        diffclock.tv_nsec -= 1000000000;
    }
    while (diffclock.tv_nsec < 0)
    {
        diffclock.tv_sec--;
        diffclock.tv_nsec += 1000000000;
    }
    //IDLog("Get Julian; ln_date was %02d:%02d:%.9f\n", lndate.hours, lndate.minutes, lndate.seconds);
    //IDLog("Clocks last: %d secs %d nsecs current: %d secs %d nsecs\n", lastclockupdate.tv_sec,  lastclockupdate.tv_nsec, currentclock.tv_sec,  currentclock.tv_nsec);
    //IDLog("Diff %d secs %d nsecs\n", diffclock.tv_sec,  diffclock.tv_nsec);
    //IDLog("Diff %d %d\n", difftime.tv_sec,  difftime.tv_usec);
    //lndate.seconds += (difftime.tv_sec + (difftime.tv_usec / 1000000));
    //usecs=lndate.seconds - floor(lndate.seconds);
    lndate.seconds += (diffclock.tv_sec + ((double)diffclock.tv_nsec / 1000000000.0));
    nsecs        = lndate.seconds - floor(lndate.seconds);
    utc.tm_sec   = lndate.seconds;
    utc.tm_isdst = -1; // let mktime find if DST already in effect in utc
    //IDLog("Get julian: setting UTC secs to %f", utc.tm_sec);
    mktime(&utc); // normalize time
    //IDLog("Get Julian; UTC is now %s", asctime(&utc));
    ln_get_date_from_tm(&utc, &lndate);
    //IDLog("Get Julian; ln_date is now %02d:%02d:%.9f\n", lndate.hours, lndate.minutes, lndate.seconds);
    //lndate.seconds+=usecs;
    //lasttimeupdate = currenttime;
    lndate.seconds += nsecs;
    //IDLog("     ln_date with nsecs %02d:%02d:%.9f\n", lndate.hours, lndate.minutes, lndate.seconds);
    lastclockupdate = currentclock;
    juliandate      = ln_get_julian_day(&lndate);
    //IDLog("julian diff: %g\n", juliandate - ln_get_julian_from_sys());
    return juliandate;
    //return ln_get_julian_from_sys();
}

double EQMod::getLst(double jd, double lng)
{
    double lst;
    //lst=ln_get_mean_sidereal_time(jd);
    lst = ln_get_apparent_sidereal_time(jd);
    lst += (lng / 15.0);
    lst = range24(lst);
    return lst;
}

bool EQMod::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    loadProperties();

    initSlewRates();

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    SetParkDataType(PARK_RA_DEC_ENCODER);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
#ifdef WITH_ALIGN
    InitAlignmentProperties(this);

    // Force the alignment system to always be on
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")[0].setState(ISS_ON);
#endif

    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(11880);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);

    addAuxControls();
    return true;
}

void EQMod::initSlewRates()
{
    for (size_t i = 0; i < SlewRateSP.count() - 1; i++)
    {
        SlewRateSP[i].setState(ISS_OFF);
        SlewRateSP[i].setLabel(std::to_string(slewspeeds[i]) + "x");

        SlewRateSP[i].setAux((void *)&slewspeeds[i]);
    }

    // Since last item is NOT maximum (but custom), let's set item before custom to SLEWMAX
    SlewRateSP[SlewRateSP.count() - 2].setState(ISS_ON);
    SlewRateSP[SlewRateSP.count() - 2].setName("SLEW_MAX");
    // Last is custom
    SlewRateSP[SlewRateSP.count() - 1].setName("SLEWCUSTOM");
    SlewRateSP[SlewRateSP.count() - 1].setLabel("Custom");

}

void EQMod::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
        GI::updateProperties();
        defineProperty(SlewSpeedsNP);
        defineProperty(GuideRateNP);
        defineProperty(PulseLimitsNP);
        defineProperty(MountInformationTP);
        defineProperty(SteppersNP);
        defineProperty(CurrentSteppersNP);
        defineProperty(PeriodsNP);
        defineProperty(JulianNP);
        defineProperty(TimeLSTNP);
        defineProperty(RAStatusLP);
        defineProperty(DEStatusLP);
        defineProperty(HemisphereSP);
        defineProperty(HorizontalCoordNP);
        defineProperty(ReverseDECSP);
        defineProperty(TargetPierSideSP);
        defineProperty(StandardSyncNP);
        defineProperty(StandardSyncPointNP);
        defineProperty(SyncPolarAlignNP);
        defineProperty(SyncManageSP);
        defineProperty(BacklashNP);
        defineProperty(UseBacklashSP);
        defineProperty(TrackDefaultSP);
        defineProperty(ST4GuideRateNSSP);
        defineProperty(ST4GuideRateWESP);

#if defined WITH_ALIGN && defined WITH_ALIGN_GEEHALEL
        defineProperty(&AlignMethodSP);
#endif
#if defined WITH_ALIGN || defined WITH_ALIGN_GEEHALEL
        defineProperty(AlignSyncModeSP);
#endif

        if (mount->HasAuxEncoders())
        {
            defineProperty(AuxEncoderSP);
            defineProperty(AuxEncoderNP);
        }
        if (mount->HasPPEC())
        {
            defineProperty(PPECTrainingSP);
            defineProperty(PPECSP);
        }
        if (mount->HasSnapPort1())
        {
            defineProperty(SNAPPORT1SP);
        }
        if (mount->HasSnapPort2())
        {
            defineProperty(SNAPPORT2SP);
        }
        if (mount->HasPolarLed())
        {
            defineProperty(LEDBrightnessNP);
        }

#ifdef WITH_ALIGN_GEEHALEL
        if (align)
        {
            align->ISGetProperties();
        }
#endif

#ifdef WITH_SCOPE_LIMITS
        if (horizon)
        {
            horizon->ISGetProperties();
        }
#endif
        simulator->updateProperties(isSimulation());
    }
}

bool EQMod::loadProperties()
{
    buildSkeleton("indi_eqmod_sk.xml");

    GuideRateNP = getNumber("GUIDE_RATE");
    // GuideRateN  = GuideRateNP->np;

    PulseLimitsNP  = getNumber("PULSE_LIMITS");
    MinPulseN      = PulseLimitsNP.findWidgetByName("MIN_PULSE");
    MinPulseTimerN = PulseLimitsNP.findWidgetByName("MIN_PULSE_TIMER");

    MountInformationTP = getText("MOUNTINFORMATION");
    SteppersNP         = getNumber("STEPPERS");
    CurrentSteppersNP  = getNumber("CURRENTSTEPPERS");
    PeriodsNP          = getNumber("PERIODS");
    JulianNP           = getNumber("JULIAN");
    TimeLSTNP          = getNumber("TIME_LST");
    RAStatusLP         = getLight("RASTATUS");
    DEStatusLP         = getLight("DESTATUS");
    SlewSpeedsNP       = getNumber("SLEWSPEEDS");
    HemisphereSP       = getSwitch("HEMISPHERE");
    TrackDefaultSP     = getSwitch("TELESCOPE_TRACK_DEFAULT");
    ReverseDECSP       = getSwitch("REVERSEDEC");
    TargetPierSideSP   = getSwitch("TARGETPIERSIDE");

    HorizontalCoordNP   = getNumber("HORIZONTAL_COORD");
    StandardSyncNP      = getNumber("STANDARDSYNC");
    StandardSyncPointNP = getNumber("STANDARDSYNCPOINT");
    SyncPolarAlignNP    = getNumber("SYNCPOLARALIGN");
    SyncManageSP        = getSwitch("SYNCMANAGE");
    BacklashNP          = getNumber("BACKLASH");
    UseBacklashSP       = getSwitch("USEBACKLASH");
    AuxEncoderSP        = getSwitch("AUXENCODER");
    AuxEncoderNP        = getNumber("AUXENCODERVALUES");
    ST4GuideRateNSSP    = getSwitch("ST4_GUIDE_RATE_NS");
    ST4GuideRateWESP    = getSwitch("ST4_GUIDE_RATE_WE");
    PPECTrainingSP      = getSwitch("PPEC_TRAINING");
    PPECSP              = getSwitch("PPEC");
    LEDBrightnessNP     = getNumber("LED_BRIGHTNESS");
    SNAPPORT1SP         = getSwitch("SNAPPORT1");
    SNAPPORT2SP         = getSwitch("SNAPPORT2");
#ifdef WITH_ALIGN_GEEHALEL
    align->initProperties();
#endif
#if defined WITH_ALIGN && defined WITH_ALIGN_GEEHALEL
    IUFillSwitch(&AlignMethodS[0], "ALIGN_METHOD_EQMOD", "EQMod Align", ISS_ON);
    IUFillSwitch(&AlignMethodS[1], "ALIGN_METHOD_SUBSYSTEM", "Alignment Subsystem", ISS_OFF);
    IUFillSwitchVector(&AlignMethodSP, AlignMethodS, NARRAY(AlignMethodS), getDeviceName(), "ALIGN_METHOD",
                       "Align Method", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#endif
#if defined WITH_ALIGN || defined WITH_ALIGN_GEEHALEL
    AlignSyncModeSP = getSwitch("ALIGNSYNCMODE");
#endif

    simulator->initProperties();

    GI::initProperties(MOTION_TAB);

#ifdef WITH_SCOPE_LIMITS
    if (horizon)
    {
        if (!horizon->initProperties())
            return false;
    }
#endif

    return true;
}

bool EQMod::updateProperties()
{
    // JM 2024.07.29: Need to run this *before* Telescope::updateProperty so we can check if
    // the mount supports homing since we need to update the telescope capabilities accordingly.
    if (isConnected())
    {
        try
        {
            mount->InquireBoardVersion(MountInformationTP);

            for (const auto &it : MountInformationTP)
            {
                LOGF_DEBUG("Got Board Property %s: %s", it.getName(), it.getText());
            }

            mount->InquireRAEncoderInfo(SteppersNP);
            mount->InquireDEEncoderInfo(SteppersNP);

            for (const auto &it : SteppersNP)
            {
                LOGF_DEBUG("Got Encoder Property %s: %.0f", it.getLabel(), it.getValue());
            }

            mount->InquireFeatures();
            if (mount->HasHomeIndexers())
            {
                LOG_INFO("Mount has home indexers. Enabling Autohome.");
                SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_CAN_HOME_FIND, SLEWMODES);
                initSlewRates();
            }
        }
        catch (EQModError &e)
        {
            return (e.DefaultHandleException(this));
        }
    }

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(SlewSpeedsNP);
        defineProperty(GuideRateNP);
        defineProperty(PulseLimitsNP);
        defineProperty(MountInformationTP);
        defineProperty(SteppersNP);
        defineProperty(CurrentSteppersNP);
        defineProperty(PeriodsNP);
        defineProperty(JulianNP);
        defineProperty(TimeLSTNP);
        defineProperty(RAStatusLP);
        defineProperty(DEStatusLP);
        defineProperty(HemisphereSP);
        defineProperty(HorizontalCoordNP);
        defineProperty(ReverseDECSP);
        defineProperty(TargetPierSideSP);
        defineProperty(StandardSyncNP);
        defineProperty(StandardSyncPointNP);
        defineProperty(SyncPolarAlignNP);
        defineProperty(SyncManageSP);
        defineProperty(BacklashNP);
        defineProperty(UseBacklashSP);
        defineProperty(TrackDefaultSP);
        defineProperty(ST4GuideRateNSSP);
        defineProperty(ST4GuideRateWESP);

#if defined WITH_ALIGN && defined WITH_ALIGN_GEEHALEL
        defineProperty(&AlignMethodSP);
#endif
#if defined WITH_ALIGN || defined WITH_ALIGN_GEEHALEL
        defineProperty(AlignSyncModeSP);
#endif
        try
        {
            if (mount->HasAuxEncoders())
            {
                defineProperty(AuxEncoderSP);
                defineProperty(AuxEncoderNP);
                LOG_INFO("Mount has auxiliary encoders. Turning them off.");
                mount->TurnRAEncoder(false);
                mount->TurnDEEncoder(false);
            }
            if (mount->HasPPEC())
            {
                bool intraining, inppec;
                defineProperty(PPECTrainingSP);
                defineProperty(PPECSP);
                LOG_INFO("Mount has PPEC.");
                mount->GetPPECStatus(&intraining, &inppec);
                if (intraining)
                {
                    PPECTrainingSP[0].setState(ISS_OFF);
                    PPECTrainingSP[1].setState(ISS_ON);
                    PPECTrainingSP.setState(IPS_BUSY);
                    PPECTrainingSP.apply();
                }
                if (inppec)
                {
                    PPECSP[0].setState(ISS_OFF);
                    PPECSP[1].setState(ISS_ON);
                    PPECSP.setState(IPS_BUSY);
                    PPECSP.apply();
                }
            }

            if (mount->HasPolarLed())
            {
                defineProperty(LEDBrightnessNP);
            }

            LOG_DEBUG("Init backlash.");
            mount->SetBacklashUseRA(UseBacklashSP.findWidgetByName("USEBACKLASHRA")->getState() == ISS_ON ? true : false);
            mount->SetBacklashUseDE(UseBacklashSP.findWidgetByName("USEBACKLASHDE")->getState() == ISS_ON ? true : false);
            mount->SetBacklashRA((uint32_t)(BacklashNP.findWidgetByName("BACKLASHRA")->getValue()));
            mount->SetBacklashDE((uint32_t)(BacklashNP.findWidgetByName("BACKLASHDE")->getValue()));

            if (mount->HasSnapPort1())
            {
                defineProperty(SNAPPORT1SP);
            }
            if (mount->HasSnapPort2())
            {
                defineProperty(SNAPPORT2SP);
            }

            mount->Init();

            zeroRAEncoder  = mount->GetRAEncoderZero();
            totalRAEncoder = mount->GetRAEncoderTotal();
            homeRAEncoder  = mount->GetRAEncoderHome();
            zeroDEEncoder  = mount->GetDEEncoderZero();
            totalDEEncoder = mount->GetDEEncoderTotal();
            homeDEEncoder  = mount->GetDEEncoderHome();

            parkRAEncoder = GetAxis1Park();
            parkDEEncoder = GetAxis2Park();

            auto latitude = LocationNP.findWidgetByName("LAT");
            auto longitude = LocationNP.findWidgetByName("LONG");
            auto elevation = LocationNP.findWidgetByName("ELEV");
            if (latitude && longitude && elevation)
                updateLocation(latitude->getValue(), longitude->getValue(), elevation->getValue());

            sendTimeFromSystem();
        }
        catch (EQModError &e)
        {
            return (e.DefaultHandleException(this));
        }
    }
    else
    {
        deleteProperty(GuideRateNP);
        deleteProperty(PulseLimitsNP);
        deleteProperty(MountInformationTP);
        deleteProperty(SteppersNP);
        deleteProperty(CurrentSteppersNP);
        deleteProperty(PeriodsNP);
        deleteProperty(JulianNP);
        deleteProperty(TimeLSTNP);
        deleteProperty(RAStatusLP);
        deleteProperty(DEStatusLP);
        deleteProperty(SlewSpeedsNP);
        deleteProperty(HemisphereSP);
        deleteProperty(HorizontalCoordNP);
        deleteProperty(ReverseDECSP);
        deleteProperty(TargetPierSideSP);
        deleteProperty(StandardSyncNP);
        deleteProperty(StandardSyncPointNP);
        deleteProperty(SyncPolarAlignNP);
        deleteProperty(SyncManageSP);
        deleteProperty(TrackDefaultSP);
        deleteProperty(BacklashNP);
        deleteProperty(UseBacklashSP);
        deleteProperty(ST4GuideRateNSSP);
        deleteProperty(ST4GuideRateWESP);
        deleteProperty(LEDBrightnessNP);

        if (mount->HasAuxEncoders())
        {
            deleteProperty(AuxEncoderSP);
            deleteProperty(AuxEncoderNP);
        }
        if (mount->HasPPEC())
        {
            deleteProperty(PPECTrainingSP);
            deleteProperty(PPECSP);
        }
        if (mount->HasSnapPort1())
        {
            deleteProperty(SNAPPORT1SP);
        }
        if (mount->HasSnapPort2())
        {
            deleteProperty(SNAPPORT2SP);
        }
        if (mount->HasPolarLed())
        {
            deleteProperty(LEDBrightnessNP);
        }
#if defined WITH_ALIGN && defined WITH_ALIGN_GEEHALEL
        deleteProperty(AlignMethodSP.name);
#endif
#if defined WITH_ALIGN || defined WITH_ALIGN_GEEHALEL
        deleteProperty(AlignSyncModeSP);
#endif
        //MountInformationTP=nullptr;
        //}
    }
#ifdef WITH_ALIGN_GEEHALEL
    if (align)
    {
        if (!align->updateProperties())
            return false;
    }
#endif

#ifdef WITH_SCOPE_LIMITS
    if (horizon)
    {
        if (!horizon->updateProperties())
            return false;
    }
#endif

    GI::updateProperties();
    mount->setSimulation(isSimulation());
    simulator->updateProperties(isSimulation());

    return true;
}

bool EQMod::Handshake()
{
    try
    {
        if (!getActiveConnection()->name().compare("CONNECTION_TCP")
                && tcpConnection->connectionType() == Connection::TCP::TYPE_UDP)
        {
            tty_set_generic_udp_format(1);
        }

        mount->setPortFD(PortFD);
        mount->Handshake();
        // Mount initialisation is in updateProperties as it sets directly Indi properties which should be defined
    }
    catch (EQModError &e)
    {
        return false;
        //return (e.DefaultHandleException(this));
    }

#ifdef WITH_ALIGN
    // Set this according to mount type
    SetApproximateMountAlignmentFromMountType(EQUATORIAL);
#endif

    LOG_INFO("Successfully connected to EQMod Mount.");
    return true;
}

void EQMod::abnormalDisconnectCallback(void *userpointer)
{
    EQMod *p = static_cast<EQMod *>(userpointer);
    if (p->Connect())
    {
        p->setConnected(true, IPS_OK);
        p->updateProperties();
    }
}

void EQMod::abnormalDisconnect()
{
    // Ignore disconnect errors
    INDI::Telescope::Disconnect();

    // Set Disconnected
    setConnected(false, IPS_IDLE);
    // Update properties
    updateProperties();

    // Reconnect in 2 seconds
    IEAddTimer(2000, (IE_TCF *)abnormalDisconnectCallback, this);
}

bool EQMod::Disconnect()
{
    if (isConnected())
    {
        try
        {
            mount->Disconnect();
        }
        catch (EQModError &e)
        {
            LOGF_ERROR("Error when disconnecting mount -> %s", e.message);
            return (false);
        }
        return INDI::Telescope::Disconnect();
    }
    else
        return false;
}

void EQMod::TimerHit()
{
    if (isConnected())
    {
        bool rc;

        // Skip reading scope status if we are in a middle of a pulse
        // to avoid delaying it
        if (pulseInProgress != 0)
        {
            rc = true;
        }
        else
        {
            rc = ReadScopeStatus();
        }

        if (rc == false)
        {
            // read was not good
            EqNP.setState(IPS_ALERT);
            EqNP.apply();
        }

        SetTimer(getCurrentPollingPeriod());
    }
}

bool EQMod::ReadScopeStatus()
{
    // Time
    double juliandate = 0;
    double lst = 0;
    char hrlst[12] = {0};

    const char *datenames[] = { "LST", "JULIANDATE", "UTC" };
    double periods[2];
    const char *periodsnames[] = { "RAPERIOD", "DEPERIOD" };
    double horizvalues[2];
    const char *horiznames[2] = { "AZ", "ALT" };
    double steppervalues[2];
    const char *steppernames[] = { "RAStepsCurrent", "DEStepsCurrent" };

    juliandate = getJulianDate();
    lst        = getLst(juliandate, getLongitude());

    fs_sexa(hrlst, lst, 2, 360000);
    hrlst[11] = '\0';
    DEBUGF(DBG_SCOPE_STATUS, "Compute local time: lst=%2.8f (%s) - julian date=%8.8f", lst, hrlst, juliandate);

    TimeLSTNP.update(&lst, (char **)(datenames), 1);
    TimeLSTNP.setState(IPS_OK);
    TimeLSTNP.apply();

    JulianNP.update(&juliandate, (char **)(datenames + 1), 1);
    JulianNP.setState(IPS_OK);
    JulianNP.apply();

    try
    {
        TelescopePierSide pierSide;
        currentRAEncoder = mount->GetRAEncoder();
        currentDEEncoder = mount->GetDEEncoder();
        DEBUGF(DBG_SCOPE_STATUS, "Current encoders RA=%ld DE=%ld", static_cast<long>(currentRAEncoder),
               static_cast<long>(currentDEEncoder));
        EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &currentRA, &currentDEC, &currentHA, &pierSide);
        setPierSide(pierSide);

        alignedRA    = currentRA;
        alignedDEC   = currentDEC;
        ghalignedRA  = currentRA;
        ghalignedDEC = currentDEC;
        bool aligned = false;
#ifdef WITH_ALIGN_GEEHALEL
        if (align)
        {
            align->GetAlignedCoords(syncdata, juliandate, &m_Location, currentRA, currentDEC, &ghalignedRA,
                                    &ghalignedDEC);
            aligned = true;
        }
        //   else
#endif
#ifdef WITH_ALIGN
        // Only use INDI Alignment Subsystem if it is active.
        if (AlignMethodSP.sp[1].s == ISS_ON)
        {
            const char *maligns[3] = { "ZENITH", "NORTH", "SOUTH" };
            INDI::IEquatorialCoordinates RaDec;
            // Use HA/Dec as  telescope coordinate system
            RaDec.rightascension = currentRA;
            RaDec.declination = currentDEC;
            TelescopeDirectionVector TDV = TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
                   "Status: Mnt. Algnt. %s Date %lf encoders RA=%ld DE=%ld Telescope RA %lf DEC %lf",
                   maligns[GetApproximateMountAlignment()], juliandate,
                   static_cast<long>(currentRAEncoder), static_cast<long>(currentDEEncoder),
                   currentRA, currentDEC);
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, " Direction RA(deg.)  %lf DEC %lf TDV(x %lf y %lf z %lf)",
                   RaDec.rightascension, RaDec.declination, TDV.x, TDV.y, TDV.z);
            aligned = true;
            if (!TransformTelescopeToCelestial(TDV, alignedRA, alignedDEC))
            {
                aligned = false;
                DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
                       "Failed TransformTelescopeToCelestial: Scope RA=%g Scope DE=%f, Aligned RA=%f DE=%f", currentRA,
                       currentDEC, alignedRA, alignedDEC);
            }
            else
            {
                DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
                       "TransformTelescopeToCelestial: Scope RA=%f Scope DE=%f, Aligned RA=%f DE=%f", currentRA, currentDEC,
                       alignedRA, alignedDEC);
            }
        }
#endif
        if (!aligned && (syncdata.lst != 0.0))
        {
            DEBUGF(DBG_SCOPE_STATUS, "Aligning with last sync delta RA %g DE %g", syncdata.deltaRA, syncdata.deltaDEC);
            // should check values are in range!
            alignedRA += syncdata.deltaRA;
            alignedDEC += syncdata.deltaDEC;
            if (alignedDEC > 90.0 || alignedDEC < -90.0)
            {
                alignedRA += 12.00;
                if (alignedDEC > 0.0)
                    alignedDEC = 180.0 - alignedDEC;
                else
                    alignedDEC = -180.0 - alignedDEC;
            }
            alignedRA = range24(alignedRA);
        }

#if defined WITH_ALIGN_GEEHALEL && !defined WITH_ALIGN
        alignedRA  = ghalignedRA;
        alignedDEC = ghalignedDEC;
#endif
#if defined WITH_ALIGN_GEEHALEL && defined WITH_ALIGN
        if (AlignMethodSP.sp[0].s == ISS_ON)
        {
            alignedRA  = ghalignedRA;
            alignedDEC = ghalignedDEC;
        }
#endif

        lnradec.rightascension  = alignedRA;
        lnradec.declination = alignedDEC;
        // Only update Alt/Az if the scope is not idle.
        if (TrackState != SCOPE_IDLE && TrackState != SCOPE_PARKED)
        {
            /* uses sidereal time, not local sidereal time */
            INDI::EquatorialToHorizontal(&lnradec, &m_Location, juliandate, &lnaltaz);
            horizvalues[0] = lnaltaz.azimuth;
            horizvalues[1] = lnaltaz.altitude;
            HorizontalCoordNP.update(horizvalues, (char **)horiznames, 2);
            HorizontalCoordNP.apply();
        }

        steppervalues[0] = currentRAEncoder;
        steppervalues[1] = currentDEEncoder;
        CurrentSteppersNP.update(steppervalues, (char **)steppernames, 2);
        CurrentSteppersNP.apply();

        mount->GetRAMotorStatus(RAStatusLP);
        mount->GetDEMotorStatus(DEStatusLP);
        RAStatusLP.apply();
        DEStatusLP.apply();

        periods[0] = mount->GetRAPeriod();
        periods[1] = mount->GetDEPeriod();
        PeriodsNP.update(periods, (char **)periodsnames, 2);
        PeriodsNP.apply();

        // Log all coords
        {
            char CurrentRAString[64] = {0}, CurrentDEString[64] = {0},
                                       AlignedRAString[64] = {0}, AlignedDEString[64] = {0},
                                               AZString[64] = {0}, ALString[64] = {0};
            fs_sexa(CurrentRAString, currentRA, 2, 3600);
            fs_sexa(CurrentDEString, currentDEC, 2, 3600);
            fs_sexa(AlignedRAString, alignedRA, 2, 3600);
            fs_sexa(AlignedDEString, alignedDEC, 2, 3600);
            fs_sexa(AZString, horizvalues[0], 2, 3600);
            fs_sexa(ALString, horizvalues[1], 2, 3600);

            LOGF_DEBUG("Scope RA (%s) DE (%s) Aligned RA (%s) DE (%s) AZ (%s) ALT (%s), PierSide (%s)",
                       CurrentRAString,
                       CurrentDEString,
                       AlignedRAString,
                       AlignedDEString,
                       AZString,
                       ALString,
                       pierSide == PIER_EAST ? "East" : (pierSide == PIER_WEST ? "West" : "Unknown"));
        }

        if (mount->HasAuxEncoders())
        {
            double auxencodervalues[2];
            const char *auxencodernames[] = { "AUXENCRASteps", "AUXENCDESteps" };
            auxencodervalues[0]           = mount->GetRAAuxEncoder();
            auxencodervalues[1]           = mount->GetDEAuxEncoder();
            AuxEncoderNP.update(auxencodervalues, (char **)auxencodernames, 2);
            AuxEncoderNP.apply();
        }

        if (gotoInProgress())
        {
            if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
            {
                // Goto iteration
                gotoparams.iterative_count += 1;
                LOGF_INFO(
                    "Iterative Goto (%d): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
                    gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),
                    3600 * fabs(gotoparams.detarget - currentDEC));
                if ((gotoparams.iterative_count <= GOTO_ITERATIVE_LIMIT) &&
                        (((3600 * fabs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) ||
                         ((3600 * fabs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION)))
                {
                    gotoparams.racurrent        = currentRA;
                    gotoparams.decurrent        = currentDEC;
                    gotoparams.racurrentencoder = currentRAEncoder;
                    gotoparams.decurrentencoder = currentDEEncoder;
                    EncoderTarget(&gotoparams);
                    // Start iterative slewing
                    LOGF_INFO(
                        "Iterative goto (%d): slew mount to RA increment = %d, DE increment = %d",
                        gotoparams.iterative_count, static_cast<int>(gotoparams.ratargetencoder - gotoparams.racurrentencoder),
                        static_cast<int>(gotoparams.detargetencoder - gotoparams.decurrentencoder));
                    mount->SlewTo(static_cast<int>(gotoparams.ratargetencoder - gotoparams.racurrentencoder),
                                  static_cast<int>(gotoparams.detargetencoder - gotoparams.decurrentencoder));
                }
                else
                {
                    if ((gotoparams.iterative_count > GOTO_ITERATIVE_LIMIT) &&
                            (((3600 * fabs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) ||
                             ((3600 * fabs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION)))
                    {
                        LOGF_INFO(
                            "Iterative Goto Limit reached (%d iterations): RA diff = %4.2f arcsecs DE diff = %4.2f "
                            "arcsecs",
                            gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),
                            3600 * fabs(gotoparams.detarget - currentDEC));
                    }

                    // For AstroEQ (needs an explicit :G command at the end of gotos)
                    mount->ResetMotions();

                    if ((RememberTrackState == SCOPE_TRACKING) || CoordSP.isSwitchOn("TRACK"))
                    {
                        std::string name;

                        if (RememberTrackState == SCOPE_TRACKING)
                        {
                            name = TrackModeSP.findOnSwitchName();;
                            mount->StartRATracking(GetRATrackRate());
                            mount->StartDETracking(GetDETrackRate());
                        }
                        else
                        {
                            name = TrackDefaultSP.findOnSwitchName();
                            mount->StartRATracking(GetDefaultRATrackRate());
                            mount->StartDETracking(GetDefaultDETrackRate());

#if 0
                            IUResetSwitch(TrackModeSP);
                            IUUpdateSwitch(TrackModeSP, &state, &name, 1);
                            TrackModeSP->s = IPS_BUSY;
                            IDSetSwitch(TrackModeSP, nullptr);
#endif
                        }

                        TrackState = SCOPE_TRACKING;
                        RememberTrackState = TrackState;

#if 0
                        TrackModeSP->s = IPS_BUSY;
                        IDSetSwitch(TrackModeSP, nullptr);
#endif
                        LOGF_INFO("Telescope slew is complete. Tracking %s...", name.c_str());
                    }
                    else
                    {
                        TrackState = SCOPE_IDLE;
                        RememberTrackState = TrackState;
                        LOG_INFO("Telescope slew is complete. Stopping...");
                    }
                    gotoparams.completed = true;
                }
            }
        }

#ifdef WITH_SCOPE_LIMITS
        if (horizon)
        {
            if (horizon->checkLimits(horizvalues[0], horizvalues[1], TrackState, gotoInProgress()))
                Abort();
        }
#endif

        if (TrackState == SCOPE_PARKING)
        {
            if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
            {
                currentRAEncoder = mount->GetRAEncoder();
                currentDEEncoder = mount->GetDEEncoder();
                parkRAEncoder    = GetAxis1Park();
                parkDEEncoder    = GetAxis2Park();
                if (std::abs(static_cast<int32_t>(parkRAEncoder - currentRAEncoder)) > PARKING_THRESHOLD )
                {
                    // Start slewing
                    LOGF_INFO("Motors while parking stopped, reparking mount: RA increment = %d, DE increment = %d",
                              static_cast<int32_t>(parkRAEncoder - currentRAEncoder), static_cast<int32_t>(parkDEEncoder - currentDEEncoder));
                    mount->SlewTo(static_cast<int32_t>(parkRAEncoder - currentRAEncoder),
                                  static_cast<int32_t>(parkDEEncoder - currentDEEncoder));

                    TrackState = SCOPE_PARKING;
                }
                else
                {
                    SetParked(true);
                }
            }
        }

        if (mount->HasPPEC())
        {
            if (PPECTrainingSP.getState() == IPS_BUSY)
            {
                bool intraining, inppec;
                mount->GetPPECStatus(&intraining, &inppec);
                if (!(intraining))
                {
                    LOG_INFO("PPEC Training completed.");
                    PPECTrainingSP[0].setState(ISS_ON);
                    PPECTrainingSP[1].setState(ISS_OFF);
                    PPECTrainingSP.setState(IPS_IDLE);
                    PPECTrainingSP.apply();
                }
            }
        }

        if (TrackState == SCOPE_AUTOHOMING)
        {
            uint32_t indexRA = 0, indexDE = 0;

            LOGF_DEBUG("Autohoming status: %d", AutohomeState);
            switch (AutohomeState)
            {
                case AUTO_HOME_IDLE:
                    AutohomeState = AUTO_HOME_IDLE;
                    TrackState    = SCOPE_IDLE;
                    RememberTrackState = TrackState;
                    LOG_INFO("Invalid status while Autohoming. Aborting");
                    break;
                case AUTO_HOME_WAIT_PHASE1:
                    if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
                    {
                        LOG_INFO("Autohome phase 1: end");
                        LOG_INFO(
                            "AutoHome phase 2: reading home position indexes for extra moves");
                        mount->GetRAIndexer();
                        mount->GetDEIndexer();
                        uint32_t raindex = mount->GetlastreadRAIndexer();
                        uint32_t deindex = mount->GetlastreadDEIndexer();
                        LOGF_INFO(
                            "AutoHome phase 2: read home position indexes: RA=0x%x DE=0x%x", raindex, deindex);
                        if (raindex == 0 || raindex == 0xFFFFFF)
                            ah_bIndexChanged_RA = false;
                        else
                            ah_bIndexChanged_RA = true;
                        if (deindex == 0 || deindex == 0xFFFFFF)
                            ah_bIndexChanged_DE = false;
                        else
                            ah_bIndexChanged_DE = true;
                        if (ah_bIndexChanged_RA)
                        {
                            LOGF_INFO(
                                "AutoHome phase 2: RA home index changed RA=0x%x, slewing again", raindex);
                            ah_iPosition_RA = mount->GetRAEncoder();
                            ah_iChanges     = (5 * mount->GetRAEncoderTotal()) / 360;
                            if (ah_bSlewingUp_RA)
                                ah_iPosition_RA = ah_iPosition_RA - ah_iChanges;
                            else
                                ah_iPosition_RA = ah_iPosition_RA + ah_iChanges;
                        }
                        if (ah_bIndexChanged_DE)
                        {
                            LOGF_INFO(
                                "AutoHome phase 2: DE home index changed DE=0x%x, slewing again", deindex);
                            ah_iPosition_DE = mount->GetDEEncoder();
                            ah_iChanges     = (5 * mount->GetDEEncoderTotal()) / 360;
                            if (ah_bSlewingUp_DE)
                                ah_iPosition_DE = ah_iPosition_DE - ah_iChanges;
                            else
                                ah_iPosition_DE = ah_iPosition_DE + ah_iChanges;
                        }
                        if ((ah_bIndexChanged_RA) || (ah_bIndexChanged_DE))
                        {
                            LOGF_INFO(
                                "AutoHome phase 2: slewing to RA=0x%x (up=%c) DE=0x%x (up=%c)", ah_iPosition_RA,
                                (ah_bSlewingUp_RA ? '1' : '0'), ah_iPosition_DE, (ah_bSlewingUp_DE ? '1' : '0'));
                            mount->AbsSlewTo(ah_iPosition_RA, ah_iPosition_DE, ah_bSlewingUp_RA, ah_bSlewingUp_DE);
                            LOG_INFO(
                                "Autohome phase 2: start slewing, waiting for motors to stop");
                        }
                        else
                        {
                            LOG_INFO("Autohome phase 2: nothing to do");
                        }
                        AutohomeState = AUTO_HOME_WAIT_PHASE2;
                    }
                    else
                    {
                        LOG_DEBUG("Autohome phase 1: Waiting for motors to stop");
                    }
                    break;
                case AUTO_HOME_WAIT_PHASE2:
                    if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
                    {
                        LOG_INFO("Autohome phase 2: end");
                        LOG_INFO("AutoHome phase 3: resetting home position indexes");
                        if (ah_bIndexChanged_RA)
                        {
                            uint32_t raindex = mount->GetlastreadRAIndexer();
                            mount->ResetRAIndexer();
                            mount->GetRAIndexer();
                            LOGF_INFO(
                                "AutoHome phase 3: resetting RA home index: 0x%x (was 0x%x)",
                                mount->GetlastreadRAIndexer(), raindex);
                        }
                        if (ah_bIndexChanged_DE)
                        {
                            uint32_t deindex = mount->GetlastreadDEIndexer();
                            mount->ResetDEIndexer();
                            mount->GetDEIndexer();
                            LOGF_INFO(
                                "AutoHome phase 3: resetting DE home index: 0x%x (was 0x%x)",
                                mount->GetlastreadDEIndexer(), deindex);
                        }
                        LOG_INFO(
                            "AutoHome phase 3: reading home position indexes to update directions");
                        if (ah_bIndexChanged_RA)
                        {
                            mount->GetRAIndexer();
                            if (mount->GetlastreadRAIndexer() == 0)
                                ah_bSlewingUp_RA = false;
                            else
                                ah_bSlewingUp_RA = true;
                            LOGF_INFO(
                                "AutoHome phase 3: reading RA home position index: RA=0x%x up=%c",
                                mount->GetlastreadRAIndexer(), (ah_bSlewingUp_RA ? '1' : '0'));
                        }
                        if (ah_bIndexChanged_DE)
                        {
                            mount->GetDEIndexer();
                            if (mount->GetlastreadDEIndexer() == 0)
                                ah_bSlewingUp_DE = false;
                            else
                                ah_bSlewingUp_DE = true;
                            LOGF_INFO(
                                "AutoHome phase 3: reading DE home position index: DE=0x%x up=%c",
                                mount->GetlastreadDEIndexer(), (ah_bSlewingUp_DE ? '1' : '0'));
                        }

                        if (!ah_bSlewingUp_RA)
                        {
                            LOG_INFO(
                                "AutoHome phase 3: starting RA negative slewing, waiting RA home indexer");
                            ah_waitRA = -1;
                            mount->SlewRA(-800.0);
                        }
                        if (!ah_bSlewingUp_DE)
                        {
                            LOG_INFO(
                                "AutoHome phase 3: starting DE negative slewing, waiting DE home indexer");
                            ah_waitDE = -1;
                            mount->SlewDE(-800.0);
                        }
                        AutohomeState = AUTO_HOME_WAIT_PHASE3;
                    }
                    else
                    {
                        LOG_DEBUG("Autohome phase 2: Waiting for motors to stop");
                    }
                    break;
                case AUTO_HOME_WAIT_PHASE3:
                    if (mount->IsRARunning())
                    {
                        if (ah_waitRA < 0)
                        {
                            mount->GetRAIndexer();
                            if ((indexRA = mount->GetlastreadRAIndexer()) != 0xFFFFFF)
                            {
                                ah_waitRA = 3000 / getCurrentPollingPeriod();
                                LOGF_INFO(
                                    "Autohome phase 3: detected RA Index changed, waiting %d poll periods",
                                    ah_waitRA);
                            }
                        }
                        else
                            ah_waitRA -= 1;
                        if (ah_waitRA == 0)
                        {
                            LOG_INFO("Autohome phase 3: stopping RA");
                            mount->StopRA();
                        }
                    }
                    if (mount->IsDERunning())
                    {
                        if (ah_waitDE < 0)
                        {
                            mount->GetDEIndexer();
                            if ((indexDE = mount->GetlastreadDEIndexer()) != 0xFFFFFF)
                            {
                                ah_waitDE = 3000 / getCurrentPollingPeriod();
                                LOGF_INFO(
                                    "Autohome phase 3: detected DE Index changed, waiting %d poll periods",
                                    ah_waitDE);
                            }
                        }
                        else
                            ah_waitDE -= 1;
                        if (ah_waitDE == 0)
                        {
                            LOG_INFO("Autohome phase 3: stopping DE");
                            mount->StopDE();
                        }
                    }
                    if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
                    {
                        if (!ah_bSlewingUp_RA)
                        {
                            mount->ResetRAIndexer();
                            mount->GetRAIndexer();
                            LOGF_INFO(
                                "AutoHome phase 3: resetting RA home index: 0x%x (was 0x%x)",
                                mount->GetlastreadRAIndexer(), indexRA);
                            ah_bSlewingUp_RA = true;
                        }
                        if (!ah_bSlewingUp_DE)
                        {
                            mount->ResetDEIndexer();
                            mount->GetDEIndexer();
                            LOGF_INFO(
                                "AutoHome phase 3: resetting DE home index: 0x%x (was 0x%x)",
                                mount->GetlastreadDEIndexer(), indexDE);
                            ah_bSlewingUp_DE = true;
                        }
                        LOG_INFO("Autohome phase 3: end");
                        LOG_INFO("Autohome phase 4: *** find the home position index ***");
                        LOG_INFO(
                            "AutoHome phase 4: starting RA positive slewing, waiting RA home indexer");
                        ah_waitRA           = -1;
                        ah_bIndexChanged_RA = false;
                        mount->SlewRA(400.0);

                        LOG_INFO(
                            "AutoHome phase 4: starting DE positive slewing, waiting DE home indexer");
                        ah_waitDE = -1;
                        mount->SlewDE(400.0);
                        ah_bIndexChanged_DE = false;
                        AutohomeState       = AUTO_HOME_WAIT_PHASE4;
                    }
                    break;
                case AUTO_HOME_WAIT_PHASE4:
                    if (!ah_bIndexChanged_RA)
                    {
                        mount->GetRAIndexer();
                        ah_iPosition_RA = mount->GetlastreadRAIndexer();
                        if (ah_iPosition_RA != 0)
                        {
                            ah_bIndexChanged_RA      = true;
                            ah_sHomeIndexPosition_RA = ah_iPosition_RA;
                            LOGF_INFO(
                                "Autohome phase 4: detected RA Home index: 0x%x, stopping motor", ah_iPosition_RA);
                            mount->StopRA();
                        }
                    }
                    if (!ah_bIndexChanged_DE)
                    {
                        mount->GetDEIndexer();
                        ah_iPosition_DE = mount->GetlastreadDEIndexer();
                        if (ah_iPosition_DE != 0)
                        {
                            ah_bIndexChanged_DE      = true;
                            ah_sHomeIndexPosition_DE = ah_iPosition_DE;
                            LOGF_INFO(
                                "Autohome phase 4: detected DE Home index: 0x%x, stopping motor", ah_iPosition_DE);
                            mount->StopDE();
                        }
                    }
                    if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
                    {
                        LOG_INFO("Autohome phase 4: end");
                        LOG_INFO("Autohome phase 5: Moving back 10 deg.");
                        ah_iChanges     = (10 * mount->GetRAEncoderTotal()) / 360;
                        ah_iPosition_RA = ah_iPosition_RA - ah_iChanges;
                        ah_iChanges     = (10 * mount->GetDEEncoderTotal()) / 360;
                        ah_iPosition_DE = ah_iPosition_DE - ah_iChanges;
                        LOGF_INFO(
                            "AutoHome phase 5: slewing to RA=0x%x (up=%c) DE=0x%x (up=%c)", ah_iPosition_RA, '0',
                            ah_iPosition_DE, '0');
                        mount->AbsSlewTo(ah_iPosition_RA, ah_iPosition_DE, false, false);
                        AutohomeState = AUTO_HOME_WAIT_PHASE5;
                    }
                    break;
                case AUTO_HOME_WAIT_PHASE5:
                    if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
                    {
                        LOG_INFO("Autohome phase 5: end");
                        LOG_INFO("Autohome phase 6: Goto Home Position");
                        LOGF_INFO(
                            "AutoHome phase 6: slewing to RA=0x%x (up=%c) DE=0x%x (up=%c)", ah_sHomeIndexPosition_RA,
                            '1', ah_sHomeIndexPosition_DE, '1');
                        mount->AbsSlewTo(ah_sHomeIndexPosition_RA, ah_sHomeIndexPosition_DE, true, true);
                        AutohomeState = AUTO_HOME_WAIT_PHASE6;
                    }
                    else
                    {
                        LOG_DEBUG("Autohome phase 5: Waiting for motors to stop");
                    }
                    break;
                case AUTO_HOME_WAIT_PHASE6:
                    if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
                    {
                        LOG_INFO("Autohome phase 6: end");
                        LOGF_INFO("AutoHome phase 6: Mount at RA=0x%x DE=0x%x",
                                  mount->GetRAEncoder(), mount->GetDEEncoder());
                        LOGF_INFO(
                            "Autohome: Mount at Home Position, setting encoders RA=0x%x DE=0X%x",
                            mount->GetRAEncoderHome(), mount->GetDEEncoderHome());
                        mount->SetRAAxisPosition(mount->GetRAEncoderHome());
                        mount->SetDEAxisPosition(mount->GetDEEncoderHome());
                        TrackState    = SCOPE_IDLE;
                        RememberTrackState = TrackState;
                        AutohomeState = AUTO_HOME_IDLE;
                        HomeSP.setState(IPS_IDLE);
                        HomeSP.reset();
                        HomeSP.apply();
                        LOG_INFO("Autohome: end");
                    }
                    else
                    {
                        LOG_DEBUG("Autohome phase 6: Waiting for motors to stop");
                    }
                    break;
                default:
                    LOGF_WARN("Unknown Autohome status %d: aborting", AutohomeState);
                    Abort();
                    break;
            }
        }
    }
    catch (EQModError &e)
    {
        return (e.DefaultHandleException(this));
    }

    // This should be kept last so that any TRACK_STATE
    // change are reflected in EQNP property in INDI::Telescope

    NewRaDec(alignedRA, alignedDEC);
    return true;
}

void EQMod::EncodersToRADec(uint32_t rastep, uint32_t destep, double lst, double *ra, double *de, double *ha,
                            TelescopePierSide *pierSide)
{
    double RACurrent = 0.0, DECurrent = 0.0, HACurrent = 0.0;
    TelescopePierSide p;
    HACurrent = EncoderToHours(rastep, zeroRAEncoder, totalRAEncoder, Hemisphere);
    RACurrent = HACurrent + lst;
    DECurrent = EncoderToDegrees(destep, zeroDEEncoder, totalDEEncoder, Hemisphere);
    //IDLog("EncodersToRADec: destep=%6X zeroDEncoder=%6X totalDEEncoder=%6x DECurrent=%f\n", destep, zeroDEEncoder , totalDEEncoder, DECurrent);
    if (Hemisphere == NORTH)
    {
        if ((DECurrent > 90.0) && (DECurrent <= 270.0))
        {
            RACurrent = RACurrent - 12.0;
            p = PIER_EAST;
        }
        else
            p = PIER_WEST;
    }
    else if ((DECurrent <= 90.0) || (DECurrent > 270.0))
    {
        RACurrent = RACurrent + 12.0;
        //currentPierSide = EAST;
        p = PIER_EAST;
    }
    else
        p = PIER_WEST;
    //currentPierSide = WEST;
    HACurrent = rangeHA(HACurrent);
    RACurrent = range24(RACurrent);
    DECurrent = rangeDec(DECurrent);
    *ra       = RACurrent;
    *de       = DECurrent;
    if (ha)
        *ha = HACurrent;
    if (pierSide)
        *pierSide = p;
}

double EQMod::EncoderToHours(uint32_t step, uint32_t initstep, uint32_t totalstep, enum Hemisphere h)
{
    double result = 0.0;
    if (step > initstep)
    {
        result = (static_cast<double>(step - initstep) / totalstep) * 24.0;
        result = 24.0 - result;
    }
    else
    {
        result = (static_cast<double>(initstep - step) / totalstep) * 24.0;
    }

    if (h == NORTH)
        result = range24(result + 6.0);
    else
        result = range24((24 - result) + 6.0);
    return result;
}

double EQMod::EncoderToDegrees(uint32_t step, uint32_t initstep, uint32_t totalstep, enum Hemisphere h)
{
    double result = 0.0;
    if (step > initstep)
    {
        result = (static_cast<double>(step - initstep) / totalstep) * 360.0;
    }
    else
    {
        result = (static_cast<double>(initstep - step) / totalstep) * 360.0;
        result = 360.0 - result;
    }
    //IDLog("EncodersToDegrees: step=%6X initstep=%6x result=%f hemisphere %s \n", step, initstep, result, (h==NORTH?"North":"South"));
    if (h == NORTH)
        result = range360(result);
    else
        result = range360(360.0 - result);
    //IDLog("EncodersToDegrees: returning result=%f\n", result);

    return result;
}

double EQMod::EncoderFromHour(double hour, uint32_t initstep, uint32_t totalstep, enum Hemisphere h)
{
    double shifthour = 0.0;
    shifthour        = range24(hour - 6);
    if (h == NORTH)
        if (shifthour < 12.0)
            return round(initstep - ((shifthour / 24.0) * totalstep));
        else
            return round(initstep + (((24.0 - shifthour) / 24.0) * totalstep));
    else if (shifthour < 12.0)
        return round(initstep + ((shifthour / 24.0) * totalstep));
    else
        return round(initstep - (((24.0 - shifthour) / 24.0) * totalstep));
}

double EQMod::EncoderFromRA(double ratarget, TelescopePierSide p, double lst, uint32_t initstep,
                            uint32_t totalstep, enum Hemisphere h)
{
    double ha = 0.0;
    ha        = ratarget - lst;

    //    if ((h == NORTH && p == PIER_EAST) || (h == SOUTH && p == PIER_WEST))
    if (p == PIER_EAST)
        ha = ha + 12.0;

    ha = range24(ha);
    return EncoderFromHour(ha, initstep, totalstep, h);
}

double EQMod::EncoderFromDegree(double degree, uint32_t initstep, uint32_t totalstep, enum Hemisphere h)
{
    double target = 0.0;
    target        = range360(degree);
    if (h == SOUTH)
        target = 360.0 - target;
    if (target > 270.0)
        target -= 360.0;
    return round(initstep + ((target / 360.0) * totalstep));
}

double EQMod::EncoderFromDec(double detarget, TelescopePierSide p, uint32_t initstep, uint32_t totalstep,
                             enum Hemisphere h)
{
    if ((h == NORTH && p == PIER_EAST) || (h == SOUTH && p == PIER_WEST))
        detarget = 180.0 - detarget;
    return EncoderFromDegree(detarget, initstep, totalstep, h);
}

void EQMod::SetSouthernHemisphere(bool southern)
{
    const char *hemispherenames[] = { "NORTH", "SOUTH" };
    ISState hemispherevalues[2];
    LOGF_DEBUG("Set southern %s", (southern ? "true" : "false"));
    if (southern)
        Hemisphere = SOUTH;
    else
        Hemisphere = NORTH;
    RAInverted = (Hemisphere == SOUTH);
    UpdateDEInverted();
    if (Hemisphere == NORTH)
    {
        hemispherevalues[0] = ISS_ON;
        hemispherevalues[1] = ISS_OFF;
        HemisphereSP.update(hemispherevalues, (char **)hemispherenames, 2);
    }
    else
    {
        hemispherevalues[0] = ISS_OFF;
        hemispherevalues[1] = ISS_ON;
        HemisphereSP.update(hemispherevalues, (char **)hemispherenames, 2);
    }
    HemisphereSP.setState(IPS_IDLE);
    HemisphereSP.apply();
}

void EQMod::UpdateDEInverted()
{
    bool prev = DEInverted;
    DEInverted = (Hemisphere == SOUTH) ^ (ReverseDECSP[0].getState() == ISS_ON);
    if (DEInverted != prev)
        LOGF_DEBUG("Set DEInverted %s", DEInverted ? "true" : "false");
}

void EQMod::EncoderTarget(GotoParams *g)
{
    double r, d;
    double ha = 0.0;
    double juliandate;
    double lst;
    uint32_t targetraencoder = 0, targetdecencoder = 0;
    bool outsidelimits = false;
    r                  = g->ratarget;
    d                  = g->detarget;

    juliandate = getJulianDate();
    lst        = getLst(juliandate, getLongitude());

    if (g->pier_side == PIER_UNKNOWN)
    {
        // decide pier side and keep it consistent in iterative calls
        ha = rangeHA(r - lst);
        if (ha < 0.0)
        {
            // target WEST
            g->pier_side = PIER_EAST;
        }
        else
        {
            g->pier_side = PIER_WEST;
        }
    }

    targetraencoder  = EncoderFromRA(r, g->pier_side, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
    targetdecencoder = EncoderFromDec(d, g->pier_side, zeroDEEncoder, totalDEEncoder, Hemisphere);

    if (g->checklimits)
    {
        if (Hemisphere == NORTH)
        {
            assert(g->limiteast <= g->limitwest);
            if ((targetraencoder < g->limiteast) || (targetraencoder > g->limitwest))
                outsidelimits = true;
        }
        else
        {
            assert(g->limiteast >= g->limitwest);
            if ((targetraencoder > g->limiteast) || (targetraencoder < g->limitwest))
                outsidelimits = true;
        }
    }
    g->outsidelimits   = outsidelimits;
    g->ratargetencoder = targetraencoder;
    g->detargetencoder = targetdecencoder;
}

double EQMod::GetRATrackRate()
{
    double rate = 0.0;
    ISwitch *sw;
    sw = TrackModeSP.findOnSwitch();
    if (!sw)
        return 0.0;
    if (!strcmp(sw->name, "TRACK_SIDEREAL"))
    {
        rate = TRACKRATE_SIDEREAL;
    }
    else if (!strcmp(sw->name, "TRACK_LUNAR"))
    {
        rate = TRACKRATE_LUNAR;
    }
    else if (!strcmp(sw->name, "TRACK_SOLAR"))
    {
        rate = TRACKRATE_SOLAR;
    }
    else if (!strcmp(sw->name, "TRACK_CUSTOM"))
    {
        auto number = TrackRateNP.findWidgetByName("TRACK_RATE_RA");
        if(number)
            rate = number->getValue();
    }
    else
        return 0.0;
    if (RAInverted)
        rate = -rate;
    return rate;
}

double EQMod::GetDETrackRate()
{
    double rate = 0.0;
    ISwitch *sw;
    sw = TrackModeSP.findOnSwitch();
    if (!sw)
        return 0.0;
    if (!strcmp(sw->name, "TRACK_SIDEREAL"))
    {
        rate = 0.0;
    }
    else if (!strcmp(sw->name, "TRACK_LUNAR"))
    {
        rate = 0.0;
    }
    else if (!strcmp(sw->name, "TRACK_SOLAR"))
    {
        rate = 0.0;
    }
    else if (!strcmp(sw->name, "TRACK_CUSTOM"))
    {
        auto number = TrackRateNP.findWidgetByName("TRACK_RATE_DE");
        if(number)
            rate = number->getValue();
    }
    else
        return 0.0;
    if (DEInverted)
        rate = -rate;
    return rate;
}

double EQMod::GetDefaultRATrackRate()
{
    double rate = 0.0;
    auto element = TrackDefaultSP.findOnSwitch();
    if (!element)
        return 0.0;
    if (element->isNameMatch("TRACK_SIDEREAL"))
    {
        rate = TRACKRATE_SIDEREAL;
    }
    else if (element->isNameMatch("TRACK_LUNAR"))
    {
        rate = TRACKRATE_LUNAR;
    }
    else if (element->isNameMatch("TRACK_SOLAR"))
    {
        rate = TRACKRATE_SOLAR;
    }
    else if (element->isNameMatch("TRACK_CUSTOM"))
    {
        auto number = TrackRateNP.findWidgetByName("TRACK_RATE_RA");
        if(number)
            rate = number->getValue();
    }
    else
        return 0.0;
    if (RAInverted)
        rate = -rate;
    return rate;
}

double EQMod::GetDefaultDETrackRate()
{
    double rate = 0.0;
    ISwitch *sw;
    sw = TrackDefaultSP.findOnSwitch();
    if (!sw)
        return 0.0;
    if (!strcmp(sw->name, "TRACK_SIDEREAL"))
    {
        rate = 0.0;
    }
    else if (!strcmp(sw->name, "TRACK_LUNAR"))
    {
        rate = 0.0;
    }
    else if (!strcmp(sw->name, "TRACK_SOLAR"))
    {
        rate = 0.0;
    }
    else if (!strcmp(sw->name, "TRACK_CUSTOM"))
    {
        auto number = TrackRateNP.findWidgetByName("TRACK_RATE_DE");
        if(number)
            rate = number->getValue();
    }
    else
        return 0.0;
    if (DEInverted)
        rate = -rate;
    return rate;
}

bool EQMod::gotoInProgress()
{
    return (!gotoparams.completed);
}

bool EQMod::Goto(double r, double d)
{
    double juliandate;
#ifdef WITH_SCOPE_LIMITS
    INDI::IEquatorialCoordinates gotoradec;
    INDI::IHorizontalCoordinates gotoaltaz;
    double gotoaz;
    double gotoalt;
#endif

    if ((TrackState == SCOPE_SLEWING) || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
    {
        LOG_WARN("Can not perform goto while goto/park in progress, or scope parked.");
        //        EqNP.s = IPS_IDLE;
        //        IDSetNumber(&EqNP, nullptr);
        //        return true;
        return false;
    }

    juliandate = getJulianDate();

#ifdef WITH_SCOPE_LIMITS
    gotoradec.rightascension  = r;
    gotoradec.declination = d;
    INDI::EquatorialToHorizontal(&gotoradec, &m_Location, juliandate, &gotoaltaz);
    gotoaz  = gotoaltaz.azimuth;
    gotoalt = gotoaltaz.altitude;
    if (horizon)
    {
        if (!horizon->inGotoLimits(gotoaz, gotoalt))
        {
            LOG_WARN("Goto outside Horizon Limits.");
            //            EqNP.s = IPS_IDLE;
            //            IDSetNumber(&EqNP, nullptr);
            //            return true;
            return false;
        }
    }
#endif

    LOGF_INFO("Starting Goto RA=%g DE=%g (current RA=%g DE=%g)", r, d, currentRA, currentDEC);
    targetRA  = r;
    targetDEC = d;
    char RAStr[64], DecStr[64];

    // Compute encoder targets and check RA limits if forced
    bzero(&gotoparams, sizeof(gotoparams));
    gotoparams.ratarget  = r;
    gotoparams.detarget  = d;
    gotoparams.racurrent = currentRA;
    gotoparams.decurrent = currentDEC;
    bool aligned         = false;
#ifdef WITH_ALIGN_GEEHALEL
    double ghratarget = r, ghdetarget = d;
    if (AlignMethodSP.sp[0].s == ISS_ON)
    {
        aligned = true;
        if (align)
        {
            align->AlignGoto(syncdata, juliandate, &m_Location, &ghratarget, &ghdetarget);
            LOGF_INFO("Aligned Eqmod Goto RA=%g DE=%g (target RA=%g DE=%g)", ghratarget, ghdetarget,
                      r, d);
        }
        else
        {
            if (syncdata.lst != 0.0)
            {
                ghratarget = gotoparams.ratarget - syncdata.deltaRA;
                ghdetarget = gotoparams.detarget - syncdata.deltaDEC;
                LOGF_INFO("Failed Eqmod Goto RA=%g DE=%g (target RA=%g DE=%g)", ghratarget,
                          ghdetarget, r, d);
            }
        }
    }
#endif
#ifdef WITH_ALIGN
    if (AlignMethodSP.sp[1].s == ISS_ON)
    {
        TelescopeDirectionVector TDV;
        aligned = true;
        if (!TransformCelestialToTelescope(r, d, 0.0, TDV))
        {
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
                   "Failed TransformCelestialToTelescope:  RA=%lf DE=%lf, Goto RA=%lf DE=%lf", r, d, gotoparams.ratarget,
                   gotoparams.detarget);
            if (syncdata.lst != 0.0)
            {
                gotoparams.ratarget -= syncdata.deltaRA;
                gotoparams.detarget -= syncdata.deltaDEC;
            }
        }
        else
        {
            INDI::IEquatorialCoordinates RaDec;
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV, RaDec);
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
                   "TransformCelestialToTelescope: RA=%lf DE=%lf, TDV (x :%lf, y: %lf, z: %lf), local hour RA %lf DEC %lf",
                   r, d, TDV.x, TDV.y, TDV.z, RaDec.rightascension, RaDec.declination);
            gotoparams.ratarget = RaDec.rightascension;
            gotoparams.detarget = RaDec.declination;
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
                   "TransformCelestialToTelescope: RA=%lf DE=%lf, Goto RA=%lf DE=%lf", r, d, gotoparams.ratarget,
                   gotoparams.detarget);
        }
    }
#endif

    if (!aligned && (syncdata.lst != 0.0))
    {
        gotoparams.ratarget -= syncdata.deltaRA;
        gotoparams.detarget -= syncdata.deltaDEC;
    }

#if defined WITH_ALIGN_GEEHALEL && !defined WITH_ALIGN
    if (aligned)
    {
        gotoparams.ratarget = ghratarget;
        gotoparams.detarget = ghdetarget;
    }
#endif
#if defined WITH_ALIGN_GEEHALEL && defined WITH_ALIGN
    if (aligned && (AlignMethodSP.sp[0].s == ISS_ON))
    {
        LOGF_INFO("Setting Eqmod Goto RA=%g DE=%g (target RA=%g DE=%g)", ghratarget, ghdetarget,
                  r, d);
        gotoparams.ratarget = ghratarget;
        gotoparams.detarget = ghdetarget;
    }
#endif

    gotoparams.racurrentencoder = currentRAEncoder;
    gotoparams.decurrentencoder = currentDEEncoder;
    gotoparams.completed        = false;
    gotoparams.checklimits      = true;
    gotoparams.pier_side        = TargetPier;
    gotoparams.outsidelimits    = false;
    if (Hemisphere == NORTH)
    {
        gotoparams.limiteast        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 13h
        gotoparams.limitwest        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 23h
    }
    else
    {
        gotoparams.limiteast        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // ??
        gotoparams.limitwest        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // ??
    }

    if (gotoparams.pier_side != PIER_UNKNOWN)
    {
        LOG_WARN("Enforcing the pier side prevents a meridian flip and may lead to collisions of the telescope with obstacles.");
    }

    EncoderTarget(&gotoparams);

    if (gotoparams.outsidelimits)
    {
        LOGF_INFO("Target is unreachable, aborting (target encoders %u %u)", gotoparams.ratargetencoder,
                  gotoparams.detargetencoder);
        Abort();
        return false;
    }

    try
    {
        // stop motor
        mount->StopRA();
        mount->StopDE();
        // Start slewing
        LOGF_INFO("Slewing mount: RA increment = %d, DE increment = %d",
                  static_cast<int>(gotoparams.ratargetencoder - gotoparams.racurrentencoder),
                  static_cast<int>(gotoparams.detargetencoder - gotoparams.decurrentencoder));
        mount->SlewTo(static_cast<int>(gotoparams.ratargetencoder - gotoparams.racurrentencoder),
                      static_cast<int>(gotoparams.detargetencoder - gotoparams.decurrentencoder));
    }
    catch (EQModError &e)
    {
        return (e.DefaultHandleException(this));
    }

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // This is already set before Goto in INDI::Telescope
    //RememberTrackState = TrackState;

    TrackState         = SCOPE_SLEWING;

    //EqREqNP.s = IPS_BUSY;
    //EqNP.s = IPS_BUSY;

#if 0
    // 2017-08-01 Jasem: We should set TrackState to IPS_IDLE instead here?
    TrackModeSP->s = IPS_IDLE;
    IDSetSwitch(TrackModeSP, nullptr);
#endif

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool EQMod::Park()
{
    if (!isParked())
    {
        if (TrackState == SCOPE_SLEWING)
        {
            LOG_INFO("Can not park while slewing...");
            ParkSP.setState(IPS_ALERT);
            ParkSP.apply();
            return false;
        }

        try
        {
            // stop motor
            mount->StopRA();
            mount->StopDE();
            currentRAEncoder = mount->GetRAEncoder();
            currentDEEncoder = mount->GetDEEncoder();
            parkRAEncoder    = GetAxis1Park();
            parkDEEncoder    = GetAxis2Park();
            // Start slewing
            LOGF_INFO("Parking mount: RA increment = %d, DE increment = %d",
                      static_cast<int32_t>(parkRAEncoder - currentRAEncoder), static_cast<int32_t>(parkDEEncoder - currentDEEncoder));
            mount->SlewTo(static_cast<int32_t>(parkRAEncoder - currentRAEncoder),
                          static_cast<int32_t>(parkDEEncoder - currentDEEncoder));
        }
        catch (EQModError e)
        {
            return (e.DefaultHandleException(this));
        }
        //TrackModeSP->s = IPS_IDLE;
        //IDSetSwitch(TrackModeSP, nullptr);
        TrackState = SCOPE_PARKING;
        //        ParkSP.s   = IPS_BUSY;
        //        IDSetSwitch(&ParkSP, nullptr);
        LOG_INFO("Mount park in progress...");

        return true;
    }

    return false;
}

bool EQMod::UnPark()
{
    SetParked(false);
    return true;
}

bool EQMod::Sync(double ra, double dec)
{
    double juliandate;
    double lst;
    SyncData tmpsyncdata;
    double ha;
    TelescopePierSide pier_side;

    // get current mount position asap
    tmpsyncdata.telescopeRAEncoder  = mount->GetRAEncoder();
    tmpsyncdata.telescopeDECEncoder = mount->GetDEEncoder();

    juliandate = getJulianDate();
    lst        = getLst(juliandate, getLongitude());

    if (TrackState != SCOPE_TRACKING)
    {
        //        EqNP.s = IPS_ALERT;
        //        IDSetNumber(&EqNP, nullptr);
        LOG_WARN("Syncs are allowed only when Tracking");
        return false;
    }
    /* remember the two last syncs to compute Polar alignment */

    tmpsyncdata.lst       = lst;
    tmpsyncdata.jd        = juliandate;
    tmpsyncdata.targetRA  = ra;
    tmpsyncdata.targetDEC = dec;

    if (TargetPier == PIER_UNKNOWN)
    {
        ha = rangeHA(ra - lst);
        if (ha < 0.0)
        {
            // target WEST
            pier_side = PIER_EAST;
        }
        else
        {
            pier_side = PIER_WEST;
        }
    }
    else
    {
        pier_side = TargetPier;
    }
    tmpsyncdata.targetRAEncoder  = EncoderFromRA(ra, pier_side, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
    tmpsyncdata.targetDECEncoder = EncoderFromDec(dec, pier_side, zeroDEEncoder, totalDEEncoder, Hemisphere);

    try
    {
        EncodersToRADec(tmpsyncdata.telescopeRAEncoder, tmpsyncdata.telescopeDECEncoder, lst, &tmpsyncdata.telescopeRA,
                        &tmpsyncdata.telescopeDEC, nullptr, nullptr);
    }
    catch (EQModError e)
    {
        return (e.DefaultHandleException(this));
    }

    tmpsyncdata.deltaRA         = tmpsyncdata.targetRA - tmpsyncdata.telescopeRA;
    tmpsyncdata.deltaDEC        = tmpsyncdata.targetDEC - tmpsyncdata.telescopeDEC;
    tmpsyncdata.deltaRAEncoder  = static_cast<int>(tmpsyncdata.targetRAEncoder - tmpsyncdata.telescopeRAEncoder);
    tmpsyncdata.deltaDECEncoder = static_cast<int>(tmpsyncdata.targetDECEncoder - tmpsyncdata.telescopeDECEncoder);
#ifdef WITH_ALIGN_GEEHALEL
    if (align && !isStandardSync())
    {
        align->AlignSync(syncdata, tmpsyncdata);
        //return true;
    }
#endif
#ifdef WITH_ALIGN
    if (!isStandardSync())
    {
        AlignmentDatabaseEntry NewEntry;
        INDI::IEquatorialCoordinates RaDec;
        RaDec.rightascension  = tmpsyncdata.telescopeRA;
        RaDec.declination = tmpsyncdata.telescopeDEC;
        //NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
        NewEntry.ObservationJulianDate = juliandate;
        NewEntry.RightAscension        = ra;
        NewEntry.Declination           = dec;
        NewEntry.TelescopeDirection    = TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);
        NewEntry.PrivateDataSize       = 0;
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New sync point Date %lf RA %lf DEC %lf TDV(x %lf y %lf z %lf)",
               NewEntry.ObservationJulianDate, NewEntry.RightAscension, NewEntry.Declination,
               NewEntry.TelescopeDirection.x, NewEntry.TelescopeDirection.y, NewEntry.TelescopeDirection.z);
        if (!CheckForDuplicateSyncPoint(NewEntry, 0.01))
        {
            GetAlignmentDatabase().push_back(NewEntry);

            // Tell the client about size change
            UpdateSize();

            // Tell the math plugin to reinitialise
            Initialise(this);

        }
    }
#endif
#if defined WITH_ALIGN_GEEHALEL || defined WITH_ALIGN
    if (isStandardSync())
#endif
    {
#ifdef WITH_ALIGN_GEEHALEL
        if (align && isStandardSync())
            align->AlignStandardSync(syncdata, &tmpsyncdata, &m_Location);
#endif
        syncdata2 = syncdata;
        syncdata  = tmpsyncdata;

        StandardSyncNP.findWidgetByName("STANDARDSYNC_RA")->setValue(syncdata.deltaRA);
        StandardSyncNP.findWidgetByName("STANDARDSYNC_DE")->setValue(syncdata.deltaDEC);
        StandardSyncNP.apply();

        StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_JD")->setValue(juliandate);
        StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_SYNCTIME")->setValue(lst);
        StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_CELESTIAL_RA")->setValue(syncdata.targetRA);
        StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_CELESTIAL_DE")->setValue(syncdata.targetDEC);
        StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_TELESCOPE_RA")->setValue(syncdata.telescopeRA);
        StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_TELESCOPE_DE")->setValue(syncdata.telescopeDEC);
        StandardSyncPointNP.apply();

        LOGF_INFO("Mount Synced (deltaRA = %.6f deltaDEC = %.6f)", syncdata.deltaRA, syncdata.deltaDEC);
        if (syncdata2.lst != 0.0)
        {
            computePolarAlign(syncdata2, syncdata, getLatitude(), &tpa_alt, &tpa_az);
            SyncPolarAlignNP.findWidgetByName("SYNCPOLARALIGN_ALT")->setValue(tpa_alt);
            SyncPolarAlignNP.findWidgetByName("SYNCPOLARALIGN_AZ")->setValue(tpa_az);
            SyncPolarAlignNP.apply();
            LOGF_DEBUG("computePolarAlign: Telescope Polar Axis: alt = %g, az = %g", tpa_alt, tpa_az);
        }
    }
    return true;
}

IPState EQMod::GuideNorth(uint32_t ms)
{
    if (ms < MinPulseN->value)
    {
        return IPS_IDLE;
    }

    double rateshift = TRACKRATE_SIDEREAL * GuideRateNP.findWidgetByName("GUIDE_RATE_NS")->getValue();
    LOGF_DEBUG("Timed guide North %d ms at rate %g %s", ms, rateshift, DEInverted ? "(Inverted)" : "");

    IPState pulseState = IPS_BUSY;

    if (DEInverted)
        rateshift = -rateshift;
    try
    {
        if (ms >= MinPulseTimerN->value)
        {
            pulseInProgress |= 1;
            GuideTimerNS = IEAddTimer(ms, (IE_TCF *)timedguideNSCallback, this);
            mount->StartDETracking(GetDETrackRate() + rateshift);
        }
        else
        {
            // We should be done once the synchronous guide is complete
            pulseState = IPS_IDLE;

            struct timespec starttime, endtime;
            clock_gettime(CLOCK_MONOTONIC, &starttime);
            mount->StartDETracking(GetDETrackRate() + rateshift);
            clock_gettime(CLOCK_MONOTONIC, &endtime);
            double elapsed =
                (endtime.tv_sec - starttime.tv_sec) * 1000.0 + ((endtime.tv_nsec - starttime.tv_nsec) / 1000000.0);
            if (elapsed < ms)
            {
                uint32_t left = ms - elapsed;
                usleep(left * 1000);
            }
            try
            {
                mount->StartDETracking(GetDETrackRate());
            }
            catch (EQModError e)
            {
                if (!(e.DefaultHandleException(this)))
                {
                    DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_WARNING,
                                "Timed guide North/South Error: can not restart tracking");
                }
            }
            GuideComplete(AXIS_DE);
            DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_DEBUG, "End Timed guide North/South");
        }
    }
    catch (EQModError e)
    {
        e.DefaultHandleException(this);
        return IPS_ALERT;
    }

    return pulseState;
}

IPState EQMod::GuideSouth(uint32_t ms)
{
    if (ms < MinPulseN->value)
    {
        return IPS_IDLE;
    }

    double rateshift = 0.0;
    rateshift        = TRACKRATE_SIDEREAL * GuideRateNP.findWidgetByName("GUIDE_RATE_NS")->getValue();
    LOGF_DEBUG("Timed guide South %d ms at rate %g %s", ms, rateshift, DEInverted ? "(Inverted)" : "");

    IPState pulseState = IPS_BUSY;

    if (DEInverted)
        rateshift = -rateshift;
    try
    {
        if (ms >= MinPulseTimerN->value)
        {
            pulseInProgress |= 1;
            GuideTimerNS = IEAddTimer(ms, (IE_TCF *)timedguideNSCallback, this);
            mount->StartDETracking(GetDETrackRate() - rateshift);
        }
        else
        {
            // We should be done once the synchronous guide is complete
            pulseState = IPS_IDLE;

            struct timespec starttime, endtime;
            clock_gettime(CLOCK_MONOTONIC, &starttime);
            mount->StartDETracking(GetDETrackRate() - rateshift);
            clock_gettime(CLOCK_MONOTONIC, &endtime);
            double elapsed =
                (endtime.tv_sec - starttime.tv_sec) * 1000.0 + ((endtime.tv_nsec - starttime.tv_nsec) / 1000000.0);
            if (elapsed < ms)
            {
                uint32_t left = ms - elapsed;
                usleep(left * 1000);
            }
            try
            {
                mount->StartDETracking(GetDETrackRate());
            }
            catch (EQModError e)
            {
                if (!(e.DefaultHandleException(this)))
                {
                    DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_WARNING,
                                "Timed guide North/South Error: can not restart tracking");
                }
            }
            GuideComplete(AXIS_DE);
            DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_DEBUG, "End Timed guide North/South");
        }
    }
    catch (EQModError e)
    {
        e.DefaultHandleException(this);
        return IPS_ALERT;
    }
    return pulseState;
}

IPState EQMod::GuideEast(uint32_t ms)
{
    if (ms < MinPulseN->value)
    {
        return IPS_IDLE;
    }

    double rateshift = 0.0;
    rateshift        = TRACKRATE_SIDEREAL * GuideRateNP.findWidgetByName("GUIDE_RATE_WE")->getValue();
    LOGF_DEBUG("Timed guide East %d ms at rate %g %s", ms, rateshift, RAInverted ? "(Inverted)" : "");

    IPState pulseState = IPS_BUSY;

    if (RAInverted)
        rateshift = -rateshift;
    try
    {
        if (mount->HasPPEC())
        {
            restartguidePPEC = false;
            if (PPECSP.getState() == IPS_BUSY)
            {
                restartguidePPEC = true;
                LOG_INFO("Turning PPEC off while guiding.");
                mount->TurnPPEC(false);
            }
        }
        if (ms >= MinPulseTimerN->value)
        {
            pulseInProgress |= 2;
            GuideTimerWE = IEAddTimer(ms, (IE_TCF *)timedguideWECallback, this);
            mount->StartRATracking(GetRATrackRate() - rateshift);
        }
        else
        {
            // We should be done once the synchronous guide is complete
            pulseState = IPS_IDLE;

            struct timespec starttime, endtime;
            clock_gettime(CLOCK_MONOTONIC, &starttime);
            mount->StartRATracking(GetRATrackRate() - rateshift);
            clock_gettime(CLOCK_MONOTONIC, &endtime);
            double elapsed =
                (endtime.tv_sec - starttime.tv_sec) * 1000.0 + ((endtime.tv_nsec - starttime.tv_nsec) / 1000000.0);
            if (elapsed < ms)
            {
                uint32_t left = ms - elapsed;
                usleep(left * 1000);
            }
            try
            {
                if (mount->HasPPEC())
                {
                    if (restartguidePPEC)
                    {
                        restartguidePPEC = false;
                        DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_SESSION, "Turning PPEC on after guiding.");
                        mount->TurnPPEC(true);
                    }
                }
                mount->StartRATracking(GetRATrackRate());
            }
            catch (EQModError e)
            {
                if (!(e.DefaultHandleException(this)))
                {
                    DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_WARNING,
                                "Timed guide West/East Error: can not restart tracking");
                }
            }
            GuideComplete(AXIS_RA);
            DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_DEBUG, "End Timed guide West/East");
        }
    }
    catch (EQModError e)
    {
        e.DefaultHandleException(this);
        return IPS_ALERT;
    }

    return pulseState;
}

IPState EQMod::GuideWest(uint32_t ms)
{
    if (ms < MinPulseN->value)
    {
        return IPS_IDLE;
    }

    double rateshift = 0.0;
    rateshift        = TRACKRATE_SIDEREAL * GuideRateNP.findWidgetByName("GUIDE_RATE_WE")->getValue();
    LOGF_DEBUG("Timed guide West %d ms at rate %g %s", ms, rateshift, RAInverted ? "(Inverted)" : "");

    IPState pulseState = IPS_BUSY;

    if (RAInverted)
        rateshift = -rateshift;
    try
    {
        if (mount->HasPPEC())
        {
            restartguidePPEC = false;
            if (PPECSP.getState() == IPS_BUSY)
            {
                restartguidePPEC = true;
                LOG_INFO("Turning PPEC off while guiding.");
                mount->TurnPPEC(false);
            }
        }
        if (ms >= MinPulseTimerN->value)
        {
            pulseInProgress |= 2;
            GuideTimerWE = IEAddTimer(ms, (IE_TCF *)timedguideWECallback, this);
            mount->StartRATracking(GetRATrackRate() + rateshift);
        }
        else
        {
            // We should be done once the synchronous guide is complete
            pulseState = IPS_IDLE;

            struct timespec starttime, endtime;
            clock_gettime(CLOCK_MONOTONIC, &starttime);
            mount->StartRATracking(GetRATrackRate() + rateshift);
            clock_gettime(CLOCK_MONOTONIC, &endtime);
            double elapsed =
                (endtime.tv_sec - starttime.tv_sec) * 1000.0 + ((endtime.tv_nsec - starttime.tv_nsec) / 1000000.0);
            if (elapsed < ms)
            {
                uint32_t left = ms - elapsed;
                usleep(left * 1000);
            }
            try
            {
                if (mount->HasPPEC())
                {
                    if (restartguidePPEC)
                    {
                        restartguidePPEC = false;
                        DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_SESSION, "Turning PPEC on after guiding.");
                        mount->TurnPPEC(true);
                    }
                }
                mount->StartRATracking(GetRATrackRate());
            }
            catch (EQModError e)
            {
                if (!(e.DefaultHandleException(this)))
                {
                    DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_WARNING,
                                "Timed guide West/East Error: can not restart tracking");
                }
            }
            GuideComplete(AXIS_RA);
            DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_DEBUG, "End Timed guide West/East");
        }
    }
    catch (EQModError e)
    {
        e.DefaultHandleException(this);
        return IPS_ALERT;
    }

    return pulseState;
}

bool EQMod::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    bool compose = true;
    //  first check if it's for our device
    if (strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here

        if (strcmp(name, "SLEWSPEEDS") == 0)
        {
            /* TODO: don't change speed in gotos gotoparams.inprogress... */
            if (TrackState != SCOPE_TRACKING)
            {
                try
                {
                    for (int i = 0; i < n; i++)
                    {
                        if (strcmp(names[i], "RASLEW") == 0)
                            mount->SetRARate(values[i]);
                        else if (strcmp(names[i], "DESLEW") == 0)
                            mount->SetDERate(values[i]);
                    }
                }
                catch (EQModError e)
                {
                    return (e.DefaultHandleException(this));
                }
            }
            SlewSpeedsNP.update(values, names, n);
            SlewSpeedsNP.setState(IPS_OK);
            SlewSpeedsNP.apply();
            LOGF_INFO("Setting Slew rates - RA=%.2fx DE=%.2fx",
                      SlewSpeedsNP.findWidgetByName("RASLEW")->getValue(), SlewSpeedsNP.findWidgetByName("DESLEW")->getValue());
            return true;
        }

        // Guider interface
        if (GuideNSNP.isNameMatch(name) || GuideWENP.isNameMatch(name))
        {
            // Unless we're in track mode, we don't obey guide commands.
            if (TrackState != SCOPE_TRACKING)
            {
                GuideNSNP.setState(IPS_IDLE);
                GuideNSNP.apply();;
                GuideWENP.setState(IPS_IDLE);
                GuideWENP.apply();
                LOG_WARN("Can not guide if not tracking.");
                return true;
            }

            return GI::processNumber(dev, name, values, names, n);
        }
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, n);
            GuideRateNP.setState(IPS_OK);
            GuideRateNP.apply();
            LOGF_INFO("Setting Custom Tracking Rates - RA=%1.1f arcsec/s DE=%1.1f arcsec/s",
                      GuideRateNP.findWidgetByName("GUIDE_RATE_WE")->getValue(),
                      GuideRateNP.findWidgetByName("GUIDE_RATE_NS")->getValue());
            return true;
        }

        if (PulseLimitsNP.isNameMatch(name))
        {
            PulseLimitsNP.update(values, names, n);
            PulseLimitsNP.setState(IPS_OK);
            PulseLimitsNP.apply();
            LOGF_INFO("Setting pulse limits: minimum pulse %3.0f ms, minimum timer pulse %4.0f ms", MinPulseN->value,
                      MinPulseTimerN->value);
            return true;
        }

        if (strcmp(name, "BACKLASH") == 0)
        {
            BacklashNP.update(values, names, n);
            BacklashNP.setState(IPS_OK);
            BacklashNP.apply();
            mount->SetBacklashRA((uint32_t)(BacklashNP.findWidgetByName("BACKLASHRA")->getValue()));
            mount->SetBacklashDE((uint32_t)(BacklashNP.findWidgetByName("BACKLASHDE")->getValue()));
            LOGF_INFO("Setting Backlash compensation - RA=%.0f microsteps DE=%.0f microsteps",
                      BacklashNP.findWidgetByName("BACKLASHRA")->getValue(), BacklashNP.findWidgetByName("BACKLASHDE")->getValue());
            return true;
        }

        if (mount->HasPolarLed())
        {
            if (strcmp(name, "LED_BRIGHTNESS") == 0)
            {
                LEDBrightnessNP.update(values, names, n);
                LEDBrightnessNP.setState(IPS_OK);
                LEDBrightnessNP.apply();
                mount->SetLEDBrightness(static_cast<uint8_t>(values[0]));
                LOGF_INFO("Setting LED brightness to %.f", values[0]);
                return true;
            }
        }

        if (strcmp(name, "STANDARDSYNCPOINT") == 0)
        {
            syncdata2 = syncdata;
            bzero(&syncdata, sizeof(syncdata));
            StandardSyncPointNP.update(values, names, n);
            StandardSyncPointNP.setState(IPS_OK);

            syncdata.jd           = StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_JD")->getValue();
            syncdata.lst          = StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_SYNCTIME")->getValue();
            syncdata.targetRA     = StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_CELESTIAL_RA")->getValue();
            syncdata.targetDEC    = StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_CELESTIAL_DE")->getValue();
            syncdata.telescopeRA  = StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_TELESCOPE_RA")->getValue();
            syncdata.telescopeDEC = StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_TELESCOPE_DE")->getValue();
            syncdata.deltaRA      = syncdata.targetRA - syncdata.telescopeRA;
            syncdata.deltaDEC     = syncdata.targetDEC - syncdata.telescopeDEC;
            StandardSyncPointNP.apply();

            StandardSyncNP.findWidgetByName("STANDARDSYNC_RA")->setValue(syncdata.deltaRA);
            StandardSyncNP.findWidgetByName("STANDARDSYNC_DE")->setValue(syncdata.deltaDEC);
            StandardSyncNP.apply();

            LOGF_INFO("Mount manually Synced (deltaRA = %.6f deltaDEC = %.6f)",
                      syncdata.deltaRA, syncdata.deltaDEC);
            //IDLog("Mount Synced (deltaRA = %.6f deltaDEC = %.6f)\n", syncdata.deltaRA, syncdata.deltaDEC);
            if (syncdata2.lst != 0.0)
            {
                computePolarAlign(syncdata2, syncdata, getLatitude(), &tpa_alt, &tpa_az);
                SyncPolarAlignNP.findWidgetByName("SYNCPOLARALIGN_ALT")->setValue(tpa_alt);
                SyncPolarAlignNP.findWidgetByName("SYNCPOLARALIGN_AZ")->setValue(tpa_az);
                SyncPolarAlignNP.apply();
                IDLog("computePolarAlign: Telescope Polar Axis: alt = %g, az = %g\n", tpa_alt, tpa_az);
            }

            return true;
        }
    }

#ifdef WITH_ALIGN_GEEHALEL
    if (align)
    {
        compose = align->ISNewNumber(dev, name, values, names, n);
        if (compose)
            return true;
    }
#endif

    if (simulator)
    {
        compose = simulator->ISNewNumber(dev, name, values, names, n);
        if (compose)
            return true;
    }

#ifdef WITH_SCOPE_LIMITS
    if (horizon)
    {
        compose = horizon->ISNewNumber(dev, name, values, names, n);
        if (compose)
            return true;
    }
#endif

#ifdef WITH_ALIGN
    ProcessAlignmentNumberProperties(this, name, values, names, n);
#endif
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool EQMod::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    bool compose = true;
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, "SIMULATION"))
        {
            auto svp = getSwitch(name);
            svp.update(states, names, n);

            auto sp = svp.findOnSwitch();
            if (!sp)
                return false;

            if (isConnected())
            {
                DEBUG(INDI::Logger::DBG_WARNING,
                      "Mount must be disconnected before you can change simulation settings.");
                svp.setState(IPS_ALERT);
                svp.apply();
                return false;
            }

            setStepperSimulation(sp->isNameMatch("ENABLE"));
            return true;
        }

        if (strcmp(name, "USEBACKLASH") == 0)
        {
            UseBacklashSP.update(states, names, n);
            mount->SetBacklashUseRA(UseBacklashSP.findWidgetByName("USEBACKLASHRA")->getState() == ISS_ON ? true : false);
            mount->SetBacklashUseDE(UseBacklashSP.findWidgetByName("USEBACKLASHDE")->getState() == ISS_ON ? true : false);
            LOGF_INFO("Use Backlash :  RA: %s, DE: %s",
                      UseBacklashSP.findWidgetByName("USEBACKLASHRA")->getState() == ISS_ON ? "True" : "False",
                      UseBacklashSP.findWidgetByName("USEBACKLASHDE")->getState() == ISS_ON ? "True" : "False");
            UseBacklashSP.setState(IPS_IDLE);
            UseBacklashSP.apply();
            return true;
        }

        if (strcmp(name, "TRACKDEFAULT") == 0)
        {
            auto swbefore = TrackDefaultSP.findOnSwitch();
            TrackDefaultSP.update(states, names, n);
            auto swafter = TrackDefaultSP.findOnSwitch();
            if (swbefore != swafter)
            {
                TrackDefaultSP.setState(IPS_IDLE);
                TrackDefaultSP.apply();
                LOGF_INFO("Changed Track Default (from %s to %s).", swbefore->getName(),
                          swafter->getName());
            }
            return true;
        }

        if (strcmp(name, "ST4_GUIDE_RATE_WE") == 0)
        {
            auto swbefore = ST4GuideRateWESP.findOnSwitch();
            ST4GuideRateWESP.update(states, names, n);
            auto swafter = ST4GuideRateWESP.findOnSwitch();
            if (swbefore != swafter)
            {
                unsigned char rate = '0' + (unsigned char)ST4GuideRateWESP.findOnSwitchIndex();
                mount->SetST4RAGuideRate(rate);
                ST4GuideRateWESP.setState(IPS_IDLE);
                ST4GuideRateWESP.apply();
                LOGF_INFO("Changed ST4 Guide rate WE (from %s to %s).", swbefore->getLabel(),
                          swafter->getLabel());
            }
            return true;
        }

        if (strcmp(name, "ST4_GUIDE_RATE_NS") == 0)
        {
            auto swbefore = ST4GuideRateNSSP.findOnSwitch();
            ST4GuideRateNSSP.update(states, names, n);
            auto swafter = ST4GuideRateNSSP.findOnSwitch();

            if (swbefore != swafter)
            {
                unsigned char rate = '0' + (unsigned char)ST4GuideRateNSSP.findOnSwitchIndex();
                mount->SetST4DEGuideRate(rate);
                ST4GuideRateNSSP.setState(IPS_IDLE);
                ST4GuideRateNSSP.apply();
                LOGF_INFO("Changed ST4 Guide rate NS (from %s to %s).", swbefore->getLabel(),
                          swafter->getLabel());
            }
            return true;
        }

        if (!strcmp(name, "SYNCMANAGE"))
        {
            auto svp = getSwitch(name);
            svp.update(states, names, n);
            auto sp = svp.findOnSwitch();
            if (!sp)
                return false;
            svp.apply();

            if (!strcmp(sp->name, "SYNCCLEARDELTA"))
            {
                bzero(&syncdata, sizeof(syncdata));
                bzero(&syncdata2, sizeof(syncdata2));
                StandardSyncNP.findWidgetByName("STANDARDSYNC_RA")->setValue(syncdata.deltaRA);
                StandardSyncNP.findWidgetByName("STANDARDSYNC_DE")->setValue(syncdata.deltaDEC);
                StandardSyncNP.apply();

                StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_JD")->setValue(syncdata.jd);
                StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_SYNCTIME")->setValue(syncdata.lst);
                StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_CELESTIAL_RA")->setValue(syncdata.targetRA);
                ;
                StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_CELESTIAL_DE")->setValue(syncdata.targetDEC);
                ;
                StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_TELESCOPE_RA")->setValue(syncdata.telescopeRA);
                ;
                StandardSyncPointNP.findWidgetByName("STANDARDSYNCPOINT_TELESCOPE_DE")->setValue(syncdata.telescopeDEC);
                ;
                StandardSyncPointNP.apply();
                LOG_INFO("Cleared current Sync Data");
                tpa_alt                                                     = 0.0;
                tpa_az                                                      = 0.0;
                SyncPolarAlignNP.findWidgetByName("SYNCPOLARALIGN_ALT")->setValue(tpa_alt);
                SyncPolarAlignNP.findWidgetByName("SYNCPOLARALIGN_AZ")->setValue(tpa_az);
                SyncPolarAlignNP.apply();
                return true;
            }
        }

        if (!strcmp(name, "REVERSEDEC"))
        {
            ReverseDECSP.update(states, names, n);

            ReverseDECSP.setState(IPS_OK);

            UpdateDEInverted();

            LOG_INFO("Inverting Declination Axis.");

            ReverseDECSP.apply();
        }

        if (!strcmp(name, "TARGETPIERSIDE"))
        {
            TargetPierSideSP.update(states, names, n);

            TargetPierSideSP.setState(IPS_OK);

            TargetPier = PIER_UNKNOWN;
            if (TargetPierSideSP.findWidgetByName("PIER_EAST")->getState() == ISS_ON)
            {
                TargetPier = PIER_EAST;
                LOG_INFO("Target pier side set to EAST");
            }
            else if (TargetPierSideSP.findWidgetByName("PIER_WEST")->getState() == ISS_ON)
            {
                TargetPier = PIER_WEST;
                LOG_INFO("Target pier side set to WEST");
            }

            TargetPierSideSP.apply();
        }

        //if (MountInformationTP && MountInformationTP->tp && (!strcmp(MountInformationTP->tp[0].text, "EQ8") || !strcmp(MountInformationTP->tp[0].text, "AZEQ6"))) {

        if (mount->HasAuxEncoders())
        {
            if (AuxEncoderSP && AuxEncoderSP.isNameMatch(name))
            {
                AuxEncoderSP.update(states, names, n);
                if (AuxEncoderSP[1].getState() == ISS_ON)
                {
                    AuxEncoderSP.setState(IPS_OK);
                    LOG_DEBUG("Turning auxiliary encoders on.");
                    mount->TurnRAEncoder(true);
                    mount->TurnDEEncoder(true);
                }
                else
                {
                    AuxEncoderSP.setState(IPS_IDLE);
                    LOG_DEBUG("Turning auxiliary encoders off.");
                    mount->TurnRAEncoder(false);
                    mount->TurnDEEncoder(false);
                }
                AuxEncoderSP.apply();
            }
        }

        if (mount->HasPPEC())
        {
            if (PPECTrainingSP && PPECTrainingSP.isNameMatch(name))
            {
                PPECTrainingSP.update(states, names, n);
                if (PPECTrainingSP[1].getState() == ISS_ON)
                {
                    if (TrackState != SCOPE_TRACKING)
                    {
                        PPECTrainingSP.setState(IPS_IDLE);
                        LOG_WARN("Can not start PPEC Training. Scope not tracking");
                        PPECTrainingSP.reset();
                        PPECTrainingSP[0].setState(ISS_ON);
                        PPECTrainingSP[1].setState(ISS_OFF);
                    }
                    else
                    {
                        PPECTrainingSP.setState(IPS_BUSY);
                        LOG_INFO("Turning PPEC Training on.");
                        try
                        {
                            mount->TurnPPECTraining(true);
                        }
                        catch (EQModError e)
                        {
                            LOG_WARN("Unable to start PPEC Training.");
                            PPECTrainingSP.setState(IPS_ALERT);
                            PPECTrainingSP[0].setState(ISS_ON);
                            PPECTrainingSP[1].setState(ISS_OFF);
                        }
                    }
                }
                else
                {
                    PPECTrainingSP.setState(IPS_IDLE);
                    LOG_INFO("Turning PPEC Training off.");
                    mount->TurnPPECTraining(false);
                }
                PPECTrainingSP.apply();
                return true;
            }
            if (PPECSP && PPECSP.isNameMatch(name))
            {
                PPECSP.update(states, names, n);
                if (PPECSP[1].getState() == ISS_ON)
                {
                    PPECSP.setState(IPS_BUSY);
                    LOG_INFO("Turning PPEC on.");
                    mount->TurnPPEC(true);
                }
                else
                {
                    PPECSP.setState(IPS_IDLE);
                    LOG_INFO("Turning PPEC off.");
                    mount->TurnPPEC(false);
                }
                PPECSP.apply();
                return true;
            }
        }

        if (mount->HasSnapPort1())
        {
            if (SNAPPORT1SP && SNAPPORT1SP.isNameMatch(name))
            {
                SNAPPORT1SP.update(states, names, n);
                if (SNAPPORT1SP[1].getState() == ISS_ON)
                {
                    SNAPPORT1SP.setState(IPS_OK);
                    DEBUG(INDI::Logger::DBG_DEBUG, "Turning snap port 1 on.");
                    mount->TurnSnapPort1(true);
                }
                else
                {
                    SNAPPORT1SP.setState(IPS_IDLE);
                    DEBUG(INDI::Logger::DBG_DEBUG, "Turning snap port 1 off.");
                    mount->TurnSnapPort1(false);
                }
                SNAPPORT1SP.apply();
                return true;
            }
        }

        if (mount->HasSnapPort2())
        {
            if (SNAPPORT2SP && SNAPPORT2SP.isNameMatch(name))
            {
                SNAPPORT2SP.update(states, names, n);
                if (SNAPPORT2SP[1].getState() == ISS_ON)
                {
                    SNAPPORT2SP.setState(IPS_OK);
                    DEBUG(INDI::Logger::DBG_DEBUG, "Turning snap port 2 on.");
                    mount->TurnSnapPort2(true);
                }
                else
                {
                    SNAPPORT2SP.setState(IPS_IDLE);
                    DEBUG(INDI::Logger::DBG_DEBUG, "Turning snap port 2 off.");
                    mount->TurnSnapPort2(false);
                }

                SNAPPORT2SP.apply();
                return true;
            }
        }



#if defined WITH_ALIGN || defined WITH_ALIGN_GEEHALEL
        if (AlignSyncModeSP && AlignSyncModeSP.isNameMatch(name))
        {
            AlignSyncModeSP.setState(IPS_OK);
            AlignSyncModeSP.update(states, names, n);
            //for (int i=0; i < n; i++)
            //  IDLog("AlignSyncMode Switch %s %d\n", names[i], states[i]);
            auto sw = AlignSyncModeSP.findOnSwitch();
            AlignSyncModeSP.apply("Sync mode set to %s", sw->getLabel());
            return true;
        }
#endif

#if defined WITH_ALIGN_GEEHALEL && defined WITH_ALIGN
        if (strcmp(name, AlignMethodSP.name) == 0)
        {
            ISwitch *sw;
            AlignMethodSP.s = IPS_OK;
            IUUpdateSwitch(&AlignMethodSP, states, names, n);
            //for (int i=0; i < n; i++)
            //  IDLog("AlignSyncMode Switch %s %d\n", names[i], states[i]);
            sw = IUFindOnSwitch(&AlignMethodSP);
            IDSetSwitch(&AlignMethodSP, "Align method set to %s", sw->label);
            return true;
        }
#endif
    }
#ifdef WITH_ALIGN_GEEHALEL
    if (align)
    {
        compose = align->ISNewSwitch(dev, name, states, names, n);
        if (compose)
            return true;
    }
#endif

    if (simulator)
    {
        compose = simulator->ISNewSwitch(dev, name, states, names, n);
        if (compose)
            return true;
    }
#ifdef WITH_SCOPE_LIMITS
    if (horizon)
    {
        compose = horizon->ISNewSwitch(dev, name, states, names, n);
        if (compose)
            return true;
    }
#endif
#ifdef WITH_ALIGN
    ProcessAlignmentSwitchProperties(this, name, states, names, n);
#endif

    INDI::Logger::ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool EQMod::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    bool compose;
#ifdef WITH_ALIGN_GEEHALEL
    if (align)
    {
        compose = align->ISNewText(dev, name, texts, names, n);
        if (compose)
            return true;
    }
#endif
    if (simulator)
    {
        compose = simulator->ISNewText(dev, name, texts, names, n);
        if (compose)
            return true;
    }
#ifdef WITH_SCOPE_LIMITS
    if (horizon)
    {
        compose = horizon->ISNewText(dev, name, texts, names, n);
        if (compose)
            return true;
    }
#endif
#ifdef WITH_ALIGN
    ProcessAlignmentTextProperties(this, name, texts, names, n);
#endif
    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

#ifdef WITH_ALIGN
bool EQMod::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                      char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Process alignment properties
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}
#endif

bool EQMod::updateTime(ln_date *lndate_utc, double utc_offset)
{
    char utc_time[32];
    lndate.seconds = lndate_utc->seconds;
    lndate.minutes = lndate_utc->minutes;
    lndate.hours   = lndate_utc->hours;
    lndate.days    = lndate_utc->days;
    lndate.months  = lndate_utc->months;
    lndate.years   = lndate_utc->years;

    utc.tm_sec  = lndate.seconds;
    utc.tm_min  = lndate.minutes;
    utc.tm_hour = lndate.hours;
    utc.tm_mday = lndate.days;
    utc.tm_mon  = lndate.months - 1;
    utc.tm_year = lndate.years - 1900;

    gettimeofday(&lasttimeupdate, nullptr);
    get_utc_time(&lastclockupdate);

    strftime(utc_time, 32, "%Y-%m-%dT%H:%M:%S", &utc);

    LOGF_INFO("Setting UTC Time to %s, Offset %g", utc_time, utc_offset);

    return true;
}

double EQMod::GetRASlew()
{
    ISwitch *sw;
    double rate = 1.0;
    sw          = SlewRateSP.findOnSwitch();
    if (!strcmp(sw->name, "SLEWCUSTOM"))
        rate = SlewSpeedsNP.findWidgetByName("RASLEW")->getValue();
    else
        rate = *((int *)sw->aux);
    return rate;
}

double EQMod::GetDESlew()
{
    ISwitch *sw;
    double rate = 1.0;
    sw          = SlewRateSP.findOnSwitch();
    if (!strcmp(sw->name, "SLEWCUSTOM"))
        rate = SlewSpeedsNP.findWidgetByName("DESLEW")->getValue();
    else
        rate = *((int *)sw->aux);
    return rate;
}

bool EQMod::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    const char *dirStr = (dir == DIRECTION_NORTH) ? "North" : "South";
    double rate        = (dir == DIRECTION_NORTH) ? GetDESlew() : GetDESlew() * -1;

    try
    {
        switch (command)
        {
            case MOTION_START:
                if (gotoInProgress() || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
                {
                    LOG_WARN("Can not slew while goto/park in progress, or scope parked.");
                    return false;
                }

                LOGF_INFO("Starting %s slew.", dirStr);
                if (DEInverted)
                    rate = -rate;
                mount->SlewDE(rate);
                //TrackState = SCOPE_SLEWING;
                break;

            case MOTION_STOP:
                LOGF_INFO("%s Slew stopped", dirStr);
                mount->StopDE();
                //if (TrackModeSP->s == IPS_BUSY)
                if (RememberTrackState == SCOPE_TRACKING)
                {
                    LOG_INFO("Restarting DE Tracking...");
                    TrackState = SCOPE_TRACKING;
                    mount->StartDETracking(GetDETrackRate());
                }
                else
                    TrackState = SCOPE_IDLE;

                RememberTrackState = TrackState;

                break;
        }
    }
    catch (EQModError e)
    {
        return e.DefaultHandleException(this);
    }
    return true;
}

bool EQMod::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    const char *dirStr = (dir == DIRECTION_WEST) ? "West" : "East";
    double rate        = (dir == DIRECTION_WEST) ? GetRASlew() : GetRASlew() * -1;

    try
    {
        switch (command)
        {
            case MOTION_START:
                if (gotoInProgress() || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
                {
                    LOG_WARN("Can not slew while goto/park in progress, or scope parked.");
                    return false;
                }

                LOGF_INFO("Starting %s slew.", dirStr);
                if (RAInverted)
                    rate = -rate;
                mount->SlewRA(rate);
                //TrackState = SCOPE_SLEWING;
                break;

            case MOTION_STOP:
                LOGF_INFO("%s Slew stopped", dirStr);
                mount->StopRA();
                //if (TrackModeSP->s == IPS_BUSY)
                if (RememberTrackState == SCOPE_TRACKING)
                {
                    LOG_INFO("Restarting RA Tracking...");
                    TrackState = SCOPE_TRACKING;
                    mount->StartRATracking(GetRATrackRate());
                }
                else
                    TrackState = SCOPE_IDLE;

                RememberTrackState = TrackState;

                break;
        }
    }
    catch (EQModError e)
    {
        return e.DefaultHandleException(this);
    }
    return true;
}

bool EQMod::Abort()
{
    try
    {
        mount->StopRA();
    }
    catch (EQModError e)
    {
        if (!(e.DefaultHandleException(this)))
        {
            LOG_WARN("Abort: error while stopping RA motor");
        }
    }
    try
    {
        mount->StopDE();
    }
    catch (EQModError e)
    {
        if (!(e.DefaultHandleException(this)))
        {
            LOG_WARN("Abort: error while stopping DE motor");
        }
    }

    GuideNSNP.setState(IPS_IDLE);
    GuideNSNP.apply();
    GuideWENP.setState(IPS_IDLE);
    GuideWENP.apply();
#if 0
    TrackModeSP->s = IPS_IDLE;
    IUResetSwitch(TrackModeSP);
    IDSetSwitch(TrackModeSP, nullptr);
#endif
    AutohomeState = AUTO_HOME_IDLE;
    HomeSP.setState(IPS_IDLE);
    HomeSP.reset();
    HomeSP.apply();

    TrackState = SCOPE_IDLE;
    RememberTrackState = TrackState;
    if (gotoparams.completed == false)
        gotoparams.completed = true;

    return true;
}

void EQMod::timedguideNSCallback(void *userpointer)
{
    EQMod *p = ((EQMod *)userpointer);
    p->pulseInProgress &= ~1;

    try
    {
        p->mount->StartDETracking(p->GetDETrackRate());
    }
    catch (EQModError e)
    {
        if (!(e.DefaultHandleException(p)))
        {
            DEBUGDEVICE(p->getDeviceName(), INDI::Logger::DBG_WARNING,
                        "Timed guide North/South Error: can not restart tracking");
        }
    }
    p->GuideComplete(AXIS_DE);
    DEBUGDEVICE(p->getDeviceName(), INDI::Logger::DBG_DEBUG, "End Timed guide North/South");
    IERmTimer(p->GuideTimerNS);
}

void EQMod::timedguideWECallback(void *userpointer)
{
    EQMod *p = ((EQMod *)userpointer);
    p->pulseInProgress &= ~2;

    try
    {
        if (p->mount->HasPPEC())
        {
            if (p->restartguidePPEC)
            {
                p->restartguidePPEC = false;
                DEBUGDEVICE(p->getDeviceName(), INDI::Logger::DBG_SESSION, "Turning PPEC on after guiding.");
                p->mount->TurnPPEC(true);
            }
        }
        p->mount->StartRATracking(p->GetRATrackRate());
    }
    catch (EQModError e)
    {
        if (!(e.DefaultHandleException(p)))
        {
            DEBUGDEVICE(p->getDeviceName(), INDI::Logger::DBG_WARNING,
                        "Timed guide West/East Error: can not restart tracking");
        }
    }
    p->GuideComplete(AXIS_RA);
    DEBUGDEVICE(p->getDeviceName(), INDI::Logger::DBG_DEBUG, "End Timed guide West/East");
    IERmTimer(p->GuideTimerWE);
}

void EQMod::computePolarAlign(SyncData s1, SyncData s2, double lat, double *tpaalt, double *tpaaz)
/*
From // // http://www.whim.org/nebula/math/pdf/twostar.pdf
 */
{
    double delta1 = 0, alpha1 = 0, delta2 = 0, alpha2 = 0;
    double d1 = 0, d2 = 0; /* corrected delta1/delta2 */
    double cdelta1 = 0, calpha1 = 0, cdelta2 = 0, calpha2 = 0;
    double Delta = 0;
    double cosDelta1 = 0, cosDelta2 = 0;
    double cosd2pd1 = 0, d2pd1 = 0;
    double tpadelta = 0, tpaalpha = 0;
    double sintpadelta = 0, costpaalpha = 0, sintpaalpha = 0;
    double cosama1 = 0, cosama2 = 0;
    double cosaz = 0, sinaz = 0;
    double beta = 0;

    // Star s2 polar align
    double s2tra = 0, s2tdec = 0;
    char s2trasexa[13], s2tdecsexa[13];
    char s2rasexa[13], s2decsexa[13];

    alpha1  = DEG_TO_RAD((s1.telescopeRA - s1.lst) * 360.0 / 24.0);
    delta1  = DEG_TO_RAD(s1.telescopeDEC);
    alpha2  = DEG_TO_RAD((s2.telescopeRA - s2.lst) * 360.0 / 24.0);
    delta2  = DEG_TO_RAD(s2.telescopeDEC);
    calpha1 = DEG_TO_RAD((s1.targetRA - s1.lst) * 360.0 / 24.0);
    cdelta1 = DEG_TO_RAD(s1.targetDEC);
    calpha2 = DEG_TO_RAD((s2.targetRA - s2.lst) * 360.0 / 24.0);
    cdelta2 = DEG_TO_RAD(s2.targetDEC);

    if ((calpha2 == calpha1) || (alpha1 == alpha2))
        return;

    cosDelta1 = sin(cdelta1) * sin(cdelta2) + (cos(cdelta1) * cos(cdelta2) * cos(calpha2 - calpha1));
    cosDelta2 = sin(delta1) * sin(delta2) + (cos(delta1) * cos(delta2) * cos(alpha2 - alpha1));

    if (cosDelta1 != cosDelta2)
        LOGF_DEBUG(
            "PolarAlign -- Telescope axes are not perpendicular. Angular distances are:celestial=%g telescope=%g",
            acos(cosDelta1), acos(cosDelta2));
    Delta = acos(cosDelta1);
    LOGF_DEBUG("Angular distance of the two stars is %g", Delta);

    //cosd2md1 = sin(delta1) * sin(delta2) + cos(delta1) * cos(delta2);
    cosd2pd1 = ((cos(delta2 - delta1) * (1 + cos(alpha2 - alpha1))) - (2.0 * cosDelta2)) / (1 - cos(alpha2 - alpha1));
    d2pd1    = acos(cosd2pd1);
    if (delta2 * delta1 > 0.0)
    {
        /* same sign */
        if (delta1 < 0.0)
            d2pd1 = -d2pd1;
    }
    else
    {
        if (fabs(delta1) > fabs(delta2))
        {
            if (delta1 < 0.0)
                d2pd1 = -d2pd1;
        }
        else
        {
            if (delta2 < 0.0)
                d2pd1 = -d2pd1;
        }
    }

    d2 = (d2pd1 + delta2 - delta1) / 2.0;
    d1 = d2pd1 - d2;
    LOGF_DEBUG("Computed delta1 = %g (%g) delta2 = %g (%g)", d1, delta1, d2, delta2);

    delta1 = d1;
    delta2 = d2;

    sintpadelta =
        (sin(delta1) * sin(cdelta1)) + (sin(delta2) * sin(cdelta2)) -
        cosDelta1 * ((sin(delta1) * sin(cdelta2)) + (sin(cdelta1) * sin(delta2))) +
        (cos(delta1) * cos(delta2) * sin(alpha2 - alpha1) * cos(cdelta1) * cos(cdelta2) * sin(calpha2 - calpha1));
    sintpadelta = sintpadelta / (sin(Delta) * sin(Delta));
    tpadelta    = asin(sintpadelta);
    cosama1     = (sin(delta1) - (sin(cdelta1) * sintpadelta)) / (cos(cdelta1) * cos(tpadelta));
    cosama2     = (sin(delta2) - (sin(cdelta2) * sintpadelta)) / (cos(cdelta2) * cos(tpadelta));

    costpaalpha = (sin(calpha2) * cosama1 - sin(calpha1) * cosama2) / sin(calpha2 - calpha1);
    sintpaalpha = (cos(calpha1) * cosama2 - cos(calpha2) * cosama1) / sin(calpha2 - calpha1);
    //tpaalpha = acos(costpaalpha);
    //if (sintpaalpha < 0) tpaalpha = 2 * M_PI - tpaalpha;
    // tpadelta and tpaaplha are very near M_PI / 2 d: DON'T USE  atan2
    //tpaalpha=atan2(sintpaalpha, costpaalpha);
    tpaalpha = 2 * atan2(sintpaalpha, (1.0 + costpaalpha));
    LOGF_DEBUG("Computed Telescope polar alignment (rad): delta(dec) = %g alpha(ha) = %g",
               tpadelta, tpaalpha);

    beta    = DEG_TO_RAD(lat);
    *tpaalt = asin(sin(tpadelta) * sin(beta) + (cos(tpadelta) * cos(beta) * cos(tpaalpha)));
    cosaz   = (sin(tpadelta) - (sin(*tpaalt) * sin(beta))) / (cos(*tpaalt) * cos(beta));
    sinaz   = (cos(tpadelta) * sin(tpaalpha)) / cos(*tpaalt);
    //*tpaaz = acos(cosaz);
    //if (sinaz < 0) *tpaaz = 2 * M_PI - *tpaaz;
    *tpaaz  = atan2(sinaz, cosaz);
    *tpaalt = RAD_TO_DEG(*tpaalt);
    *tpaaz  = RAD_TO_DEG(*tpaaz);
    LOGF_DEBUG("Computed Telescope polar alignment (deg): alt = %g az = %g", *tpaalt, *tpaaz);

    starPolarAlign(s2.lst, s2.targetRA, s2.targetDEC, (M_PI / 2) - tpaalpha, (M_PI / 2) - tpadelta, &s2tra, &s2tdec);
    fs_sexa(s2trasexa, s2tra, 2, 3600);
    fs_sexa(s2tdecsexa, s2tdec, 3, 3600);
    fs_sexa(s2rasexa, s2.targetRA, 2, 3600);
    fs_sexa(s2decsexa, s2.targetDEC, 3, 3600);
    LOGF_INFO("Star (RA=%s DEC=%s) Polar Align Coords: RA=%s DEC=%s", s2rasexa, s2decsexa,
              s2trasexa, s2tdecsexa);
    s2tra  = s2.targetRA + (s2.targetRA - s2tra);
    s2tdec = s2.targetDEC + (s2.targetDEC - s2tdec);
    fs_sexa(s2trasexa, s2tra, 2, 3600);
    fs_sexa(s2tdecsexa, s2tdec, 3, 3600);
    fs_sexa(s2rasexa, s2.targetRA, 2, 3600);
    fs_sexa(s2decsexa, s2.targetDEC, 3, 3600);

    LOGF_INFO("Star (RA=%s DEC=%s) Polar Align Goto: RA=%s DEC=%s", s2rasexa, s2decsexa,
              s2trasexa, s2tdecsexa);
}

void EQMod::starPolarAlign(double lst, double ra, double dec, double theta, double gamma, double *tra, double *tdec)
{
    double rotz[3][3];
    double rotx[3][3];
    double mat[3][3];

    double H;
    double Lc, Mc, Nc;

    double mra, mdec;
    double L, M, N;
    int i, j, k;

    H   = (lst - ra) * M_PI / 12.0;
    dec = dec * M_PI / 180.0;

    rotz[0][0] = cos(theta);
    rotz[0][1] = -sin(theta);
    rotz[0][2] = 0.0;
    rotz[1][0] = sin(theta);
    rotz[1][1] = cos(theta);
    rotz[1][2] = 0.0;
    rotz[2][0] = 0.0;
    rotz[2][1] = 0.0;
    rotz[2][2] = 1.0;

    rotx[0][0] = 1.0;
    rotx[0][1] = 0.0;
    rotx[0][2] = 0.0;
    rotx[1][0] = 0.0;
    rotx[1][1] = cos(gamma);
    rotx[1][2] = -sin(gamma);
    rotx[2][0] = 0.0;
    rotx[2][1] = sin(gamma);
    rotx[2][2] = cos(gamma);

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            mat[i][j] = 0.0;
            for (k = 0; k < 3; k++)
                mat[i][j] += rotx[i][k] * rotz[k][j];
        }
    }

    Lc = cos(dec) * cos(-H);
    Mc = cos(dec) * sin(-H);
    Nc = sin(dec);

    L = mat[0][0] * Lc + mat[0][1] * Mc + mat[0][2] * Nc;
    M = mat[1][0] * Lc + mat[1][1] * Mc + mat[1][2] * Nc;
    N = mat[2][0] * Lc + mat[2][1] * Mc + mat[2][2] * Nc;

    mra = atan2(M, L) * 12.0 / M_PI;
    //mra=atan(M/L) * 12.0 / M_PI;
    //printf("atan(M/L) %g L=%g M=%g N=%g\n", mra, L, M, N);
    //if (L < 0.0) mra = 12.0 + mra;
    mra += lst;
    while (mra < 0.0)
        mra += 24.0;
    while (mra > 24.0)
        mra -= 24.0;
    mdec  = asin(N) * 180.0 / M_PI;
    *tra  = mra;
    *tdec = mdec;
}

bool EQMod::updateLocation(double latitude, double longitude, double elevation)
{
    m_Location.longitude = longitude;
    m_Location.latitude  = latitude;
    m_Location.elevation = elevation;

    if (latitude < 0.0)
        SetSouthernHemisphere(true);
    else
        SetSouthernHemisphere(false);
#ifdef WITH_ALIGN
    INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers::UpdateLocation(latitude, longitude, elevation);
    // Set this according to mount type
    SetApproximateMountAlignmentFromMountType(EQUATORIAL);
#endif
    // Make display longitude to be in the standard 0 to +180 East, and 0 to -180 West.
    // No need to confuse new users with INDI format.
    char lat_str[MAXINDIFORMAT] = {0}, lng_str[MAXINDIFORMAT] = {0};
    double display_longitude = longitude > 180 ? longitude - 360 : longitude;
    fs_sexa(lat_str, m_Location.latitude, 2, 36000);
    fs_sexa(lng_str, display_longitude, 2, 36000);
    // Choose WGS 84, also known as EPSG:4326 for latitude/longitude ordering
    LOGF_INFO("Observer location updated: Latitude %.12s (%.2f) Longitude %.12s (%.2f)", lat_str, m_Location.latitude, lng_str,
              display_longitude);
    return true;
}

void EQMod::saveInitialParkPosition()
{
    // If there is no initial park data. We assume the default parking position
    // Looking at celestial pole with weights down
    SetDefaultPark();
    WriteParkData();
}

bool EQMod::SetCurrentPark()
{
    parkRAEncoder = currentRAEncoder;
    parkDEEncoder = currentDEEncoder;
    SetAxis1Park(parkRAEncoder);
    SetAxis2Park(parkDEEncoder);
    LOGF_INFO("Setting Park Position to current RA Encoder=%ld DE Encoder=%ld",
              static_cast<long>(parkRAEncoder), static_cast<long>(parkDEEncoder));

    return true;
}

bool EQMod::SetDefaultPark()
{
    parkRAEncoder = GetAxis1ParkDefault();
    parkDEEncoder = GetAxis2ParkDefault();
    SetAxis1Park(parkRAEncoder);
    SetAxis2Park(parkDEEncoder);
    LOGF_INFO("Setting Park Position to default RA Encoder=%ld DE Encoder=%ld",
              static_cast<long>(parkRAEncoder), static_cast<long>(parkDEEncoder));

    return true;
}

bool EQMod::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    if (BacklashNP)
        BacklashNP.save(fp);
    if (UseBacklashSP)
        UseBacklashSP.save(fp);
    if (GuideRateNP)
        GuideRateNP.save(fp);
    if (PulseLimitsNP)
        PulseLimitsNP.save(fp);
    if (SlewSpeedsNP)
        SlewSpeedsNP.save(fp);
    if (ReverseDECSP)
        ReverseDECSP.save(fp);
    if (LEDBrightnessNP)
        LEDBrightnessNP.save(fp);
    if (HasPECState())
        PPECSP.save(fp);

#ifdef WITH_ALIGN_GEEHALEL
    if (align)
        align->saveConfigItems(fp);
#endif
#ifdef WITH_SCOPE_LIMITS
    if (horizon)
        horizon->saveConfigItems(fp);
#endif
    return true;
}

bool EQMod::SetTrackRate(double raRate, double deRate)
{
    try
    {
        mount->SetRARate(raRate / SKYWATCHER_STELLAR_SPEED);
        mount->SetDERate(deRate / SKYWATCHER_STELLAR_SPEED);
    }
    catch (EQModError e)
    {
        return (e.DefaultHandleException(this));
    }

    LOGF_INFO("Setting Custom Tracking Rates - RA=%.6f  DE=%.6f arcsec/s", raRate, deRate);

    return true;
}

bool EQMod::SetTrackMode(uint8_t mode)
{
    // GetRATrackRate..etc al already check TrackModeSP to obtain the appropiate tracking rate, so no need for mode here.
    INDI_UNUSED(mode);

    try
    {
        mount->StartRATracking(GetRATrackRate());
        mount->StartDETracking(GetDETrackRate());
    }
    catch (EQModError e)
    {
        return (e.DefaultHandleException(this));
    }

    return true;
}

bool EQMod::SetTrackEnabled(bool enabled)
{
    try
    {
        if (enabled)
        {
            LOGF_INFO("Start Tracking (%s).", TrackModeSP.findOnSwitch()->getLabel());
            TrackState     = SCOPE_TRACKING;
            RememberTrackState = TrackState;
            mount->StartRATracking(GetRATrackRate());
            mount->StartDETracking(GetDETrackRate());
        }
        else if (enabled == false)
        {
            LOGF_WARN("Stopping Tracking (%s).", TrackModeSP.findOnSwitch()->getLabel());
            TrackState     = SCOPE_IDLE;
            RememberTrackState = TrackState;
            mount->StopRA();
            mount->StopDE();
        }
    }
    catch (EQModError e)
    {
        return (e.DefaultHandleException(this));
    }

    return true;
}

IPState EQMod::ExecuteHomeAction(TelescopeHomeAction action)
{
    if (action != HOME_FIND)
        return IPS_ALERT;

    if ((TrackState != SCOPE_IDLE) && (TrackState != SCOPE_AUTOHOMING))
    {
        LOG_WARN("Can not start AutoHome. Scope not idle");
        return IPS_IDLE;
    }

    if (TrackState == SCOPE_AUTOHOMING)
    {
        LOG_WARN("Aborting AutoHome.");
        Abort();
        return IPS_IDLE;
    }

    if (AutohomeState == AUTO_HOME_IDLE)
    {
        LOG_INFO("Starting Autohome.");
        TrackState = SCOPE_AUTOHOMING;
        try
        {
            LOG_INFO("AutoHome phase 1: turning off aux encoders");
            mount->TurnRAEncoder(false);
            mount->TurnDEEncoder(false);
            LOG_INFO("AutoHome phase 1: resetting home position indexes");
            mount->ResetRAIndexer();
            mount->ResetDEIndexer();
            LOG_INFO(
                "AutoHome phase 1: reading home position indexes to set directions");
            mount->GetRAIndexer();
            mount->GetDEIndexer();
            LOGF_INFO(
                "AutoHome phase 1: read home position indexes: RA=0x%x DE=0x%x",
                mount->GetlastreadRAIndexer(), mount->GetlastreadDEIndexer());
            if (mount->GetlastreadRAIndexer() == 0)
                ah_bSlewingUp_RA = true;
            else
                ah_bSlewingUp_RA = false;
            if (mount->GetlastreadDEIndexer() == 0)
                ah_bSlewingUp_DE = true;
            else
                ah_bSlewingUp_DE = false;
            ah_iPosition_RA = mount->GetRAEncoder();
            ah_iPosition_DE = mount->GetDEEncoder();
            ah_iChanges     = (5 * mount->GetRAEncoderTotal()) / 360;
            if (ah_bSlewingUp_RA)
                ah_iPosition_RA = ah_iPosition_RA - ah_iChanges;
            else
                ah_iPosition_RA = ah_iPosition_RA + ah_iChanges;
            ah_iChanges = (5 * mount->GetDEEncoderTotal()) / 360;
            if (ah_bSlewingUp_DE)
                ah_iPosition_DE = ah_iPosition_DE - ah_iChanges;
            else
                ah_iPosition_DE = ah_iPosition_DE + ah_iChanges;
            LOG_INFO(
                "AutoHome phase 1: trying to move further away from home position");
            LOGF_INFO(
                "AutoHome phase 1: slewing to RA=0x%x (up=%c) DE=0x%x (up=%c)", ah_iPosition_RA,
                (ah_bSlewingUp_RA ? '1' : '0'), ah_iPosition_DE, (ah_bSlewingUp_DE ? '1' : '0'));
            mount->AbsSlewTo(ah_iPosition_RA, ah_iPosition_DE, ah_bSlewingUp_RA, ah_bSlewingUp_DE);
            AutohomeState = AUTO_HOME_WAIT_PHASE1;
            return IPS_BUSY;
        }
        catch (EQModError e)
        {
            AutohomeState = AUTO_HOME_IDLE;
            TrackState    = SCOPE_IDLE;
            RememberTrackState = TrackState;
            return IPS_ALERT;
        }
    }

    return IPS_ALERT;
}
