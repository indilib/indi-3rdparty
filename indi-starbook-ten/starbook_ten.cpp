#include <regex>
#include <cmath>
#include <stdio.h>
#include "starbook_ten.h"

StarbookTen::StarbookTen(httplib::Client *http) : http(http) {
    setHttpClient(http);
}


StarbookTen::StarbookTen(const char *base_url) {
    http = new httplib::Client(base_url);

    http->set_connection_timeout(2, 0);
    http->set_read_timeout(3, 0);
    http->set_write_timeout(3, 0);

    http->set_keep_alive(true);

    http->set_url_encode(false);

    destroyClient = true;
}


StarbookTen::~StarbookTen() {
    if (destroyClient)
        delete http;
}


void
StarbookTen::setHttpClient(httplib::Client *http) {
    if (http) {
        http->set_connection_timeout(2, 0);
        http->set_read_timeout(3, 0);
        http->set_write_timeout(3, 0);

        http->set_keep_alive(true);

        http->set_url_encode(false);
    }

    this->http = http;
    destroyClient = false;
}


bool
StarbookTen::sendBasicCmd(const char *cmd) {
    auto res = http->Get(cmd);

    if (!res || res->status != 200) {
        throw std::runtime_error("sendBasicCmd HTTP error");
    }

    if (res->body.find(R"(<!--OK-->)") == std::string::npos) {
        throw std::runtime_error("sendBasicCmd response error");
    }

    return true;
}


std::tuple<int,int>
StarbookTen::getFirmwareVersion() {
    auto res = http->Get("/version");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(<!--VERSION=([0-9]+)\.([0-9]+)-->)");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        int vmaj = std::stoi(sm[1]);
        int vmin = std::stoi(sm[2]);

        return std::tuple<int,int>(vmaj, vmin);
    } else {
        throw std::runtime_error("Could not get version");
    }
}


StarbookTen::PierSide
StarbookTen::getPierSide() {
    auto res = http->Get("/get_pierside");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(PIERSIDE=([01]))");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        return static_cast<StarbookTen::PierSide>(std::stoi(sm[1]));
    } else {
        throw std::runtime_error("Could not get pier side");
    }
}


StarbookTen::PierSide
StarbookTen::getNewPierSide(double ra, double dec) {
    std::stringstream cmd_ss;
    cmd_ss << "/calc_sideofpier?ra=" << ra << "&dec=" << dec;
    auto res = http->Get(cmd_ss.str().c_str());

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(PIERSIDE=([01]))");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        return static_cast<StarbookTen::PierSide>(std::stoi(sm[1]));
    } else {
        throw std::runtime_error("Could not get new pier side");
    }
}


bool
StarbookTen::setPierSide(PierSide pierside) {
    std::stringstream cmd_ss;
    cmd_ss << "/set_pierside?pierside=" << static_cast<int>(pierside);
    return sendBasicCmd(cmd_ss.str().c_str());
}


ln_zonedate
StarbookTen::getDateTime() {
    ln_zonedate zdt;

    auto res = http->Get("/gettime");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(TIME=(\d{4})\+(\d{1,2})\+(\d{1,2})\+(\d{1,2})\+(\d{1,2})\+(\d{1,2}))");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        zdt.years = std::stoi(sm[1]);
        zdt.months = std::stoi(sm[2]);
        zdt.days = std::stoi(sm[3]);
        zdt.hours = std::stoi(sm[4]);
        zdt.minutes = std::stoi(sm[5]);
        zdt.seconds = std::stoi(sm[6]);
    } else {
        throw std::runtime_error("Could not get time");
    }

    res = http->Get("/getplace");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex rtz(R"(<!--.*timezone=([+-]?\d+)-->)");
    std::smatch smtz;

    if (std::regex_search(res->body, smtz, rtz)) {
        zdt.gmtoff = std::stoi(smtz[1])*3600;
    } else {
        throw std::runtime_error("Could not get timezone");
    }

    return zdt;
}


