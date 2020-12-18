/*
    indi_interferometer - a telescope array driver for INDI
    Support for AHP cross-correlators
    Copyright (C) 2020  Ilia Platone

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <termios.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/file.h>
#include <memory>
#include <regex>
#include <indicom.h>
#include <sys/stat.h>

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>
#include <libnova/julian_day.h>

#include <connectionplugins/connectionserial.h>
#include "indi_ahp_correlator.h"

static int nplots = 1;
static std::unique_ptr<AHP_XC> array(new AHP_XC());

void ISGetProperties(const char *dev)
{
    array->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    array->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(    const char *dev, const char *name, char *texts[], char *names[], int num)
{
    array->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    array->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    array->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    array->ISSnoopDevice(root);
}

std::string regex_replace_compat(const std::string &input, const std::string &pattern, const std::string &replace)
{
    std::stringstream s;
    std::regex_replace(std::ostreambuf_iterator<char>(s), input.begin(), input.end(), std::regex(pattern), replace);
    return s.str();
}

int AHP_XC::getFileIndex(const char * dir, const char * prefix, const char * ext)
{
    INDI_UNUSED(ext);

    DIR * dpdf = nullptr;
    struct dirent * epdf = nullptr;
    std::vector<std::string> files = std::vector<std::string>();

    std::string prefixIndex = prefix;
    prefixIndex             = regex_replace_compat(prefixIndex, "_ISO8601", "");
    prefixIndex             = regex_replace_compat(prefixIndex, "_XXX", "");

    // Create directory if does not exist
    struct stat st;

    if (stat(dir, &st) == -1)
    {
        if (errno == ENOENT)
        {
            LOGF_INFO("Creating directory %s...", dir);
            if (mkdir(dir, 0755) == -1)
                LOGF_ERROR("Error creating directory %s (%s)", dir, strerror(errno));
        }
        else
        {
            LOGF_ERROR("Couldn't stat directory %s: %s", dir, strerror(errno));
            return -1;
        }
    }

    dpdf = opendir(dir);
    if (dpdf != nullptr)
    {
        while ((epdf = readdir(dpdf)))
        {
            if (strstr(epdf->d_name, prefixIndex.c_str()))
                files.push_back(epdf->d_name);
        }
    }
    else
    {
        closedir(dpdf);
        return -1;
    }
    int maxIndex = 0;

    for (uint32_t i = 0; i < files.size(); i++)
    {
        int index = -1;

        std::string file  = files.at(i);
        std::size_t start = file.find_last_of("_");
        std::size_t end   = file.find_last_of(".");
        if (start != std::string::npos)
        {
            index = atoi(file.substr(start + 1, end).c_str());
            if (index > maxIndex)
                maxIndex = index;
        }
    }

    closedir(dpdf);
    return (maxIndex + 1);
}

void AHP_XC::sendFile(IBLOB* Blobs, IBLOBVectorProperty BlobP, int len)
{
    bool sendImage = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveImage = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);
    for(int x = 0; x < len; x++) {
        if (saveImage)
        {
            snprintf(Blobs[x].format, MAXINDIBLOBFMT, ".%s", PrimaryCCD.getImageExtension());

            FILE * fp = nullptr;
            char imageFileName[MAXRBUF];

            std::string prefix = UploadSettingsT[UPLOAD_PREFIX].text;
            int maxIndex       = getFileIndex(UploadSettingsT[UPLOAD_DIR].text, UploadSettingsT[UPLOAD_PREFIX].text,
                                              Blobs[x].format);

            if (maxIndex < 0)
            {
                LOGF_ERROR("Error iterating directory %s. %s", UploadSettingsT[0].text,
                           strerror(errno));
                return;
            }

            if (maxIndex > 0)
            {
                char ts[32];
                struct tm * tp;
                time_t t;
                time(&t);
                tp = localtime(&t);
                strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
                std::string filets(ts);
                prefix = std::regex_replace(prefix, std::regex("ISO8601"), filets);

                char indexString[8];
                snprintf(indexString, 8, "%03d", maxIndex);
                std::string prefixIndex = indexString;
                //prefix.replace(prefix.find("XXX"), std::string::npos, prefixIndex);
                prefix = std::regex_replace(prefix, std::regex("XXX"), prefixIndex);
            }

            snprintf(imageFileName, MAXRBUF, "%s/%s_%s%s", UploadSettingsT[0].text, prefix.c_str(), Blobs[x].name, Blobs[x].format);

            fp = fopen(imageFileName, "w");
            if (fp == nullptr)
            {
                LOGF_ERROR("Unable to save image file (%s). %s", imageFileName, strerror(errno));
                return;
            }

            size_t n = 0;
            for (int nr = 0; nr < Blobs[x].bloblen; nr += n)
                n = fwrite((static_cast<char *>(Blobs[x].blob) + nr), 1, static_cast<unsigned int>(Blobs[x].bloblen - nr), fp);

            fclose(fp);

            // Save image file path
            IUSaveText(&FileNameT[0], imageFileName);

            LOGF_INFO("Image saved to %s", imageFileName);
            FileNameTP.s = IPS_OK;
            IDSetText(&FileNameTP, nullptr);
        }

        snprintf(Blobs[x].format, MAXINDIBLOBFMT, ".%s", PrimaryCCD.getImageExtension());
    }
    BlobP.s   = IPS_OK;

    if (sendImage)
    {
#ifdef HAVE_WEBSOCKET
        if (HasWebSocket() && WebSocketS[WEBSOCKET_ENABLED].s == ISS_ON)
        {
            for(int x = 0; x < len; x++) {
                auto start = std::chrono::high_resolution_clock::now();

                // Send format/size/..etc first later
                wsServer.send_text(std::string(Blobs[x].format));
                wsServer.send_binary(Blobs[x].blob, Blobs[x].bloblen);

                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = end - start;
                LOGF_DEBUG("Websocket transfer took %g seconds", diff.count());
            }
        }
        else
#endif
        {
            auto start = std::chrono::high_resolution_clock::now();
            IDSetBLOB(&BlobP, nullptr);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            LOGF_DEBUG("BLOB transfer took %g seconds", diff.count());
        }
    }

    LOG_INFO( "Upload complete");
}

void AHP_XC::Callback()
{
    ahp_xc_packet* packet = ahp_xc_alloc_packet();

    EnableCapture(true);
    threadsRunning = true;
    while (threadsRunning)
    {
        int ntries = 10;
        while(ahp_xc_get_packet(packet) && ntries-- > 0)
            usleep(ahp_xc_get_packettime());

        if(ntries <= 0) {
            threadsRunning = false;
            break;
        }

        double julian = ln_get_julian_from_sys();
        ln_hrz_posn altaz;
        ln_equ_posn radec;
        ln_lnlat_posn obs;

        int idx = 0;
        double minalt = 90.0;
        int farest = 0;

        for(int x = 0; x < ahp_xc_get_nlines(); x++) {
            if(lineEnableSP[x].sp[0].s == ISS_ON) {
                double lst = ln_get_apparent_sidereal_time(julian)-(360.0-lineGPSNP[x].np[1].value)/15.0;
                lst = range24(lst);
                ln_get_hrz_from_equ_sidereal_time(&radec, &obs, lst, &altaz);
                alt[x] = altaz.alt;
                az[x] = altaz.az;
                double el =
                        estimate_geocentric_elevation(lineGPSNP[x].np[0].value, 0) /
                        estimate_geocentric_elevation(lineGPSNP[x].np[0].value, lineGPSNP[x].np[2].value);
                alt[x] -= 180.0*acos(el)/M_PI;
                farest = (minalt < alt[x] ? farest : x);
                minalt = (minalt < alt[x] ? minalt : alt[x]);
            }
        }
        if(InExposure) {
            timeleft = CalcTimeLeft(ExpStart, ExposureRequest);
            if(timeleft <= 0.0f) {
                // We're no longer exposing...
                AbortExposure();
                // We're done exposing
                LOG_INFO("Integration complete, downloading plots...");
                // Additional BLOBs
                char **blobs = static_cast<char**>(malloc(sizeof(char*)*static_cast<unsigned int>(plotBP.nbp)+1));
                for(int x = 0; x < nplots; x++) {
                    size_t memsize = static_cast<unsigned int>(plot_str[x]->len)*sizeof(double);
                    blobs[x] = static_cast<char*>(malloc(memsize));
                    void* fits = dsp_file_write_fits(-64, &memsize, plot_str[x]);
                    if(fits != nullptr) {
                        blobs[x] = (char*)realloc(blobs[x], memsize);
                        memcpy(blobs[x], fits, memsize);
                        free(fits);
                    }
                    plotB[x].blob = blobs[x];
                    plotB[x].bloblen = static_cast<int>(memsize);
                }
                LOG_INFO("Plots BLOBs generated, downloading...");
                sendFile(plotB, plotBP, nplots);
                for(int x = 0; x < nplots; x++) {
                    free(blobs[x]);
                    memset(plot_str[x]->buf, 0, sizeof(dsp_t)*static_cast<size_t>(plot_str[x]->len));
                }
                LOG_INFO("Generating additional BLOBs...");
                if(ahp_xc_get_nlines() > 0 && ahp_xc_get_autocorrelator_jittersize() > 1) {
                    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
                        size_t memsize = static_cast<unsigned int>(autocorrelations_str[x]->len)*sizeof(double);
                        blobs[x] = static_cast<char*>(malloc(memsize));
                        void* fits = dsp_file_write_fits(-64, &memsize, autocorrelations_str[x]);
                        if(fits != nullptr) {
                            blobs[x] = (char*)realloc(blobs[x], memsize);
                            memcpy(blobs[x], fits, memsize);
                            free(fits);
                        }
                        autocorrelationsB[x].blob = blobs[x];
                        autocorrelationsB[x].bloblen = static_cast<int>(memsize);
                        autocorrelations_str[x]->sizes[1] = 1;
                        autocorrelations_str[x]->len = autocorrelations_str[x]->sizes[0];
                        dsp_stream_alloc_buffer(autocorrelations_str[x], autocorrelations_str[x]->len);
                    }
                    LOG_INFO("Autocorrelations BLOBs generated, downloading...");
                    sendFile(autocorrelationsB, autocorrelationsBP, ahp_xc_get_nlines());
                    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
                        free(blobs[x]);
                    }
                }
                if(ahp_xc_get_nbaselines() > 0 && ahp_xc_get_crosscorrelator_jittersize() > 1) {
                    int idx = 0;
                    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
                        for(int y = x+1; y < ahp_xc_get_nlines(); y++) {
                            size_t memsize = static_cast<unsigned int>(crosscorrelations_str[idx]->len)*sizeof(double);
                            blobs[x] = static_cast<char*>(malloc(memsize));
                            void* fits = dsp_file_write_fits(-64, &memsize, crosscorrelations_str[idx]);
                            if(fits != nullptr) {
                                blobs[x] = (char*)realloc(blobs[x], memsize);
                                memcpy(blobs[x], fits, memsize);
                                free(fits);
                            }
                            autocorrelationsB[x].blob = blobs[x];
                            autocorrelationsB[x].bloblen = static_cast<int>(memsize);
                            crosscorrelations_str[idx]->sizes[1] = 1;
                            crosscorrelations_str[idx]->len = crosscorrelations_str[idx]->sizes[0];
                            dsp_stream_alloc_buffer(crosscorrelations_str[idx], crosscorrelations_str[idx]->len);
                            idx++;
                        }
                    }
                    LOG_INFO("Crosscorrelations BLOBs generated, downloading...");
                    sendFile(crosscorrelationsB, crosscorrelationsBP, ahp_xc_get_nbaselines());
                    for(int x = 0; x < ahp_xc_get_nbaselines(); x++) {
                        free(blobs[x]);
                    }
                }
                free(blobs);
                LOG_INFO("Download complete.");
            } else {
                // Filling BLOBs
                if(nplots > 0) {
                    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
                        for(int y = x+1; y < ahp_xc_get_nlines(); y++) {
                            if(lineEnableSP[x].sp[0].s == ISS_ON && lineEnableSP[y].sp[0].s == ISS_ON) {
                                int w = plot_str[0]->sizes[0];
                                int h = plot_str[0]->sizes[1];
                                INDI::Correlator::UVCoordinate uv = baselines[idx]->getUVCoordinates(alt[farest], az[farest]);
                                int xx = static_cast<int>(w*uv.u/2.0);
                                int yy = static_cast<int>(h*uv.v/2.0);
                                int z = w*h/2+w/2+xx+yy*w;
                                if(xx >= -w/2 && xx < w/2 && yy >= -w/2 && yy < h/2) {
                                    plot_str[0]->buf[z] += (double)packet->crosscorrelations[idx].correlations[packet->crosscorrelations[idx].jitter_size/2].coherence;
                                    plot_str[0]->buf[w*h-1-z] += (double)packet->crosscorrelations[idx].correlations[packet->crosscorrelations[idx].jitter_size/2].coherence;
                                }
                            }
                            idx++;
                        }
                    }
                }
                if(ahp_xc_get_nlines() > 0 && ahp_xc_get_autocorrelator_jittersize() > 1) {
                    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
                        int pos = autocorrelations_str[x]->len-autocorrelations_str[x]->sizes[0];
                        autocorrelations_str[x]->sizes[1]++;
                        autocorrelations_str[x]->len += autocorrelations_str[x]->sizes[0];
                        autocorrelations_str[x]->buf = (dsp_t*)realloc(autocorrelations_str[x]->buf, sizeof(dsp_t)*static_cast<unsigned int>(autocorrelations_str[x]->len));
                        for(unsigned int i = 0; i < packet->autocorrelations[x].jitter_size; i++)
                            autocorrelations_str[x]->buf[pos++] = packet->autocorrelations[x].correlations[i].coherence;
                    }
                }
                idx = 0;
                if(ahp_xc_get_nbaselines() > 0 && ahp_xc_get_crosscorrelator_jittersize() > 1) {
                    for(int x = 0; x < ahp_xc_get_nbaselines(); x++) {
                        int pos = crosscorrelations_str[x]->len-crosscorrelations_str[x]->sizes[0];
                        crosscorrelations_str[x]->sizes[1]++;
                        crosscorrelations_str[x]->len += crosscorrelations_str[x]->sizes[0];
                        crosscorrelations_str[x]->buf = (dsp_t*)realloc(crosscorrelations_str[x]->buf, sizeof(dsp_t)*static_cast<unsigned int>(crosscorrelations_str[x]->len));
                        for(unsigned int i = 0; i < packet->crosscorrelations[x].jitter_size; i++)
                            crosscorrelations_str[x]->buf[pos++] = packet->crosscorrelations[x].correlations[i].coherence;
                    }
                }
            }
        }

        idx = 0;
        for(int x = 0; x < ahp_xc_get_nlines(); x++) {
            if(lineEnableSP[x].sp[0].s == ISS_ON)
                totalcounts[x] += packet->counts[x];
            for(int y = x+1; y < ahp_xc_get_nlines(); y++) {
                if(lineEnableSP[x].sp[0].s == ISS_ON&&lineEnableSP[y].sp[0].s == ISS_ON) {
                    totalcorrelations[idx].counts += packet->crosscorrelations[idx].correlations[packet->crosscorrelations[idx].jitter_size/2].counts;
                    totalcorrelations[idx].correlations += packet->crosscorrelations[idx].correlations[packet->crosscorrelations[idx].jitter_size/2].correlations;
                }
                idx++;
            }
        }

        delay[farest] = 0;
        idx = 0;
        for(int x = 0; x < ahp_xc_get_nlines(); x++) {
            for(int y = x+1; y < ahp_xc_get_nlines(); y++) {
                if(lineEnableSP[x].sp[0].s == ISS_ON && lineEnableSP[y].sp[0].s == ISS_ON) {
                    if(y == farest) {
                        delay[x] = baselines[idx]->getDelay(alt[farest], az[farest]);
                    }
                    if(x == farest) {
                        delay[y] = baselines[idx]->getDelay(alt[farest], az[farest]);
                    }
                }
                idx++;
            }
        }

        for(int x = 0; x < ahp_xc_get_nlines(); x++) {
            int delay_clocks = delay[x] * ahp_xc_get_frequency() / LIGHTSPEED;
            delay_clocks = (delay_clocks > 0 ? (delay_clocks < ahp_xc_get_delaysize() ? delay_clocks : ahp_xc_get_delaysize()-1) : 0);
            ahp_xc_set_line(x, 0);
            ahp_xc_set_delay(x, delay_clocks);
        }
    }
    EnableCapture(false);
    ahp_xc_free_packet(packet);
}

AHP_XC::AHP_XC()
{
    clock_divider = 0;

    ExposureRequest = 0.0;
    InExposure = false;

    autocorrelationsB = static_cast<IBLOB*>(malloc(1));
    crosscorrelationsB = static_cast<IBLOB*>(malloc(1));
    plotB = static_cast<IBLOB*>(malloc(1));

    lineStatsN = static_cast<INumber*>(malloc(1));
    lineStatsNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineEnableS = static_cast<ISwitch*>(malloc(1));
    lineEnableSP = static_cast<ISwitchVectorProperty*>(malloc(1));

    linePowerS = static_cast<ISwitch*>(malloc(1));
    linePowerSP = static_cast<ISwitchVectorProperty*>(malloc(1));

    lineDevicesT = static_cast<IText*>(malloc(1));
    lineDevicesTP = static_cast<ITextVectorProperty*>(malloc(1));

    snoopGPSN = static_cast<INumber*>(malloc(1));
    snoopGPSNP = static_cast<INumberVectorProperty*>(malloc(1));

    snoopTelescopeN = static_cast<INumber*>(malloc(1));
    snoopTelescopeNP = static_cast<INumberVectorProperty*>(malloc(1));

    snoopTelescopeInfoN = static_cast<INumber*>(malloc(1));
    snoopTelescopeInfoNP = static_cast<INumberVectorProperty*>(malloc(1));

    snoopDomeN = static_cast<INumber*>(malloc(1));
    snoopDomeNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineDelayN = static_cast<INumber*>(malloc(1));
    lineDelayNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineGPSN = static_cast<INumber*>(malloc(1));
    lineGPSNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineTelescopeN = static_cast<INumber*>(malloc(1));
    lineTelescopeNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineDomeN = static_cast<INumber*>(malloc(1));
    lineDomeNP = static_cast<INumberVectorProperty*>(malloc(1));

    correlationsN = static_cast<INumber*>(malloc(1));

    autocorrelations_str = static_cast<dsp_stream_p*>(malloc(1));
    crosscorrelations_str = static_cast<dsp_stream_p*>(malloc(1));
    plot_str = static_cast<dsp_stream_p*>(malloc(1));

    framebuffer = static_cast<double*>(malloc(1));
    totalcounts = static_cast<double*>(malloc(1));
    totalcorrelations = static_cast<ahp_xc_correlation*>(malloc(1));
    alt = static_cast<double*>(malloc(1));
    az = static_cast<double*>(malloc(1));
    delay = static_cast<double*>(malloc(1));
    baselines = static_cast<baseline**>(malloc(1));

}

bool AHP_XC::Disconnect()
{
    for(int x = 0; x < nplots; x++) {
        dsp_stream_free_buffer(plot_str[x]);
        dsp_stream_free(plot_str[x]);
    }
    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
        if(ahp_xc_get_autocorrelator_jittersize() > 1) {
            dsp_stream_free_buffer(autocorrelations_str[x]);
            dsp_stream_free(autocorrelations_str[x]);
        }
        ActiveLine(x, false, false);
        usleep(10000);
    }
    for(int x = 0; x < ahp_xc_get_nbaselines(); x++) {
        if(ahp_xc_get_crosscorrelator_jittersize() > 1) {
            dsp_stream_free_buffer(crosscorrelations_str[x]);
            dsp_stream_free(crosscorrelations_str[x]);
        }
    }

    threadsRunning = false;

    readThread->join();
    readThread->~thread();

    ahp_xc_disconnect();

    return true;
}

const char * AHP_XC::getDefaultName()
{
    return "AHP XC Correlator";
}

const char * AHP_XC::getDeviceName()
{
    return getDefaultName();
}

bool AHP_XC::saveConfigItems(FILE *fp)
{
    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
        IUSaveConfigSwitch(fp, &lineEnableSP[x]);
        if(lineEnableSP[x].sp[0].s == ISS_ON) {
            IUSaveConfigText(fp, &lineDevicesTP[x]);
            IUSaveConfigSwitch(fp, &linePowerSP[x]);
        }
    }
    IUSaveConfigNumber(fp, &settingsNP);

    INDI::CCD::saveConfigItems(fp);
    return true;
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool AHP_XC::initProperties()
{

    // Must init parent properties first!
    INDI::CCD::initProperties();

    SetCCDCapability(CCD_CAN_ABORT|CCD_HAS_DSP);

    IUFillNumber(&settingsN[0], "INTERFEROMETER_WAVELENGTH_VALUE", "Filter wavelength (m)", "%g", 3.0E-12, 3.0E+3, 1.0E-9, 0.211121449);
    IUFillNumber(&settingsN[1], "INTERFEROMETER_BANDWIDTH_VALUE", "Filter bandwidth (m)", "%g", 3.0E-12, 3.0E+3, 1.0E-9, 1199.169832);
    IUFillNumberVector(&settingsNP, settingsN, 2, getDeviceName(), "INTERFEROMETER_SETTINGS", "AHP_XC Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 1.0, STELLAR_DAY, 1, false);
    setDefaultPollingPeriod(500);

    serialConnection = new Connection::Serial(this);
    serialConnection->setStopBits(2);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_57600);
    registerConnection(serialConnection);

    return true;
}

/**************************************************************************************
** INDI is asking us to submit list of properties for the device
***************************************************************************************/
void AHP_XC::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    if (isConnected())
    {
        for (int x=0; x<ahp_xc_get_nlines(); x++) {
            defineSwitch(&lineEnableSP[x]);
        }
        if(ahp_xc_get_autocorrelator_jittersize() > 1)
            defineBLOB(&autocorrelationsBP);
        if(ahp_xc_get_crosscorrelator_jittersize() > 1)
            defineBLOB(&crosscorrelationsBP);
        defineNumber(&correlationsNP);
        defineNumber(&settingsNP);

        // Define our properties
    }
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool AHP_XC::updateProperties()
{
    // Call parent update properties
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        // Let's get parameters now from CCD
        setupParams();

        for (int x=0; x<ahp_xc_get_nlines(); x++) {
            defineSwitch(&lineEnableSP[x]);
        }
        if(ahp_xc_get_autocorrelator_jittersize() > 1)
            defineBLOB(&autocorrelationsBP);
        if(ahp_xc_get_crosscorrelator_jittersize() > 1)
            defineBLOB(&crosscorrelationsBP);
        defineNumber(&correlationsNP);
        defineNumber(&settingsNP);
    }
    else
        // We're disconnected
    {
        if(ahp_xc_get_autocorrelator_jittersize() > 1)
            deleteProperty(autocorrelationsBP.name);
        if(ahp_xc_get_crosscorrelator_jittersize() > 1)
            deleteProperty(crosscorrelationsBP.name);
        deleteProperty(correlationsNP.name);
        deleteProperty(settingsNP.name);
        for (int x=0; x<ahp_xc_get_nlines(); x++) {
            deleteProperty(lineEnableSP[x].name);
            deleteProperty(linePowerSP[x].name);
            deleteProperty(lineGPSNP[x].name);
            deleteProperty(lineTelescopeNP[x].name);
            deleteProperty(lineStatsNP[x].name);
            deleteProperty(lineDevicesTP[x].name);
            deleteProperty(lineDelayNP[x].name);
        }
    }

    for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
        baselines[x]->updateProperties();

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void AHP_XC::setupParams()
{
    float pixelsize = (float)AIRY * (float)LIGHTSPEED / ahp_xc_get_frequency();
    int size = (float)ahp_xc_get_delaysize()*2.0f*pixelsize;
    pixelsize *= 1000000.0f;
    SetCCDParams(size, size, 64, pixelsize, pixelsize);

    if(nplots > 0) {
        plot_str[0]->sizes[0] = size;
        plot_str[0]->sizes[1] = size;
        plot_str[0]->len = size*size;
        dsp_stream_alloc_buffer(plot_str[0], plot_str[0]->len);
    }
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool AHP_XC::StartExposure(float duration)
{
    if(InExposure)
        return false;

    gettimeofday(&ExpStart, nullptr);
    ExposureRequest = duration;
    PrimaryCCD.setExposureDuration(static_cast<double>(ExposureRequest));
    InExposure = true;
    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool AHP_XC::AbortExposure()
{
    InExposure = false;
    return true;
}

/**************************************************************************************
** Client is asking us to set a new number
***************************************************************************************/
bool AHP_XC::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    INDI::CCD::ISNewNumber(dev, name, values, names, n);

    for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
        baselines[x]->ISNewNumber(dev, name, values, names, n);

    if(!strcmp(settingsNP.name, name)) {
        IUUpdateNumber(&settingsNP, values, names, n);
        for(int x = 0; x < ahp_xc_get_nbaselines(); x++) {
            baselines[x]->setWavelength(settingsN[0].value);
        }
        IDSetNumber(&settingsNP, nullptr);
        return true;
    }

    return true;
}

/**************************************************************************************
** Client is asking us to set a new switch
***************************************************************************************/
bool AHP_XC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    if(!strcmp(name, "DEVICE_BAUD_RATE")) {
        if(isConnected()) {
            if(states[0] == ISS_ON || states[1] == ISS_ON || states[2] == ISS_ON) {
                states[0] = states[1] = states[2] = ISS_OFF;
                states[3] = ISS_ON;
            }
            IUUpdateSwitch(getSwitch("DEVICE_BAUD_RATE"), states, names, n);
            if (states[3] == ISS_ON) {
                ahp_xc_set_baudrate(R_57600);
            }
            if (states[4] == ISS_ON) {
                ahp_xc_set_baudrate(R_115200);
            }
            if (states[5] == ISS_ON) {
                ahp_xc_set_baudrate(R_230400);
            }
            IDSetSwitch(getSwitch("DEVICE_BAUD_RATE"), nullptr);
        }
    }

    for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
        baselines[x]->ISNewSwitch(dev, name, states, names, n);

    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
        if(!strcmp(name, lineEnableSP[x].name)){
            IUUpdateSwitch(&lineEnableSP[x], states, names, n);
            if(lineEnableSP[x].sp[0].s == ISS_ON) {
                ActiveLine(x, true, linePowerSP[x].sp[0].s == ISS_ON);
                defineSwitch(&linePowerSP[x]);
                defineNumber(&lineGPSNP[x]);
                defineNumber(&lineTelescopeNP[x]);
                defineNumber(&lineDelayNP[x]);
                defineNumber(&lineStatsNP[x]);
                defineText(&lineDevicesTP[x]);
            } else {
                ActiveLine(x, false, false);
                deleteProperty(linePowerSP[x].name);
                deleteProperty(lineGPSNP[x].name);
                deleteProperty(lineTelescopeNP[x].name);
                deleteProperty(lineStatsNP[x].name);
                deleteProperty(lineDevicesTP[x].name);
                deleteProperty(lineDelayNP[x].name);
            }
            IDSetSwitch(&lineEnableSP[x], nullptr);
        }
        if(!strcmp(name, linePowerSP[x].name)){
            IUUpdateSwitch(&linePowerSP[x], states, names, n);
            ActiveLine(x, true, linePowerSP[x].sp[0].s == ISS_ON);
            IDSetSwitch(&linePowerSP[x], nullptr);
        }
    }
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Client is asking us to set a new BLOB
***************************************************************************************/
bool AHP_XC::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
        baselines[x]->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);

    return INDI::CCD::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Client is asking us to set a new text
