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
*/

#include "pointset.h"

#include "triangulate.h"
#include "triangulate_chull.h"
#include "indicom.h"

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <math.h>
#include <string.h>
#include <wordexp.h>

void PointSet::AltAzFromRaDec(double ra, double dec, double jd, double *alt, double *az, INDI::IGeographicCoordinates *pos)
{
    INDI::IEquatorialCoordinates lnradec;
    INDI::IGeographicCoordinates lnpos;
    INDI::IHorizontalCoordinates lnaltaz;
    lnradec.rightascension  = ra;
    lnradec.declination = dec;
    if (pos)
    {
        lnpos.longitude = pos->longitude;
        lnpos.latitude = pos->latitude;
    }
    else
    {
        lnpos.longitude = IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LONG")->value;
        lnpos.latitude = IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LAT")->value;
    }

    INDI::EquatorialToHorizontal(&lnradec, &lnpos, jd, &lnaltaz);
    *alt = lnaltaz.altitude;
    *az  = lnaltaz.azimuth;
}

void PointSet::AltAzFromRaDecSidereal(double ra, double dec, double lst, double *alt, double *az,
                                      INDI::IGeographicCoordinates *pos)
{
    struct ln_equ_posn lnradec;
    struct ln_lnlat_posn lnpos;
    struct ln_hrz_posn lnaltaz;
    lnradec.ra  = (ra * 360.0) / 24.0;
    lnradec.dec = dec;
    if (pos)
    {
        lnpos.lng = pos->longitude;
        lnpos.lat = pos->latitude;
    }
    else
    {
        lnpos.lng = IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LONG")->value;
        lnpos.lat = IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LAT")->value;
    }

    if (lnpos.lng > 180)
        lnpos.lng -= 360;

    ln_get_hrz_from_equ_sidereal_time(&lnradec, &lnpos, lst, &lnaltaz);
    *alt = lnaltaz.alt;
    *az  = range360(lnaltaz.az + 180.0);
}

void PointSet::RaDecFromAltAz(double alt, double az, double jd, double *ra, double *dec, INDI::IGeographicCoordinates *pos)
{
    INDI::IEquatorialCoordinates lnradec;
    INDI::IGeographicCoordinates lnpos;
    INDI::IHorizontalCoordinates lnaltaz;
    lnaltaz.altitude = alt;
    lnaltaz.azimuth  = az;
    if (pos)
    {
        lnpos.longitude = pos->longitude;
        lnpos.latitude = pos->latitude;
    }
    else
    {
        lnpos.longitude = IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LONG")->value;
        lnpos.latitude = IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LAT")->value;
    }

    INDI::HorizontalToEquatorial(&lnaltaz, &lnpos, jd, &lnradec);
    *ra  = lnradec.rightascension;
    *dec = lnradec.declination;
}

/* Using haversine: http://en.wikipedia.org/wiki/Haversine_formula */
double sphere_unit_distance(double theta1, double theta2, double phi1, double phi2)
{
    double sqrt_haversin_lat  = sin(((phi2 - phi1) / 2) * (M_PI / 180));
    double sqrt_haversin_long = sin(((theta2 - theta1) / 2) * (M_PI / 180));
    return (2 *
            asin(sqrt((sqrt_haversin_lat * sqrt_haversin_lat) + cos(phi1 * (M_PI / 180)) * cos(phi2 * (M_PI / 180)) *
                      (sqrt_haversin_long * sqrt_haversin_long))));
}

bool compelt(PointSet::Distance d1, PointSet::Distance d2)
{
    return d1.value < d2.value;
}

PointSet::PointSet(INDI::Telescope *t)
{
    telescope  = t;
    lnalignpos = nullptr;
    PointSetInitialized = false;
}

const char *PointSet::getDeviceName()
{
    return telescope->getDeviceName();
}