bool
StarbookTen::setDateTime(const ln_zonedate& zdt) {
    char buf[256];
    std::tuple<double,double> latlon = getLatLon();

    snprintf(buf, sizeof(buf)-1,
             "/settime?TIME=%04d+%02d+%02d+%02d+%02d+%02d",
             zdt.years, zdt.months, zdt.days,
             zdt.hours, zdt.minutes, static_cast<int>(zdt.seconds));

    sendBasicCmd(buf);

    return setLatLonTz(std::get<0>(latlon), std::get<1>(latlon), zdt.gmtoff);
}


bool
StarbookTen::setLatLon(double lat, double lon) {
    ln_zonedate zdt = getDateTime();

    return setLatLonTz(lat, lon, zdt.gmtoff);
}


bool
StarbookTen::setLatLonTz(double lat, double lon, long utc_off_secs) {
    char buf[256];
    ln_dms lat_dms, lon_dms;

    ln_deg_to_dms(lat, &lat_dms);
    ln_deg_to_dms(lon, &lon_dms);

    snprintf(buf, sizeof(buf)-1,
             "/setplace?longitude=%s%d+%d&latitude=%s%d+%d&timezone=%ld",
             (lon_dms.neg ? "W" : "E"), lon_dms.degrees, lon_dms.minutes,
             (lat_dms.neg ? "S" : "N"), lat_dms.degrees, lat_dms.minutes,
             utc_off_secs/3600);

    return sendBasicCmd(buf);
}


std::tuple<double,double>
StarbookTen::getLatLon() {
    auto res = http->Get("/getplace");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(<!--longitude=([EW])(\d+)\+(\d+)&latitude=([NS])(\d+)\+(\d+)&.*-->)");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        ln_dms lon_dms, lat_dms;

        lon_dms.neg = (sm[1].compare("W") == 0) ? 1 : 0;
        lon_dms.degrees = std::stoi(sm[2]);
        lon_dms.minutes = std::stoi(sm[3]);
        lon_dms.seconds = 0;

        lat_dms.neg = (sm[4].compare("S") == 0) ? 1 : 0;
        lat_dms.degrees = std::stoi(sm[5]);
        lat_dms.minutes = std::stoi(sm[6]);
        lat_dms.seconds = 0;

        return std::tuple<double,double>(ln_dms_to_deg(&lat_dms), ln_dms_to_deg(&lon_dms));
    } else {
        throw std::runtime_error("Could not get lat/long");
    }
}


StarbookTen::CoordType
StarbookTen::getCoordType() {
    auto res = http->Get("/getradectype");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"((J2000|NOW))");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        return (sm[1].compare("J2000") == 0) ? COORD_TYPE_J2000 : COORD_TYPE_NOW;
    } else {
        throw std::runtime_error("Could not get coordinate type");
    }
}


bool
StarbookTen::setCoordType(CoordType ct) {
    return sendBasicCmd((ct == COORD_TYPE_J2000) ? "/setradectype?type=J2000" : "/setradectype?type=NOW");
}


StarbookTen::MountStatus
StarbookTen::getStatus() {
    auto res = http->Get("/getstatus2");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(<!--RA=(\-?\d+\.\d+)&DEC=(\-?\d+\.\d+)&GOTO=([01])&STATE=([A-Z]+)-->)");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        MountStatus stat;

        stat.ra = std::stod(sm[1]);
        stat.dec = std::stod(sm[2]);
        stat.goto_busy = !(sm[3].compare("0") == 0);
        stat.state =
            (sm[4].compare("USER") == 0)  ? STATE_USER  :
            (sm[4].compare("CHART") == 0) ? STATE_CHART :
            (sm[4].compare("SCOPE") == 0) ? STATE_SCOPE : STATE_INIT;

        return stat;
    } else {
        throw std::runtime_error("Could not get status");
    }
}

bool
StarbookTen::isTracking() {
    auto res = http->Get("/gettrackstatus");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    // TRACK=2 seems to be used during gotos, but since we can already figure
    // gotos out from the getstatus2 call, there's no need to handle it here.
    std::regex r(R"(<!--TRACK=([012])-->)");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        return !(sm[1].compare("1"));
    } else {
        throw std::runtime_error("Could not get track status");
    }
}


