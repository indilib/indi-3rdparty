#ifndef _STARBOOK_TEN_H_
#define _STARBOOK_TEN_H_

#include <string>
#include <libnova/julian_day.h>
#include <libnova/utility.h>
#include "httplib.h"

#define STARBOOK_TEN_DEFAULT_PULSE_RATE 288

class StarbookTen {
private:
    httplib::Client *http;

    bool sendBasicCmd(const char *cmd);
    std::string sxfmt(double x);

public:
    enum Axis {
        AXIS_PRIMARY   = 0,
        AXIS_SECONDARY = 1
    };

    enum PierSide {
        PIERSIDE_WEST = 0,
        PIERSIDE_EAST = 1
    };

    enum State {
        STATE_INIT,
        STATE_USER,
        STATE_CHART,
        STATE_SCOPE
    };

    enum GuideDirection {
        GUIDE_NORTH = 0,
        GUIDE_SOUTH = 1,
        GUIDE_EAST  = 2,
        GUIDE_WEST  = 3
    };

    enum CoordType {
        COORD_TYPE_J2000,
        COORD_TYPE_NOW
    };

    struct MountStatus {
        double ra;
        double dec;
        bool   goto_busy;
        State  state;
    };

    static const double slewRates[];

    StarbookTen(httplib::Client *http);
    StarbookTen(const char *base_url);
    ~StarbookTen();

    bool destroyClient;

    void setHttpClient(httplib::Client *http);

    std::tuple<int,int> getFirmwareVersion();

    PierSide getPierSide();
    PierSide getNewPierSide(double ra, double dec);
    bool setPierSide(PierSide pierside);

    ln_zonedate getDateTime();
    std::tuple<double,double> getLatLon();
    bool setDateTime(const ln_zonedate& zdt);
    bool setLatLon(double lat, double lon);
    bool setLatLonTz(double lat, double lon, long utc_off_secs);

    CoordType getCoordType();
    bool setCoordType(CoordType ct);

    MountStatus getStatus();
    bool isTracking();
    std::tuple<bool,bool> getGuidingRaDec();

    std::tuple<double,double> getRaDec();

    bool setPulseRate(int ra_arcsec_per_sec, int dec_arcsec_per_sec);
    bool movePulse(GuideDirection dir, uint32_t ms);

    bool home();
    bool park();
    bool unpark();
    bool setParkCurrent();
    bool findHome();

    bool start(bool init = true);
    bool stop();

    bool setSlewRate(int index);
    bool sync(double ra, double dec);
    bool goTo(double ra, double dec);

    bool move(Axis axis, double rate);
};

#endif /* _STARBOOK_TEN_H_ */