std::set<PointSet::Distance, bool (*)(PointSet::Distance, PointSet::Distance)> *
PointSet::ComputeDistances(double alt, double az, PointFilter filter, bool ingoto)
{
    INDI_UNUSED(filter);
    std::map<HtmID, Point>::iterator it;
    std::set<Distance, bool (*)(Distance, Distance)> *distances =
        new std::set<Distance, bool (*)(Distance, Distance)>(compelt);
    std::set<Distance>::iterator distit;
    std::pair<std::set<Distance>::iterator, bool> ret;
    /* IDLog("Compute distances for point alt=%f az=%f\n", alt, az);*/
    for (it = PointSetMap->begin(); it != PointSetMap->end(); it++)
    {
        double d;
        if (ingoto)
            d = sphere_unit_distance(az, (*it).second.celestialAZ, alt, (*it).second.celestialALT);
        else
            d = sphere_unit_distance(az, (*it).second.telescopeAZ, alt, (*it).second.telescopeALT);
        Distance elt;
        elt.htmID = (*it).first;
        elt.value = d;
        ret       = distances->insert(elt);
        /*IDLog("  Point %lld (alt=%f az=%f): distance %f \n",  elt.htmID, (*it).second.celestialALT, (*it).second.celestialAZ, elt.value);*/
    }
    /*
    IDLog("  Ordered distances for point alt=%f az=%f\n", alt, az);
    for ( distit=distances->begin() ; distit != distances->end(); distit++ ) {
    IDLog("  Point %lld: distance %f \n",  distit->htmID, distit->value);
    }
    */
    return distances;
}

void PointSet::AddPoint(AlignData aligndata, INDI::IGeographicCoordinates *pos)
{
    Point point;
    point.aligndata = aligndata;
    double horangle, altangle;
    //point.celestialAZ = (range24(point.aligndata.lst - point.aligndata.targetRA - 12.0) * 360.0) / 24.0;
    //point.telescopeAZ = (range24(point.aligndata.lst - point.aligndata.telescopeRA - 12.0) * 360.0) / 24.0;
    //point.celestialALT = point.aligndata.targetDEC + lat;
    //point.telescopeALT = point.aligndata.telescopeDEC + lat;
    if (point.aligndata.jd > 0.0)
    {
        AltAzFromRaDec(point.aligndata.targetRA, point.aligndata.targetDEC, point.aligndata.jd, &point.celestialALT,
                       &point.celestialAZ, pos);
        AltAzFromRaDec(point.aligndata.telescopeRA, point.aligndata.telescopeDEC, point.aligndata.jd,
                       &point.telescopeALT, &point.telescopeAZ, pos);
    }
    else
    {
        AltAzFromRaDecSidereal(point.aligndata.targetRA, point.aligndata.targetDEC, point.aligndata.lst,
                               &point.celestialALT, &point.celestialAZ, pos);
        AltAzFromRaDecSidereal(point.aligndata.telescopeRA, point.aligndata.telescopeDEC, point.aligndata.lst,
                               &point.telescopeALT, &point.telescopeAZ, pos);
    }
    horangle    = range360(-180.0 - point.celestialAZ) * M_PI / 180.0;
    altangle    = point.celestialALT * M_PI / 180.0;
    point.cx    = cos(altangle) * cos(horangle);
    point.cy    = cos(altangle) * sin(horangle);
    point.cz    = sin(altangle);
    horangle    = range360(-180.0 - point.telescopeAZ) * M_PI / 180.0;
    altangle    = point.telescopeALT * M_PI / 180.0;
    point.tx    = cos(altangle) * cos(horangle);
    point.ty    = cos(altangle) * sin(horangle);
    point.tz    = sin(altangle);
    point.htmID = cc_radec2ID(point.celestialAZ, point.celestialALT, 19);
    cc_ID2name(point.htmname, point.htmID);
    point.index = getNbPoints();
    PointSetMap->insert(std::pair<HtmID, Point>(point.htmID, point));
    Triangulation->AddPoint(point.htmID);
    LOGF_INFO("Align Pointset: added point %d alt = %g az = %g\n", point.index,
              point.celestialALT, point.celestialAZ);
    LOGF_INFO("Align Triangulate: number of faces is %d\n", Triangulation->getFaces().size());
}

PointSet::Point *PointSet::getPoint(HtmID htmid)
{
    return &(PointSetMap->find(htmid)->second);
}

int PointSet::getNbPoints()
{
    return PointSetMap->size();
}

int PointSet::getNbTriangles()
{
    return Triangulation->getFaces().size();
}

bool PointSet::isInitialized()
{
    return  PointSetInitialized;
}

void PointSet::Init()
{
    //  PointSetMap=nullptr;
    PointSetMap     = new std::map<HtmID, Point>();
    Triangulation   = new TriangulateCHull(PointSetMap);
    PointSetXmlRoot = nullptr;
    PointSetInitialized = true;
}

