#include "indicom.h"
#include "indi_starbook_ten.h"
#include "config.h"

#define MOUNT_TAB "Mount"

template <typename Tr>
Tr retry(int retries, std::function<Tr()> f) {
    for (;;) {
        try {
            return f();
        } catch (std::exception& ex) {
            if (retries-- > 0) {
                continue;
            } else {
                throw;
            }
        }
    }
}

template <typename Tr, typename Tf, typename Tc, typename... Args>
Tr retry(int retries, Tf f, Tc inst, Args&& ... args) {
    for (;;) {
        try {
            return (inst ->* f)(std::forward<Args>(args)...);
        } catch (std::exception& ex) {
            if (retries-- > 0) {
                continue;
            } else {
                throw;
            }
        }
    }
}

static std::unique_ptr<INDIStarbookTen> scope(new INDIStarbookTen());

void ISGetProperties(const char *dev)
{
    scope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    scope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    scope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    scope->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    scope->ISSnoopDevice(root);
}


INDIStarbookTen::INDIStarbookTen() {
    setVersion(STARBOOK_TEN_VERSION_MAJOR, STARBOOK_TEN_VERSION_MINOR);

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_PIER_SIDE,
                           9);

    setTelescopeConnection(CONNECTION_NONE);

    starbook = new StarbookTen(static_cast<httplib::Client *>(nullptr));
}


INDIStarbookTen::~INDIStarbookTen() {
    delete starbook;
}


const char *
INDIStarbookTen::getDefaultName() {
    return "Starbook Ten";
}


bool
INDIStarbookTen::initProperties() {
    INDI::Telescope::initProperties();

    httpConnection = new Connection::HTTP(this);
    httpConnection->registerHandshake([&]()
    {
        return callHandshake();
    });

    registerConnection(httpConnection);

    httpConnection->setDefaultHost("http://169.254.0.1");

    IUFillText(&InfoT[MI_FW_VERSION], "MI_FW_VERSION", "Firmware Version", nullptr);
    IUFillTextVector(&InfoTP, InfoT, MI_LAST, getDeviceName(), "MOUNT_INFO",
                     "Mount Info", MOUNT_TAB, IP_RO, 60, IPS_IDLE);


    IUFillText(&StateT[MS_STATE], "STATE", "State", nullptr);
    IUFillTextVector(&StateTP, StateT, MS_LAST, getDeviceName(), "MOUNT_STATE",
                     "Status", MOUNT_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&SlewRateS[0], "0.5x", "0.5x", ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "1x", "1x", ISS_OFF);
    IUFillSwitch(&SlewRateS[2], "2x", "2x", ISS_OFF);
    IUFillSwitch(&SlewRateS[3], "5x", "5x", ISS_OFF);
    IUFillSwitch(&SlewRateS[4], "10x", "10x", ISS_OFF);
    IUFillSwitch(&SlewRateS[5], "30x", "30x", ISS_ON);
    IUFillSwitch(&SlewRateS[6], "100x", "100x", ISS_OFF);
    IUFillSwitch(&SlewRateS[7], "300x", "300x", ISS_OFF);
    IUFillSwitch(&SlewRateS[8], "500x", "500x", ISS_OFF);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 9, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&GuideRateN[GR_RA], "RA_GUIDE_RATE", "RA (arcsec/sec)", "%.0f", 0, 30, 1, 15);
    IUFillNumber(&GuideRateN[GR_DE], "DE_GUIDE_RATE", "DEC (arcsec/sec)", "%.0f", 0, 30, 1, 15);
    IUFillNumberVector(&GuideRateNP, GuideRateN, GR_LAST, getDeviceName(),
                       "GUIDE_RATE", "Guiding Rate", GUIDE_TAB, IP_RW, 0,
                       IPS_IDLE);

    IUFillSwitch(&HomeS[HS_FIND_HOME], "FindHome", "Find Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, HS_LAST, getDeviceName(), "TELESCOPE_HOME", "Homing", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    INDI::GuiderInterface::initGuiderProperties(getDeviceName(), GUIDE_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addDebugControl();
    addConfigurationControl();

    return true;
}


bool
INDIStarbookTen::updateProperties() {
    INDI::Telescope::updateProperties();

    if (isConnected()) {
        defineProperty(&InfoTP);
        defineProperty(&StateTP);
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);
        defineProperty(&HomeSP);

        return fetchStartupInfo();
    } else {
        deleteProperty(InfoTP.name);
        deleteProperty(StateTP.name);
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
        deleteProperty(HomeSP.name);

        return true;
    }
}


