/**
 * INDI driver for Orion StarShoot G3
 *
 * Copyright (C) 2020 Ben Gilsrud
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <indiccd.h>
#include <inditimer.h>
#include <indipropertynumber.h>
#include <indipropertyswitch.h>
#include <indielapsedtimer.h>
#include "orion_ssg3.h"

namespace SSG3
{
class Device;
}

class SSG3CCD : public INDI::CCD
{
  public:
    SSG3CCD(struct orion_ssg3_info *info, int inst);

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    // General device functions
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    // CCD specific functions
    virtual bool UpdateCCDBin(int hor, int ver) override;
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    virtual void TimerHit() override;
    virtual int SetTemperature(double temperature) override;
    // Guide Port
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;
    void stopNSGuide();
    void stopWEGuide();

    // misc functions
    virtual bool saveConfigItems(FILE *fp) override;

  private:
    // Utility functions
    void setupParams();
    void grabImage();
    bool activateCooler(bool enable);
    void updateTemperature(void);


    bool InExposure;
    float ExposureRequest;

    INDI::PropertyNumber GainNP{1};
    INDI::PropertyNumber OffsetNP{1};
    INDI::PropertyNumber CoolerPowerNP{1};
    INDI::PropertySwitch CoolerSP{2};
    enum {
        COOLER_ON,
        COOLER_OFF,
    };

    struct orion_ssg3_info *ssg3_info;
    struct orion_ssg3 ssg3;
    int instance;
    char name[64];
    double TemperatureRequest;
    INDI::Timer TemperatureTimer; 
    INDI::Timer WETimer;
    INDI::Timer NSTimer;
    INDI::ElapsedTimer ExposureElapsedTimer;
};
