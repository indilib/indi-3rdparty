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

#pragma once

#include "indiccd.h"
#include "indicorrelator.h"
#include <ahp/ahp_xc.h>

class baseline : public INDI::Correlator
{
public:
    baseline() : Correlator() { }
    ~baseline() { }

    const char *getDefaultName() { return "baseline"; }
    inline bool StartIntegration(double duration) { INDI_UNUSED(duration); return true; }
    inline double getCorrelationDegree() { return 0.0; }
    inline bool Handshake() { return true; }
};

class AHP_XC : public INDI::CCD
{
public:
    AHP_XC();
    ~AHP_XC() {
        for(int x = 0; x < ahp_xc_get_nbaselines(); x++)
            baselines[x]->~baseline();

        for(int x = 0; x < ahp_xc_get_nlines(); x++)
            ahp_xc_set_leds(x, 0);

        ahp_xc_set_baudrate(R_57600);
        ahp_xc_disconnect();

        free(correlationsN);

        free(lineStatsN);
        free(lineStatsNP);

        free(lineEnableS);
        free(lineEnableSP);

        free(linePowerS);
        free(linePowerSP);

        free(lineDelayN);
        free(lineDelayNP);

        free(lineGPSN);
        free(lineGPSNP);

        free(lineTelescopeN);
        free(lineTelescopeNP);

        free(lineDomeN);
        free(lineDomeNP);

        free(snoopGPSN);
        free(snoopGPSNP);

        free(snoopTelescopeN);
        free(snoopTelescopeNP);

        free(snoopTelescopeInfoN);
        free(snoopTelescopeInfoNP);

        free(snoopDomeN);
        free(snoopDomeNP);

        free(lineDevicesT);
        free(lineDevicesTP);

        free(autocorrelationsB);
        free(crosscorrelationsB);
        free(plotB);

        free(autocorrelations_str);
        free(crosscorrelations_str);
        free(plot_str);

        free(totalcounts);
        free(totalcorrelations);
        free(alt);
        free(az);
        free(delay);
        free(baselines);
    }

    void ISGetProperties(const char *dev);
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
    bool ISSnoopDevice(XMLEle *root);

protected:

    // General device functions
    bool Disconnect();
    const char *getDeviceName();
    bool saveConfigItems(FILE *fp);
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

    // CCD specific functions
    bool StartExposure(float duration);
    bool AbortExposure();
    void TimerHit();
    void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip);

    bool Connect();

    Connection::Serial *serialConnection;

private:

    enum it_cmd {
        CLEAR = 0,
        SET_INDEX = 1,
        SET_LEDS = 2,
        SET_BAUD_RATE = 3,
        SET_DELAY = 4,
        SET_FREQ_DIV = 5,
        ENABLE_CAPTURE = 13
    };

    std::thread *readThread;

    INumber *correlationsN;
    INumberVectorProperty correlationsNP;

    INumber *lineStatsN;
    INumberVectorProperty *lineStatsNP;

    ISwitch *lineEnableS;
    ISwitchVectorProperty *lineEnableSP;

    ISwitch *linePowerS;
    ISwitchVectorProperty *linePowerSP;

    INumber *lineDelayN;
    INumberVectorProperty *lineDelayNP;

    INumber *lineGPSN;
    INumberVectorProperty *lineGPSNP;

    INumber *lineTelescopeN;
    INumberVectorProperty *lineTelescopeNP;

    INumber *lineDomeN;
    INumberVectorProperty *lineDomeNP;

    INumber *snoopGPSN;
    INumberVectorProperty *snoopGPSNP;

    INumber *snoopTelescopeN;
    INumberVectorProperty *snoopTelescopeNP;

    INumber *snoopTelescopeInfoN;
    INumberVectorProperty *snoopTelescopeInfoNP;

    INumber *snoopDomeN;
    INumberVectorProperty *snoopDomeNP;

    IText *lineDevicesT;
    ITextVectorProperty *lineDevicesTP;

    double *totalcounts;
    ahp_xc_correlation *totalcorrelations;
    double  *alt;
    double *az;
    double *delay;
    double *framebuffer;
    baseline** baselines;

    IBLOB *autocorrelationsB;
    IBLOBVectorProperty autocorrelationsBP;

    IBLOB *crosscorrelationsB;
    IBLOBVectorProperty crosscorrelationsBP;

    IBLOB *plotB;
    IBLOBVectorProperty plotBP;

    dsp_stream_p *autocorrelations_str;
    dsp_stream_p *crosscorrelations_str;
    dsp_stream_p *plot_str;

    INumber settingsN[2];
    INumberVectorProperty settingsNP;

    unsigned int clock_frequency;
    unsigned int clock_divider;

    float timeleft;
    double wavelength;
    void Callback();
    bool callHandshake();
    // Utility functions
    float CalcTimeLeft();
    void  setupParams();
    bool SendChar(char);
    bool SendCommand(it_cmd cmd, unsigned char value = 0);
    void ActiveLine(int, bool, bool);
    void SetFrequencyDivider(unsigned char divider);
    void EnableCapture(bool start);
    void sendFile(IBLOB* Blobs, IBLOBVectorProperty BlobP, int len);
    int getFileIndex(const char * dir, const char * prefix, const char * ext);
    float CalcTimeLeft(timeval start, float req);
    // Struct to keep timing
    struct timeval ExpStart;
    float ExposureRequest;
    float ExposureStart;
    bool threadsRunning;

    inline double getCurrentTime()
    {
        struct timeval now
        {
            0, 0
        };
        gettimeofday(&now, nullptr);
        return (double)(now.tv_sec)+(double)now.tv_usec/1000000.0;
    }
};