void PointSet::Reset()
{
    current.clear();
    if (PointSetMap)
    {
        PointSetMap->clear();
        //delete(PointSetMap);
    }
    //PointSetMap=nullptr;
    if (PointSetXmlRoot)
        delXMLEle(PointSetXmlRoot);
    PointSetXmlRoot = nullptr;
    if (lnalignpos)
        free(lnalignpos);
    lnalignpos = nullptr;
    Triangulation->Reset();
}

char *PointSet::LoadDataFile(const char *filename)
{
    wordexp_t wexp;
    FILE *fp;
    LilXML *lp;
    static char errmsg[512];
    AlignData aligndata;
    XMLEle *alignxml, *sitexml;
    XMLAtt *ap;
    char *sitename;
    std::map<HtmID, Point>::iterator it;

    if (wordexp(filename, &wexp, 0))
    {
        wordfree(&wexp);
        return (char *)("Badly formed filename");
    }
    //if (filename == nullptr) return;
    if (!(fp = fopen(wexp.we_wordv[0], "r")))
    {
        wordfree(&wexp);
        return strerror(errno);
    }
    wordfree(&wexp);
    lp = newLilXML();
    if (PointSetXmlRoot)
        delXMLEle(PointSetXmlRoot);
    PointSetXmlRoot = readXMLFile(fp, lp, errmsg);
    delLilXML(lp);
    if (!PointSetXmlRoot)
        return errmsg;
    if (!strcmp(tagXMLEle(nextXMLEle(PointSetXmlRoot, 1)), "aligndata"))
        return (char *)("Not an alignement data file");
    sitexml = findXMLEle(PointSetXmlRoot, "site");
    if (!sitexml)
        return (char *)"No site found";
    ap = findXMLAtt(sitexml, "name");
    if (ap)
        sitename = valuXMLAtt(ap);
    else
        sitename = (char *)"No sitename";
    ap = findXMLAtt(sitexml, "lat");
    if (!ap)
        return (char *)"No latitude data found";
    else
        sscanf(valuXMLAtt(ap), "%lf", &lat);
    ap = findXMLAtt(sitexml, "lon");
    if (!ap)
        return (char *)"No longitude data found";
    else
        sscanf(valuXMLAtt(ap), "%lf", &lon);
    ap = findXMLAtt(sitexml, "alt");
    if (!ap)
        alt = 0.0;
    else
        sscanf(valuXMLAtt(ap), "%lf", &alt);
    IDLog("Align: load file for site %s (lon %f lat %f alt %f)\n", sitename, lon, lat, alt);
    IDLog("  number of points: %d\n", nXMLEle(sitexml));
    //  PointSetMap = new std::map<HtmID, Point>();
    if (lnalignpos)
        free(lnalignpos);
    lnalignpos      = (INDI::IGeographicCoordinates *)malloc(sizeof(INDI::IGeographicCoordinates));
    lnalignpos->longitude = lon;
    lnalignpos->latitude = lat;
    PointSetMap->clear();
    alignxml     = nextXMLEle(sitexml, 1);
    aligndata.jd = -1.0;
    while (alignxml)
    {
        if (strcmp(tagXMLEle(alignxml), "point") != 0)
            break;
        //IDLog("synctime %s\n", pcdataXMLEle(findXMLEle(alignxml, "synctime")));
        sscanf(pcdataXMLEle(findXMLEle(alignxml, "synctime")), " %lf ", &aligndata.lst);
        sscanf(pcdataXMLEle(findXMLEle(alignxml, "celestialra")), "%lf", &aligndata.targetRA);
        sscanf(pcdataXMLEle(findXMLEle(alignxml, "celestialde")), "%lf", &aligndata.targetDEC);
        sscanf(pcdataXMLEle(findXMLEle(alignxml, "telescopera")), "%lf", &aligndata.telescopeRA);
        sscanf(pcdataXMLEle(findXMLEle(alignxml, "telescopede")), "%lf", &aligndata.telescopeDEC);
        //IDLog("Load alignment point: %f %f %f %f %f\n", aligndata.lst, aligndata.targetRA, aligndata.targetDEC,
        //  aligndata.telescopeRA, aligndata.telescopeDEC);
        AddPoint(aligndata, lnalignpos);
        alignxml = nextXMLEle(sitexml, 0);
    }
    /*
    IDLog("Resulting Alignment map;\n");
    for ( it=PointSetMap->begin() ; it != PointSetMap->end(); it++ )
    IDLog("  Point htmID= %lld, htm name = %s,  telescope alt = %f az = %f\n",  (*it).first, (*it).second.htmname,
      (*it).second.telescopeALT,  (*it).second.telescopeAZ);
    */
    return nullptr;
}