***************************************************************************************/
bool AHP_XC::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    //  This is for our device
    //  Now lets see if it's something we process here
    for(int x = 0; x < ahp_xc_get_nlines(); x++) {
        if (!strcmp(name, lineDevicesTP[x].name))
        {
            lineDevicesTP[x].s = IPS_OK;
            IUUpdateText(&lineDevicesTP[x], texts, names, n);
            IDSetText(&lineDevicesTP[x], nullptr);

            // Update the property name!
            strncpy(snoopTelescopeNP[x].device, lineDevicesTP[x].tp[0].text, MAXINDIDEVICE);
            strncpy(snoopTelescopeInfoNP[x].device, lineDevicesTP[x].tp[0].text, MAXINDIDEVICE);
            strncpy(snoopGPSNP[x].device, lineDevicesTP[x].tp[1].text, MAXINDIDEVICE);
            strncpy(snoopDomeNP[x].device, lineDevicesTP[x].tp[2].text, MAXINDIDEVICE);

            IDSnoopDevice(lineDevicesTP[x].tp[0].text, "EQUATORIAL_EOD_COORD");
            IDSnoopDevice(lineDevicesTP[x].tp[0].text, "TELESCOPE_INFO");
            IDSnoopDevice(snoopGPSNP[x].device, "GEOGRAPHIC_COORD");
            IDSnoopDevice(snoopDomeNP[x].device, "GEOGRAPHIC_COORD");

            //  We processed this one, so, tell the world we did it
            return true;
        }
    }

    for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
        baselines[x]->ISNewText(dev, name, texts, names, n);

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Client is asking us to set a new snoop device
***************************************************************************************/
bool AHP_XC::ISSnoopDevice(XMLEle *root)
{
    for(int i = 0; i < ahp_xc_get_nlines(); i++) {
        if(!IUSnoopNumber(root, &snoopTelescopeNP[i])) {
            lineTelescopeNP[i].s = IPS_BUSY;
            lineTelescopeNP[i].np[0].value = snoopTelescopeNP[i].np[0].value;
            lineTelescopeNP[i].np[1].value = snoopTelescopeNP[i].np[1].value;
            IDSetNumber(&lineTelescopeNP[i], nullptr);
        }
        if(!IUSnoopNumber(root, &snoopTelescopeInfoNP[i])) {
            lineTelescopeNP[i].s = IPS_BUSY;
            lineTelescopeNP[i].np[2].value = snoopTelescopeInfoNP[i].np[0].value;
            lineTelescopeNP[i].np[3].value = snoopTelescopeInfoNP[i].np[1].value;
            IDSetNumber(&lineTelescopeNP[i], nullptr);
        }
        if(!IUSnoopNumber(root, &snoopGPSNP[i])) {
            lineGPSNP[i].s = IPS_BUSY;
            double Lat0, Lon0;
            lineGPSNP[i].np[0].value = snoopGPSNP[i].np[0].value;
            lineGPSNP[i].np[1].value = snoopGPSNP[i].np[1].value;
            lineGPSNP[i].np[2].value = snoopGPSNP[i].np[2].value;
            int idx = 0;
            for(int x = 0; x < ahp_xc_get_nlines(); x++) {
                for(int y = x+1; y < ahp_xc_get_nlines(); y++) {
                    if(x==i||y==i) {
                        double Lat, Lon;
                        Lat0 = (snoopGPSNP[y].np[0].value-snoopGPSNP[x].np[0].value);
                        Lon0 = (snoopGPSNP[y].np[1].value-snoopGPSNP[x].np[1].value);
                        Lon = rangeDec(Lon0);
                        Lon0 = 0;
                        Lon *= M_PI/180.0;
                        Lat = rangeDec(Lat0);
                        Lat0 = 0;
                        Lat *= M_PI/180.0;
                        double radius = estimate_geocentric_elevation(snoopGPSNP[x].np[0].value, snoopGPSNP[x].np[2].value);
                        INDI::Correlator::Baseline b;
                        b.x = sin(Lon)*radius;
                        b.y = sin(Lat)*radius;
                        b.z = (1.0-cos(Lat)*cos(Lon))*radius;
                        baselines[idx]->setBaseline(b);
                    }
                    idx++;
                }
            }
            IDSetNumber(&lineGPSNP[i], nullptr);
        }
    }

    for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
        baselines[x]->ISSnoopDevice(root);

    return INDI::CCD::ISSnoopDevice(root);
}