bool
INDIStarbookTen::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &GuideRateNP);

    return INDI::Telescope::saveConfigItems(fp);
}


bool
INDIStarbookTen::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {
        if (!strcmp(GuideRateNP.name, name)) {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            try {
                int ra_rate = (int)GuideRateN[GR_RA].value;
                int de_rate = (int)GuideRateN[GR_DE].value;

                LOGF_INFO("Setting guide rate RA=%d arcsec/sec, DE=%d arcsec/sec", ra_rate, de_rate);

                retry<bool>(1, &StarbookTen::setPulseRate, starbook, ra_rate, de_rate);
                GuideRateNP.s = IPS_OK;
            } catch (std::exception &ex) {
                LOGF_ERROR("Setting guide rate failed: %s", ex.what());
                GuideRateNP.s = IPS_ALERT;
            }

            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        } else if (!strcmp(GuideNSNP.name, name)) {
            LOG_DEBUG("Prop guiding in DE started");
            isPropGuidingDE = true;
            INDI::GuiderInterface::processGuiderProperties(name, values, names, n);
            return true;
        } else if (!strcmp(GuideWENP.name, name)) {
            LOG_DEBUG("Prop guiding in RA started");
            isPropGuidingRA = true;
            INDI::GuiderInterface::processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool
INDIStarbookTen::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {
        if (!strcmp(HomeSP.name, name)) {
            if (HomeSP.s == IPS_BUSY) {
                LOG_WARN("Find home is already in progress.");
                return true;
            }

            try {
                LOG_INFO("Find home started");
                retry<bool>(2, &StarbookTen::findHome, starbook);
                TrackState = SCOPE_SLEWING;
                HomeS[HS_FIND_HOME].s = ISS_ON;
                HomeSP.s = IPS_BUSY;
            } catch (std::exception &ex) {
                LOGF_ERROR("Find home failed: %s", ex.what());
                HomeS[HS_FIND_HOME].s = ISS_OFF;
                HomeSP.s = IPS_ALERT;
            }

            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}


bool
INDIStarbookTen::Handshake() {
    auto http = httpConnection->getClient();
    starbook->setHttpClient(http);

    try {
        starbook->getFirmwareVersion();
        retry<bool>(2, &StarbookTen::start, starbook, true);
        return true;
    } catch (std::exception& ex) {
        LOGF_ERROR("Handshake failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::updateStarbookState(StarbookTen::MountStatus& stat) {
    IUSaveText(&StateT[MS_STATE],
               (stat.state == StarbookTen::STATE_INIT)  ? "INIT"  :
               (stat.state == StarbookTen::STATE_USER)  ? "USER"  :
               (stat.state == StarbookTen::STATE_CHART) ? "CHART" : "SCOPE");
    StateTP.s = IPS_OK;
    IDSetText(&StateTP, nullptr);

    return true;
}

bool
INDIStarbookTen::fetchStartupInfo() {
    try {
        LOG_INFO("Getting startup data...");

        std::tuple<int,int> ver = starbook->getFirmwareVersion();
        auto stat = starbook->getStatus();
        ln_zonedate zdt = starbook->getDateTime();
        std::tuple<double,double> loc = starbook->getLatLon();

        std::stringstream ver_ss;
        ver_ss << std::get<0>(ver) << "." << std::get<1>(ver);

        IUSaveText(&InfoT[MI_FW_VERSION], ver_ss.str().c_str());
        InfoTP.s = IPS_OK;
        IDSetText(&InfoTP, nullptr);

        double lon = std::get<1>(loc);
        LocationN[LOCATION_LATITUDE].value  = std::get<0>(loc);
        LocationN[LOCATION_LONGITUDE].value = (lon < 0) ? (lon + 360) : lon;
        LocationNP.s = IPS_OK;
        IDSetNumber(&LocationNP, nullptr);

        char str[128];
        snprintf(str, sizeof(str)-1, "%04d-%02d-%02dT%02d:%02d:%02d",
                 zdt.years, zdt.months, zdt.days,
                 zdt.hours, zdt.minutes, (int)zdt.seconds);
        IUSaveText(&TimeT[0], str);

        snprintf(str, sizeof(str)-1, "%.2f", (zdt.gmtoff/3600.0));
        IUSaveText(&TimeT[1], str);

        TimeTP.s = IPS_OK;
        IDSetText(&TimeTP, nullptr);

        return updateStarbookState(stat);
    } catch (std::exception &ex) {
        LOGF_ERROR("fetchStartupInfo failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::ReadScopeStatus() {
    try {
        auto stat = retry<StarbookTen::MountStatus>(2, &StarbookTen::getStatus, starbook);
        bool isTracking = retry<bool>(2, &StarbookTen::isTracking, starbook);

        updateStarbookState(stat);

        if (stat.goto_busy) {
            if ((TrackState == SCOPE_IDLE) ||
                (TrackState == SCOPE_TRACKING))
                TrackState = SCOPE_SLEWING;
        } else {
            if (TrackState == SCOPE_PARKING) {
                SetParked(true);
            } else if ((stat.state == StarbookTen::STATE_INIT) ||
                       (stat.state == StarbookTen::STATE_USER)) {
                TrackState = SCOPE_IDLE;
            } else {
                TrackState = isTracking ? SCOPE_TRACKING : SCOPE_IDLE;
            }

            if (HomeSP.s == IPS_BUSY) {
                LOG_INFO("Find home completed");
                HomeSP.s = IPS_OK;
                HomeS[HS_FIND_HOME].s = ISS_OFF;
                IDSetSwitch(&HomeSP, nullptr);
            }
        }

        NewRaDec(stat.ra, stat.dec);

        auto ps = retry<StarbookTen::PierSide>(2, &StarbookTen::getPierSide, starbook);
        setPierSide((ps == StarbookTen::PIERSIDE_EAST) ? INDI::Telescope::PIER_EAST : INDI::Telescope::PIER_WEST);

        if (isPropGuidingRA || isPropGuidingDE) {
            auto gs = retry<std::tuple<bool,bool> >(2, &StarbookTen::getGuidingRaDec, starbook);
            LOGF_DEBUG("Prop guiding status: RA=%d, DEC=%d", !!(std::get<0>(gs)), !!(std::get<1>(gs)));
            if (isPropGuidingRA && !std::get<0>(gs)) {
                LOG_DEBUG("Prop guiding in RA finished");
                isPropGuidingRA = false;
                INDI::GuiderInterface::GuideComplete(AXIS_RA);
            }

            if (isPropGuidingDE && !std::get<1>(gs)) {
                LOG_DEBUG("Prop guiding in DE finished");
                isPropGuidingDE = false;
                INDI::GuiderInterface::GuideComplete(AXIS_DE);
            }
        }

        return true;
    } catch (std::exception &ex) {
        LOGF_ERROR("ReadScopeStatus failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::Goto(double ra, double dec) {
    try {
        retry<bool>(2, &StarbookTen::goTo, starbook, ra, dec);
        TrackState = SCOPE_SLEWING;
        return true;
    } catch (std::exception &ex) {
        LOGF_ERROR("Goto failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::Sync(double ra, double dec) {
    try {
        retry<bool>(2, &StarbookTen::sync, starbook, ra, dec);
        NewRaDec(ra, dec);
        return true;
    } catch (std::exception &ex) {
        LOGF_ERROR("Sync failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) {
    try {
        if (command == MOTION_START) {
            double absrate = StarbookTen::slewRates[IUFindOnSwitchIndex(&SlewRateSP)];
            double rate = (dir == DIRECTION_NORTH) ? absrate : -absrate;
            return retry<bool>(2, &StarbookTen::move, starbook, StarbookTen::AXIS_SECONDARY, rate);
        } else {
            return retry<bool>(2, &StarbookTen::move, starbook, StarbookTen::AXIS_SECONDARY, 0);
        }
    } catch (std::exception &ex) {
        LOGF_ERROR("MoveNS failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) {
    try {
        if (command == MOTION_START) {
            double absrate = StarbookTen::slewRates[IUFindOnSwitchIndex(&SlewRateSP)];
            double rate = (dir == DIRECTION_EAST) ? absrate : -absrate;
            return retry<bool>(2, &StarbookTen::move, starbook, StarbookTen::AXIS_PRIMARY, rate);
        } else {
            return retry<bool>(2, &StarbookTen::move, starbook, StarbookTen::AXIS_PRIMARY, 0);
        }
    } catch (std::exception &ex) {
        LOGF_ERROR("MoveWE failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::Park() {
    try {
        retry<bool>(2, &StarbookTen::park, starbook);
        TrackState = SCOPE_PARKING;
        return true;
    } catch (std::exception &ex) {
        LOGF_ERROR("Parking failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::UnPark() {
    try {
        retry<bool>(2, &StarbookTen::unpark, starbook);
        SetParked(false);
        retry<bool>(2, &StarbookTen::start, starbook, true);
        TrackState = SCOPE_TRACKING;
        return true;
    } catch (std::exception &ex) {
        LOGF_ERROR("Un-parking failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::SetCurrentPark() {
    try {
        return retry<bool>(2, &StarbookTen::setParkCurrent, starbook);
    } catch (std::exception &ex) {
        LOGF_ERROR("SetCurrentPark failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::Abort() {
    try {
        LOG_INFO("Aborting motion");
        retry<bool>(2, &StarbookTen::move, starbook, StarbookTen::AXIS_PRIMARY, 0);
        retry<bool>(2, &StarbookTen::move, starbook, StarbookTen::AXIS_SECONDARY, 0);
        retry<bool>(2, &StarbookTen::stop, starbook);
        return retry<bool>(2, &StarbookTen::start, starbook, true);
    } catch (std::exception &ex) {
        LOGF_ERROR("Abort failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::SetTrackEnabled(bool enabled) {
    try {
        if (enabled) {
            LOG_INFO("Enabling tracking");
            return retry<bool>(2, &StarbookTen::start, starbook, true);
        } else {
            LOG_INFO("Disabling tracking");
            return retry<bool>(2, &StarbookTen::stop, starbook);
        }
    } catch (std::exception &ex) {
        LOGF_ERROR("SetTrackEnabled failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::updateTime(ln_date *utc, double utc_offset) {
    try {
        ln_zonedate zdt;

        ln_date_to_zonedate(utc, &zdt, utc_offset * 3600.0);

        return retry<bool>(2, &StarbookTen::setDateTime, starbook, zdt);
    } catch (std::exception &ex) {
        LOGF_ERROR("UpdateTime failed: %s", ex.what());
        return false;
    }
}


bool
INDIStarbookTen::updateLocation(double latitude, double longitude, double elevation) {
    INDI_UNUSED(elevation);

    try {
        retry<bool>(2, &StarbookTen::setLatLon, starbook, latitude, longitude);

        char l[32], L[32];
        fs_sexa(l, latitude, 3, 3600);
        fs_sexa(L, longitude, 4, 3600);

        LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);
        return true;
    } catch (std::exception &ex) {
        LOGF_ERROR("UpdateLocation failed: %s", ex.what());
        return false;
    }
}

bool
INDIStarbookTen::SetSlewRate(int index) {
    try {
        LOGF_INFO("Setting slew rate: %d", index);
        return retry<bool>(2, &StarbookTen::setSlewRate, starbook, index);
    } catch (std::exception &ex) {
        LOGF_ERROR("SetSlewRate failed: %s", ex.what());
        return false;
    }
}


IPState
INDIStarbookTen::GuideNorth(uint32_t ms) {
    try {
        retry<bool>(1, &StarbookTen::movePulse, starbook, StarbookTen::GUIDE_NORTH, ms);
        return IPS_OK;
    } catch (std::exception &ex) {
        LOGF_ERROR("GuideNorth failed: %s", ex.what());
        return IPS_ALERT;
    }
}


IPState
INDIStarbookTen::GuideSouth(uint32_t ms) {
    try {
        retry<bool>(1, &StarbookTen::movePulse, starbook, StarbookTen::GUIDE_SOUTH, ms);
        return IPS_OK;
    } catch (std::exception &ex) {
        LOGF_ERROR("GuideSouth failed: %s", ex.what());
        return IPS_ALERT;
    }
}


IPState
INDIStarbookTen::GuideEast(uint32_t ms) {
    try {
        retry<bool>(1, &StarbookTen::movePulse, starbook, StarbookTen::GUIDE_EAST, ms);
        return IPS_OK;
    } catch (std::exception &ex) {
        LOGF_ERROR("GuideEast failed: %s", ex.what());
        return IPS_ALERT;
    }
}


IPState
INDIStarbookTen::GuideWest(uint32_t ms) {
    try {
        retry<bool>(1, &StarbookTen::movePulse, starbook, StarbookTen::GUIDE_WEST, ms);
        return IPS_OK;
    } catch (std::exception &ex) {
        LOGF_ERROR("GuideWest failed: %s", ex.what());
        return IPS_ALERT;
    }
}