char *PointSet::WriteDataFile(const char *filename)
{
    wordexp_t wexp;
    FILE *fp;
    XMLEle *root;

    if (wordexp(filename, &wexp, 0))
    {
        wordfree(&wexp);
        return (char *)("Badly formed filename");
    }
    if (lnalignpos)
    {
        // Why this ?
        if ((fabs(lnalignpos->longitude - IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LONG")->value) > 1E-4) ||
                (fabs(lnalignpos->latitude - IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LAT")->value) > 1E-4))
            return (char *)("Can not mix alignment data from different sites (lng. and/or lat. differs)");
    }
    //if (filename == nullptr) return;
    if (!(fp = fopen(wexp.we_wordv[0], "w")))
    {
        wordfree(&wexp);
        return strerror(errno);
    }
    root = toXML();

    prXMLEle(fp, root, 0);
    fclose(fp);
    return nullptr;
}

XMLEle *PointSet::toXML()
{
    AlignData aligndata;
    XMLEle *root = nullptr, *alignxml = nullptr, *sitexml = nullptr; //, *mountxml;
    char sitename[26];
    char sitedata[26];
    std::map<HtmID, Point>::iterator it;
    time_t tnow;
    struct tm tm_now;

    root    = addXMLEle(nullptr, "aligndata");
    sitexml = addXMLEle(root, "site");
    /* WARNING When an align data file has been loaded this should be taken from the file, not current session */
    tnow = time(nullptr);
    localtime_r(&tnow, &tm_now);
    strftime(sitename, sizeof(sitename), "%F@%T", &tm_now);
    addXMLAtt(sitexml, "name", sitename);

    snprintf(sitedata, sizeof(sitedata), "%g",
             ((lnalignpos == nullptr) ? IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LONG")->value :
              lnalignpos->longitude));
    addXMLAtt(sitexml, "lon", sitedata);

    snprintf(sitedata, sizeof(sitedata), "%g",
             ((lnalignpos == nullptr) ? IUFindNumber(telescope->getNumber("GEOGRAPHIC_COORD"), "LAT")->value :
              lnalignpos->latitude));
    addXMLAtt(sitexml, "lat", sitedata);

    //mountxml=addXMLEle(sitexml,"mount");
    //snprintf(sitedata, sizeof(sitedata), "%d", telescope->totalRAEncoder);
    //addXMLAtt(mountxml, "totalRA", sitedata);

    for (it = PointSetMap->begin(); it != PointSetMap->end(); it++)
    {
        XMLEle *data;
        char pcdata[30];
        aligndata = (*it).second.aligndata;
        alignxml  = addXMLEle(sitexml, "point");
        data      = addXMLEle(alignxml, "index");
        snprintf(pcdata, sizeof(pcdata), "%d", (*it).second.index);
        editXMLEle(data, pcdata);
        data = addXMLEle(alignxml, "synctime");
        snprintf(pcdata, sizeof(pcdata), "%g", aligndata.lst);
        editXMLEle(data, pcdata);
        data = addXMLEle(alignxml, "celestialra");
        snprintf(pcdata, sizeof(pcdata), "%g", aligndata.targetRA);
        editXMLEle(data, pcdata);
        data = addXMLEle(alignxml, "celestialde");
        snprintf(pcdata, sizeof(pcdata), "%g", aligndata.targetDEC);
        editXMLEle(data, pcdata);
        data = addXMLEle(alignxml, "telescopera");
        snprintf(pcdata, sizeof(pcdata), "%g", aligndata.telescopeRA);
        editXMLEle(data, pcdata);
        data = addXMLEle(alignxml, "telescopede");
        snprintf(pcdata, sizeof(pcdata), "%g", aligndata.telescopeDEC);
        editXMLEle(data, pcdata);
    }
    return root;
}

void PointSet::setPointBlobData(IBLOB *blob)
{
    XMLEle *root;
    char *pointblobxml;
    int rsize, pointblobsize;
    root          = toXML();
    rsize         = sprlXMLEle(root, 0);
    pointblobsize = rsize;
    pointblobxml  = (char *)malloc((pointblobsize + 1) * sizeof(char));
    sprXMLEle(pointblobxml, root, 0);
    blob->size    = pointblobsize + 1;
    blob->bloblen = blob->size;
    strcpy(blob->format, ".xml");
    blob->blob = (void *)pointblobxml;
}

void PointSet::setTriangulationBlobData(IBLOB *blob)
{
    XMLEle *troot;
    char *triangblobxml;
    int tsize, triangblobsize;
    troot          = Triangulation->toXML();
    tsize          = sprlXMLEle(troot, 0);
    triangblobsize = tsize;
    triangblobxml  = (char *)malloc((triangblobsize + 1) * sizeof(char));
    sprXMLEle(triangblobxml, troot, 0);
    blob->size    = triangblobsize + 1;
    blob->bloblen = blob->size;
    strcpy(blob->format, ".xml");
    blob->blob = (void *)triangblobxml;
}

void PointSet::setBlobData(IBLOBVectorProperty *bp)
{
    setPointBlobData(IUFindBLOB(bp, "POINTLIST"));
    setTriangulationBlobData(IUFindBLOB(bp, "TRIANGULATION"));
}

double PointSet::scalarTripleProduct(Point *p, Point *e1, Point *e2, bool ingoto)
{
    double res;
    if (ingoto)
        res = (p->cx * e1->cy * e2->cz) + (p->cz * e1->cx * e2->cy) + (p->cy * e1->cz * e2->cx) -
              (p->cz * e1->cy * e2->cx) - (p->cx * e1->cz * e2->cy) - (p->cy * e1->cx * e2->cz);
    else
        res = (p->cx * e1->ty * e2->tz) + (p->cz * e1->tx * e2->ty) + (p->cy * e1->tz * e2->tx) -
              (p->cz * e1->ty * e2->tx) - (p->cx * e1->tz * e2->ty) - (p->cy * e1->tx * e2->tz);
    // for point on edge or vertice
    //if (res < 0.0000001) return 0.0; else return res;
    return res;
}

bool PointSet::isPointInside(Point *p, std::vector<HtmID> f, bool ingoto)
{
    double r;
    bool left  = false;
    bool right = false;
    if (f.size() < 3)
        return false;
    r = scalarTripleProduct(p, &PointSetMap->at(f[2]), &PointSetMap->at(f[0]), ingoto);
    if (r < 0)
        left = true;
    else
        right = true;
    r = scalarTripleProduct(p, &PointSetMap->at(f[0]), &PointSetMap->at(f[1]), ingoto);
    if (r < 0)
        left = true;
    else
        right = true;
    if (left && right)
        return false;
    r = scalarTripleProduct(p, &PointSetMap->at(f[1]), &PointSetMap->at(f[2]), ingoto);
    if (r < 0)
        left = true;
    else
        right = true;
    if (left && right)
        return false;
    return true;
}

std::vector<HtmID> PointSet::findFace(double currentRA, double currentDEC, double jd, double pointalt, double pointaz,
                                      INDI::IGeographicCoordinates *position, bool ingoto)
{
    INDI_UNUSED(pointalt);
    INDI_UNUSED(pointaz);
    Point point;
    double horangle = 0, altangle = 0;
    std::vector<Face *> faces;
    std::vector<Face *>::iterator it;

    point.aligndata.jd        = jd;
    point.aligndata.targetRA  = currentRA;
    point.aligndata.targetDEC = currentDEC;
    AltAzFromRaDec(point.aligndata.targetRA, point.aligndata.targetDEC, point.aligndata.jd, &point.celestialALT,
                   &point.celestialAZ, position);

    horangle = range360(-180.0 - point.celestialAZ) * M_PI / 180.0;
    altangle = point.celestialALT * M_PI / 180.0;
    point.cx = cos(altangle) * cos(horangle);
    point.cy = cos(altangle) * sin(horangle);
    point.cz = sin(altangle);

    if (Triangulation->isValid() && isPointInside(&point, current, ingoto))
        return current;
    faces = Triangulation->getFaces();
    it    = faces.begin();
    while (it < faces.end())
    {
        if (isPointInside(&point, (*it)->v, ingoto))
        {
            currentFace = *it;
            current     = (*it)->v;
            LOGF_INFO("Align: current face is {%d, %d, %d}", PointSetMap->at(current[0]).index,
                      PointSetMap->at(current[1]).index, PointSetMap->at(current[2]).index);
            return current;
        }
        it++;
    }
    if (current.size() > 0)
        LOG_INFO("Align: current face is empty");
    current.clear();
    return current;
}