/**************************************************************************************
** INDI is asking us to add any FITS keywords to the FITS header
***************************************************************************************/
void AHP_XC::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    // Let's first add parent keywords
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    // Add temperature to FITS header
    int status = 0;
    fits_write_date(fptr, &status);

}

float AHP_XC::CalcTimeLeft(timeval start, float req)
{
    float timesince;
    float timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (float)(now.tv_sec * 1000.0f + now.tv_usec / 1000.0f) - (float)(start.tv_sec * 1000.0f + start.tv_usec / 1000.0f);
    timesince = timesince / 1000.0f;
    timeleft  = req - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void AHP_XC::TimerHit()
{
    if(!isConnected())
        return;  //  No need to reset timer if we are not connected anymore

    int idx = 0;
    correlationsNP.s = IPS_BUSY;
    for (int x = 0; x < ahp_xc_get_nlines(); x++) {
        double line_delay = delay[x];
        double steradian = pow(asin(lineTelescopeNP[x].np[2].value*0.5/lineTelescopeNP[x].np[3].value), 2);
        double photon_flux = ((double)totalcounts[x])*1000.0/POLLMS;
        double photon_flux0 = calc_photon_flux(0, settingsNP.np[1].value, settingsNP.np[0].value, steradian);
        lineDelayNP[x].s = IPS_BUSY;
        lineDelayNP[x].np[0].value = line_delay;
        IDSetNumber(&lineDelayNP[x], nullptr);
        lineStatsNP[x].s = IPS_BUSY;
        lineStatsNP[x].np[0].value = ((double)totalcounts[x])*1000.0/(double)POLLMS;
        lineStatsNP[x].np[1].value = photon_flux/LUMEN(settingsNP.np[0].value);
        lineStatsNP[x].np[2].value = photon_flux0/LUMEN(settingsNP.np[0].value);
        lineStatsNP[x].np[3].value = calc_rel_magnitude(photon_flux, settingsNP.np[1].value, settingsNP.np[0].value, steradian);
        IDSetNumber(&lineStatsNP[x], nullptr);
        totalcounts[x] = 0;
        for(int y = x+1; y < ahp_xc_get_nlines(); y++) {
            correlationsNP.np[idx*2].value = (double)totalcorrelations[idx].correlations*1000.0/(double)POLLMS;
            correlationsNP.np[idx*2+1].value = (double)totalcorrelations[idx].correlations/(double)totalcorrelations[idx].counts;
            totalcorrelations[idx].counts = 0;
            totalcorrelations[idx].correlations = 0;
            totalcorrelations[idx].counts = 0;
            idx++;
        }
    }
    IDSetNumber(&correlationsNP, nullptr);

    if(InExposure) {
        // Just update time left in client
        PrimaryCCD.setExposureLeft((double)timeleft);
    }

    SetTimer(POLLMS);

    return;
}

bool AHP_XC::Connect()
{
    ahp_xc_connect(serialConnection->port());

    if(0 != ahp_xc_get_properties()) {
        ahp_xc_disconnect();
        return false;
    }

    lineStatsN = static_cast<INumber*>(realloc(lineStatsN, static_cast<unsigned long>(4*ahp_xc_get_nlines())*sizeof(INumber)+1));
    lineStatsNP = static_cast<INumberVectorProperty*>(realloc(lineStatsNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    lineEnableS = static_cast<ISwitch*>(realloc(lineEnableS, static_cast<unsigned long>(ahp_xc_get_nlines())*2*sizeof(ISwitch)));
    lineEnableSP = static_cast<ISwitchVectorProperty*>(realloc(lineEnableSP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(ISwitchVectorProperty)+1));

    linePowerS = static_cast<ISwitch*>(realloc(linePowerS, static_cast<unsigned long>(ahp_xc_get_nlines())*2*sizeof(ISwitch)+1));
    linePowerSP = static_cast<ISwitchVectorProperty*>(realloc(linePowerSP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(ISwitchVectorProperty)+1));

    lineDevicesT = static_cast<IText*>(realloc(lineDevicesT, static_cast<unsigned long>(3*ahp_xc_get_nlines())*sizeof(IText)+1));
    lineDevicesTP = static_cast<ITextVectorProperty*>(realloc(lineDevicesTP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(ITextVectorProperty)+1));

    lineGPSN = static_cast<INumber*>(realloc(lineGPSN, static_cast<unsigned long>(3*ahp_xc_get_nlines())*sizeof(INumber)+1));
    lineGPSNP = static_cast<INumberVectorProperty*>(realloc(lineGPSNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    lineTelescopeN = static_cast<INumber*>(realloc(lineTelescopeN, static_cast<unsigned long>(4*ahp_xc_get_nlines())*sizeof(INumber)+1));
    lineTelescopeNP = static_cast<INumberVectorProperty*>(realloc(lineTelescopeNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    lineDomeN = static_cast<INumber*>(realloc(lineDomeN, static_cast<unsigned long>(2*ahp_xc_get_nlines())*sizeof(INumber)+1));
    lineDomeNP = static_cast<INumberVectorProperty*>(realloc(lineDomeNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    snoopGPSN = static_cast<INumber*>(realloc(snoopGPSN, static_cast<unsigned long>(3*ahp_xc_get_nlines())*sizeof(INumber)+1));
    snoopGPSNP = static_cast<INumberVectorProperty*>(realloc(snoopGPSNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    snoopTelescopeN = static_cast<INumber*>(realloc(snoopTelescopeN, static_cast<unsigned long>(2*ahp_xc_get_nlines())*sizeof(INumber)+1));
    snoopTelescopeNP = static_cast<INumberVectorProperty*>(realloc(snoopTelescopeNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    snoopTelescopeInfoN = static_cast<INumber*>(realloc(snoopTelescopeInfoN, static_cast<unsigned long>(4*ahp_xc_get_nlines())*sizeof(INumber)+1));
    snoopTelescopeInfoNP = static_cast<INumberVectorProperty*>(realloc(snoopTelescopeInfoNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    snoopDomeN = static_cast<INumber*>(realloc(snoopDomeN, static_cast<unsigned long>(2*ahp_xc_get_nlines())*sizeof(INumber)+1));
    snoopDomeNP = static_cast<INumberVectorProperty*>(realloc(snoopDomeNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    lineDelayN = static_cast<INumber*>(realloc(lineDelayN, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumber)+1));
    lineDelayNP = static_cast<INumberVectorProperty*>(realloc(lineDelayNP, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(INumberVectorProperty)+1));

    correlationsN = static_cast<INumber*>(realloc(correlationsN, static_cast<unsigned long>(ahp_xc_get_nbaselines()*2)*sizeof(INumber)+1));

    if(ahp_xc_get_autocorrelator_jittersize() > 1)
        autocorrelationsB = static_cast<IBLOB*>(realloc(autocorrelationsB, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(IBLOB)+1));
    if(ahp_xc_get_crosscorrelator_jittersize() > 1)
        crosscorrelationsB = static_cast<IBLOB*>(realloc(crosscorrelationsB, static_cast<unsigned long>(ahp_xc_get_nbaselines())*sizeof(IBLOB)+1));
    if(nplots > 0)
        plotB = static_cast<IBLOB*>(realloc(plotB, static_cast<unsigned long>(nplots)*sizeof(IBLOB)+1));

    if(ahp_xc_get_autocorrelator_jittersize() > 1)
        autocorrelations_str = static_cast<dsp_stream_p*>(realloc(autocorrelations_str, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(dsp_stream_p)+1));
    if(ahp_xc_get_crosscorrelator_jittersize() > 1)
        crosscorrelations_str = static_cast<dsp_stream_p*>(realloc(crosscorrelations_str, static_cast<unsigned long>(ahp_xc_get_nbaselines())*sizeof(dsp_stream_p)+1));
    if(nplots > 0)
        plot_str = static_cast<dsp_stream_p*>(realloc(plot_str, static_cast<unsigned long>(nplots)*sizeof(dsp_stream_p)+1));

    totalcounts = static_cast<double*>(realloc(totalcounts, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1));
    totalcorrelations = static_cast<ahp_xc_correlation*>(realloc(totalcorrelations, static_cast<unsigned long>(ahp_xc_get_nbaselines())*sizeof(ahp_xc_correlation)+1));
    alt = static_cast<double*>(realloc(alt, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1));
    az = static_cast<double*>(realloc(az, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1));
    delay = static_cast<double*>(realloc(delay, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1));
    baselines = static_cast<baseline**>(realloc(baselines, static_cast<unsigned long>(ahp_xc_get_nbaselines())*sizeof(baseline*)+1));

    memset (totalcounts, 0, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1);
    memset (totalcorrelations, 0, static_cast<unsigned long>(ahp_xc_get_nbaselines())*sizeof(ahp_xc_correlation)+1);
    memset (alt, 0, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1);
    memset (az, 0, static_cast<unsigned long>(ahp_xc_get_nlines())*sizeof(double)+1);
    for(int x = 0; x < ahp_xc_get_nbaselines(); x++) {
        if(ahp_xc_get_crosscorrelator_jittersize() > 1) {
            crosscorrelations_str[x] = dsp_stream_new();
            dsp_stream_add_dim(crosscorrelations_str[x], ahp_xc_get_crosscorrelator_jittersize()*2-1);
            dsp_stream_add_dim(crosscorrelations_str[x], 1);
            dsp_stream_alloc_buffer(crosscorrelations_str[x], crosscorrelations_str[x]->len);
        }
        baselines[x] = new baseline();
        baselines[x]->initProperties();
    }

    int idx = 0;
    char tab[MAXINDINAME];
    char name[MAXINDINAME];
    char label[MAXINDINAME];

    for(int x = 0; x < nplots; x++) {
        plot_str[x] = dsp_stream_new();
        dsp_stream_add_dim(plot_str[x], 1);
        dsp_stream_add_dim(plot_str[x], 1);
        dsp_stream_alloc_buffer(plot_str[x], plot_str[x]->len);
        sprintf(name, "PLOT%02d", x+1);
        sprintf(label, "Plot %d", x+1);
        IUFillBLOB(&plotB[x], name, label, ".fits");
    }
    IUFillBLOBVector(&plotBP, plotB, nplots, getDeviceName(), "PLOTS", "Plots", "Stats", IP_RO, 60, IPS_BUSY);

    for (int x = 0; x < ahp_xc_get_nlines(); x++) {
        if(ahp_xc_get_autocorrelator_jittersize() > 1) {
            autocorrelations_str[x] = dsp_stream_new();
            dsp_stream_add_dim(autocorrelations_str[x], ahp_xc_get_autocorrelator_jittersize());
            dsp_stream_add_dim(autocorrelations_str[x], 1);
            dsp_stream_alloc_buffer(autocorrelations_str[x], autocorrelations_str[x]->len);
        }

        //snoop properties
        IUFillNumber(&snoopTelescopeN[x*2+0], "RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&snoopTelescopeN[x*2+1], "DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);

        IUFillNumber(&snoopTelescopeInfoN[x*4+0], "TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0.0);
        IUFillNumber(&snoopTelescopeInfoN[x*4+1], "TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0.0);
        IUFillNumber(&snoopTelescopeInfoN[x*4+2], "GUIDER_APERTURE", "Guider Aperture (mm)", "%g", 10, 5000, 0, 0.0);
        IUFillNumber(&snoopTelescopeInfoN[x*4+3], "GUIDER_FOCAL_LENGTH", "Guider Focal Length (mm)", "%g", 10, 10000, 0, 0.0);

        IUFillNumber(&snoopGPSN[x*3+0], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
        IUFillNumber(&snoopGPSN[x*3+1], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
        IUFillNumber(&snoopGPSN[x*3+2], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);

        IUFillNumber(&lineDelayN[x], "DELAY", "Delay (m)", "%g", 0, EARTHRADIUSMEAN, 1.0E-9, 0);

        IUFillNumberVector(&snoopGPSNP[x], &snoopGPSN[x*3], 3, getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
        IUFillNumberVector(&snoopTelescopeNP[x], &snoopTelescopeN[x*2], 2, getDeviceName(), "EQUATORIAL_EOD_COORD", "Target coordinates", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
        IUFillNumberVector(&snoopTelescopeInfoNP[x], &snoopTelescopeInfoN[x*4], 4, getDeviceName(), "TELESCOPE_INFO", "Scope Properties", OPTIONS_TAB, IP_RW, 60, IPS_OK);

        lineDevicesT[x*3+0].text = static_cast<char*>(malloc(1));
        IUFillText(&lineDevicesT[x*3+0], "ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
        lineDevicesT[x*3+1].text = static_cast<char*>(malloc(1));
        IUFillText(&lineDevicesT[x*3+1], "ACTIVE_GPS", "GPS", "GPS Simulator");
        lineDevicesT[x*3+2].text = static_cast<char*>(malloc(1));
        IUFillText(&lineDevicesT[x*3+2], "ACTIVE_DOME", "DOME", "Dome Simulator");

        //interferometer properties
        IUFillNumber(&lineTelescopeN[x*4+0], "RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&lineTelescopeN[x*4+1], "DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
        IUFillNumber(&lineTelescopeN[x*4+2], "TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0.0);
        IUFillNumber(&lineTelescopeN[x*4+3], "TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0.0);

        IUFillNumber(&lineGPSN[x*3+0], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
        IUFillNumber(&lineGPSN[x*3+1], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
        IUFillNumber(&lineGPSN[x*3+2], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);

        IUFillSwitch(&lineEnableS[x*2+0], "LINE_ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&lineEnableS[x*2+1], "LINE_DISABLE", "Disable", ISS_ON);

        IUFillSwitch(&linePowerS[x*2+0], "LINE_POWER_ON", "On", ISS_OFF);
        IUFillSwitch(&linePowerS[x*2+1], "LINE_POWER_OFF", "Off", ISS_ON);

        //report pulse counts
        IUFillNumber(&lineStatsN[x*4+0], "LINE_COUNTS", "Counts", "%g", 0.0, 400000000.0, 1.0, 0);
        IUFillNumber(&lineStatsN[x*4+1], "LINE_FLUX", "Photon Flux (Lm)", "%g", 0.0, 1.0, 1.0E-5, 0);
        IUFillNumber(&lineStatsN[x*4+2], "LINE_FLUX0", "Flux at mag0 (Lm)", "%g", 0.0, 1.0, 1.0E-5, 0);
        IUFillNumber(&lineStatsN[x*4+3], "LINE_MAGNITUDE", "Estimated magnitude", "%g", -22.0, 22.0, 1.0E-5, 0);

        sprintf(tab, "Line %02d", x+1);
        sprintf(name, "LINE_ENABLE_%02d", x+1);
        IUFillSwitchVector(&lineEnableSP[x], &lineEnableS[x*2], 2, getDeviceName(), name, "Enable Line", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
        sprintf(name, "LINE_POWER_%02d", x+1);
        IUFillSwitchVector(&linePowerSP[x], &linePowerS[x*2], 2, getDeviceName(), name, "Power", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
        sprintf(name, "LINE_SNOOP_DEVICES_%02d", x+1);
        IUFillTextVector(&lineDevicesTP[x], &lineDevicesT[x*3], 3, getDeviceName(), name, "Locator devices", tab, IP_RW, 60, IPS_IDLE);
        sprintf(name, "LINE_GEOGRAPHIC_COORD_%02d", x+1);
        IUFillNumberVector(&lineGPSNP[x], &lineGPSN[x*3], 3, getDeviceName(), name, "Location", tab, IP_RO, 60, IPS_IDLE);
        sprintf(name, "TELESCOPE_INFO_%02d", x+1);
        IUFillNumberVector(&lineTelescopeNP[x], &lineTelescopeN[x*4], 4, getDeviceName(), name, "Target coordinates", tab, IP_RO, 60, IPS_IDLE);
        sprintf(name, "LINE_DELAY_%02d", x+1);
        IUFillNumberVector(&lineDelayNP[x], &lineDelayN[x], 1, getDeviceName(), name, "Delay line", tab, IP_RO, 60, IPS_IDLE);
        sprintf(name, "LINE_STATS_%02d", x+1);
        IUFillNumberVector(&lineStatsNP[x], &lineStatsN[x*4], 4, getDeviceName(), name, "Stats", tab, IP_RO, 60, IPS_BUSY);

        if(ahp_xc_get_crosscorrelator_jittersize() > 1) {
            sprintf(name, "AUTOCORRELATIONS_%02d", x+1);
            sprintf(label, "Autocorrelations %d", x+1);
            IUFillBLOB(&autocorrelationsB[x], name, label, ".fits");
        }

        for (int y = x+1; y < ahp_xc_get_nlines(); y++) {
            if(ahp_xc_get_crosscorrelator_jittersize() > 1) {
                sprintf(name, "CROSSCORRELATIONS_%02d_%02d", x+1, y+1);
                sprintf(label, "Crosscorrelations %d*%d", x+1, y+1);
                IUFillBLOB(&crosscorrelationsB[idx], name, label, ".fits");
            }
            sprintf(name, "CORRELATIONS_%0d_%0d", x+1, y+1);
            sprintf(label, "Correlations (%d*%d)", x+1, y+1);
            IUFillNumber(&correlationsN[idx*2], name, label, "%1.4f", 0, 1.0, 1, 0);
            sprintf(name, "COHERENCE_%0d_%0d", x+1, y+1);
            sprintf(label, "Coherence ratio (%d*%d)", x+1, y+1);
            IUFillNumber(&correlationsN[idx*2+1], name, label, "%01.04f", 0, 1.0, 0.0001, 0);
            idx++;
        }
    }
    if(ahp_xc_get_autocorrelator_jittersize() > 1)
        IUFillBLOBVector(&autocorrelationsBP, autocorrelationsB, ahp_xc_get_nlines(), getDeviceName(), "AUTOCORRELATIONS", "Autocorrelations", "Stats", IP_RO, 60, IPS_BUSY);
    if(ahp_xc_get_crosscorrelator_jittersize() > 1)
        IUFillBLOBVector(&crosscorrelationsBP, crosscorrelationsB, ahp_xc_get_nbaselines(), getDeviceName(), "CROSSCORRELATIONS", "Crosscorrelations", "Stats", IP_RO, 60, IPS_BUSY);
    IUFillNumberVector(&correlationsNP, correlationsN, ahp_xc_get_nbaselines()*2, getDeviceName(), "CORRELATIONS", "Correlations", "Stats", IP_RO, 60, IPS_BUSY);

    // Start the timer
    SetTimer(POLLMS);

    readThread = new std::thread(&AHP_XC::Callback, this);

    return true;
}

void AHP_XC::ActiveLine(int line, bool on, bool power)
{
    ahp_xc_set_leds(line, (on?1:0)|(power?2:0));
}

void AHP_XC::SetFrequencyDivider(unsigned char divider)
{
    ahp_xc_set_frequency_divider(divider);
}

void AHP_XC::EnableCapture(bool start)
{
    ahp_xc_enable_capture(start);
}
