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

#define BAUD_RATE 230400
#define NUM_NODES 14
#define NUM_BASELINES NUM_NODES*(NUM_NODES-1)/2
#define SAMPLE_SIZE 4
#define FRAME_SIZE (NUM_NODES+NUM_BASELINES)*SAMPLE_SIZE
#define FRAME_TIME_NS 10.0*FRAME_SIZE/BAUD_RATE

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

    enum
    {
        CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
        CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
        CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
    } CorrelatorConnection;


    Connection::Serial *serialConnection;
    Connection::TCP *tcpConnection;

    /// For Serial & TCP connections
    int PortFD = -1;

private:
    INumber locationN[3*NUM_NODES];
    INumberVectorProperty locationNP[NUM_NODES];

    INumber settingsN[2];
    INumberVectorProperty settingsNP;
    double timeleft;
    double wavelength;
    baseline* baselines[NUM_BASELINES];
    void Callback();
    bool callHandshake();
    uint8_t getInterferometerConnection() const;
    void setInterferometerConnection(const uint8_t &value);
    uint8_t interferometerConnection = CONNECTION_NONE;
    // Utility functions
    float CalcTimeLeft();
    void  setupParams();
    // Struct to keep timing
    struct timeval ExpStart;
    float ExposureRequest;
};
