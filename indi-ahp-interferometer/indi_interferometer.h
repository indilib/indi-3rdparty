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

#define HEADER_SIZE 16
#define MAX_RESOLUTION 2048
#define PIXEL_SIZE (AIRY / settingsN[0].value / MAX_RESOLUTION)
#define STOP_BITS 1
#define WORD_SIZE 8
#define BAUD_SIZE (STOP_BITS+WORD_SIZE+1.0)
#define BAUD_RATE (serialConnection->baud())
#define NUM_BASELINES (NUM_LINES*(NUM_LINES-1)/2)
#define FRAME_SIZE (((NUM_LINES+NUM_BASELINES*DELAY_LINES)*SAMPLE_SIZE)+HEADER_SIZE)
#define FRAME_TIME (BAUD_SIZE*FRAME_SIZE/BAUD_RATE)
#define SAMPLE_RATE (pow(2, sample_size)*serialConnection->baud())
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

class Interferometer : public INDI::CCD
{
public:
    Interferometer();
    ~Interferometer() {
        for(int x = 0; x < NUM_BASELINES; x++)
            baselines[x]->~baseline();

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

    void CaptureThread();
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

    bool Handshake();
    void setConnection(const uint8_t &value);
    uint8_t getConnection() const;

    Connection::Serial *serialConnection;

    // For Serial connection
    int PortFD = -1;

private:

    enum it_cmd {
        CLEAR = 0,
        SET_ACTIVE_LINE = 0x01,
        SET_LEDS = 0x02,
        SET_BAUDRATE = 0x03,
        SET_DELAY = 0x04,
        COMMIT = 0x0c,
        ENABLE_CAPTURE = 0x0d,
    };

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
    double *totalcorrelations;
    double  *alt;
    double *az;
    double *delay;
    baseline** baselines;

    INumber settingsN[2];
    INumberVectorProperty settingsNP;

    unsigned int clock_frequency;

    double timeleft;
    double wavelength;
    void Callback();
    bool callHandshake();
    // Utility functions
    float CalcTimeLeft();
    void  setupParams();
    bool SendChar(char);
    bool SendCommand(it_cmd cmd, unsigned char value = 0);
    void ActiveLine(int, bool, bool);
    void EnableCapture(bool start);
    // Struct to keep timing
    struct timeval ExpStart;
    float ExposureRequest;
    bool threadsRunning;

    int NUM_LINES;
    int DELAY_LINES;
    int SAMPLE_SIZE;
};
