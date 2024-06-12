/*
 Raspberry Pi High Quality Camera CCD Driver for Indi.
 Copyright (C) 2020 Lars Berntzon (lars.berntzon@cecilia-data.se).
 All rights reserved.

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

#ifndef _INDI_MMAL_H
#define _INDI_MMAL_H

#include <indiccd.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include "cameracontrol.h"
#include "jpegpipeline.h"
#include "broadcompipeline.h"
#include "raw12tobayer16pipeline.h"
#include "capturelistener.h"
#include "chipwrapper.h"
#include "config.h"

#ifdef USE_ISO
#define DEFAULT_ISO 400
#endif

class Pipeline;
class TestMMALDriver;

class MMALDriver : public INDI::CCD, CaptureListener
{
public:
	MMALDriver();
	virtual ~MMALDriver();

    // INDI::CCD Overrides
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) override;
    virtual void capture_complete() override;

protected:
    // INDI::CCD overrides.
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE * fp) override;
    virtual void addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int hor, int ver) override;
    virtual void TimerHit() override;

private:
  // Utility functions
  double CalcTimeLeft();
  void assert_framebuffer(INDI::CCDChip *ccd);

  /** Setup the pipeline of buffer processors, depending of camera type. */
  void setupPipeline();

  // Struct to keep timing
  struct timeval ExpStart { 0, 0 };

  double ExposureRequest { 0 };
  uint8_t *image_buffer_pointer {nullptr};

  std::atomic<bool> exposure_thread_done {false};

#ifdef USE_ISO
  ISwitch mIsoS[4];
  ISwitchVectorProperty mIsoSP;
#endif
  INumber mGainN[1];
  INumberVectorProperty mGainNP;

  std::unique_ptr<CameraControl> camera_control; // Controller object for the camera communication.

  std::unique_ptr<Pipeline> raw_pipe; // Start of pipeline that recieved raw data from camera.

  ChipWrapper chipWrapper;

  friend TestMMALDriver;
};

extern std::unique_ptr<MMALDriver> mmalDevice;
#endif /* _INDI_MMAL_H */
