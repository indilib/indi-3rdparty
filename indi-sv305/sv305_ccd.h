/*
 SV305 CCD
 SVBONY SV305 Camera driver
 Copyright (C) 2020 Blaise-Florentin Collin (thx8411@yahoo.fr)

 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#ifndef SV305_CCD_H
#define SV305_CCD_H

#include <indiccd.h>
#include <iostream>

#include "../libsv305/CKCameraInterface.h"


using namespace std;

class Sv305CCD : public INDI::CCD
{
  public:
    Sv305CCD(int numCamera);
    virtual ~Sv305CCD();

    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect() override;
    bool Disconnect() override;

    bool StartExposure(float duration) override;
    bool AbortExposure() override;

  protected:
    void TimerHit() override;

  private:
    CameraSdkStatus status;
    HANDLE hCamera;
    HANDLE hRawBuf;

    int num;
    char name[32];
//

    double minDuration;
    BYTE* imageBuffer;

    int timerID;

    struct timeval ExpStart;

    float ExposureRequest;

    float CalcTimeLeft();
    int grabImage();
    bool setupParams();


    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
//

};

#endif // SV305_CCD_H