std::tuple<bool,bool>
StarbookTen::getGuidingRaDec() {
    auto res = http->Get("/getguidestatus");

    if (!res || res->status != 200) {
        throw std::runtime_error("HTTP get failed");
    }

    std::regex r(R"(<!--RA\+=([01])&RA\-=([01])&DEC\+=([01])&DEC\-=([01])-->)");
    std::smatch sm;

    if (std::regex_search(res->body, sm, r)) {
        return std::tuple<bool,bool>((!(sm[1].compare("1")) || !(sm[2].compare("1"))),
                                     (!(sm[3].compare("1")) || !(sm[4].compare("1"))));
    } else {
        throw std::runtime_error("Could not get guide status");
    }
}


std::tuple<double,double>
StarbookTen::getRaDec() {
    auto stat = getStatus();

    return std::tuple<double,double>(stat.ra, stat.dec);
}


bool
StarbookTen::setPulseRate(int ra_arcsec_per_sec, int dec_arcsec_per_sec) {
    std::stringstream cmd_ss;
    cmd_ss << "/setpulsespeed?ra=" << ra_arcsec_per_sec << "&dec=" << dec_arcsec_per_sec;
    return sendBasicCmd(cmd_ss.str().c_str());
}


bool
StarbookTen::movePulse(GuideDirection dir, uint32_t ms) {
    std::stringstream cmd_ss;
    cmd_ss << "/movepulse?direct=" << static_cast<int>(dir) << "&duration=" << ms;
    return sendBasicCmd(cmd_ss.str().c_str());
}


bool
StarbookTen::home() {
    return sendBasicCmd("/home");
}


bool
StarbookTen::park() {
    return sendBasicCmd("/goto_park");
}


bool
StarbookTen::unpark() {
    return sendBasicCmd("/unpark");
}


bool
StarbookTen::setParkCurrent() {
    return sendBasicCmd("/set_park");
}


bool
StarbookTen::findHome() {
    return sendBasicCmd("/home");
}


bool
StarbookTen::start(bool init) {
    return sendBasicCmd(init ? "/start" : "/start?init=off");
}


bool
StarbookTen::stop() {
    return sendBasicCmd("/stop");
}


bool
StarbookTen::setSlewRate(int index) {
    if (index < 0 || index > 8) {
        throw std::runtime_error("Invalid slew rate");
    }

    std::stringstream cmd_ss;
    cmd_ss << "/setspeed?speed=" << index;
    return sendBasicCmd(cmd_ss.str().c_str());
}


bool
StarbookTen::goTo(double ra, double dec) {
    std::stringstream cmd_ss;
    cmd_ss << "/gotoradec?ra=" << sxfmt(ra) << "&dec=" << sxfmt(dec);
    return sendBasicCmd(cmd_ss.str().c_str());
}


bool
StarbookTen::sync(double ra, double dec) {
    std::stringstream cmd_ss;
    cmd_ss << "/align?ra=" << sxfmt(ra) << "&dec=" << sxfmt(dec);
    return sendBasicCmd(cmd_ss.str().c_str());
}


bool
StarbookTen::move(StarbookTen::Axis axis, double rate) {
    if (fabs(rate) > StarbookTen::slewRates[8]) {
        throw std::runtime_error("Invalid move rate");
    }

    std::stringstream cmd_ss;
    cmd_ss << "/move_axis?axis=" << static_cast<int>(axis) << "&rate=" << rate;
    return sendBasicCmd(cmd_ss.str().c_str());
}


std::string
StarbookTen::sxfmt(double x) {
    char buf[32];

    int w = (int)x;
    double frac = x-w;
    double mins = frac * 60.0;

    snprintf(buf, sizeof(buf)-1, "%d+%02.5lf", w, mins);

    return std::string(buf);
}

const double StarbookTen::slewRates[] = {
    0.5 * 360.0/86400.0,
      1 * 360.0/86400.0,
      2 * 360.0/86400.0,
      5 * 360.0/86400.0,
     10 * 360.0/86400.0,
     30 * 360.0/86400.0,
    100 * 360.0/86400.0,
    300 * 360.0/86400.0,
    500 * 360.0/86400.0
};
