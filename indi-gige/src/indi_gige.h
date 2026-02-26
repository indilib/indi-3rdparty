/*
 GigE interface for INDI based on aravis
 Copyright (C) 2016 Hendrik Beijeman (hbeyeman@gmail.com)

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

#ifndef GENERIC_CCD_H
#define GENERIC_CCD_H

#include <indiccd.h>

#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "ArvInterface.h"

using namespace std;

class GigECCD : public INDI::CCD
{
  public:
    GigECCD(arv::ArvCamera *camera);
    virtual ~GigECCD();

    virtual const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;

    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    /** Request streaming to start.
     * This should not be called with camera_mutex locked. */
    virtual bool StartStreaming() override;
    /** Request streaming to stop.
     * This should not be called with camera_mutex locked. */
    virtual bool StopStreaming() override;

  protected:
    virtual void TimerHit() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;
    virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword) override;
    virtual bool saveConfigItems(FILE *fp) override;

  private:
    void _delete_indi_properties(void);
    void _update_indi_properties(void);
    void _update_bin(void); /// update binning to INDI from hardware
    bool _update_geometry(void); /// update geometry to INDI from hardware
    void _update_image(uint8_t const *const data, size_t size);

    void _handle_failed(void);
    void _handle_timeout(struct timeval *const tv, uint32_t timeout_us);
    void start_streaming_thread();
    /** Request streaming to stop and the streaming thread to quit.
     * This should not be called with camera_mutex locked. */
    void stop_streaming_thread();

    arv::ArvCamera *camera;
    std::recursive_mutex camera_mutex;
    int timer_id;
    struct timeval exposure_start_time;
    struct timeval exposure_transfer_time;

    std::thread streaming_thread;
    std::atomic<bool> streaming_thread_stop_requested {false};
    std::atomic<bool> streaming_thread_active {false};
    std::condition_variable_any streaming_thread_condition;

    /* Indi properties */

    INDI::PropertyNumber GainNP {1};
    INDI::PropertyNumber PixelSizeNP {1};
    IText indiprop_info[3] {};
    ITextVectorProperty indiprop_info_prop;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                            char *formats[], char *names[], int n);
};

#endif // GENERIC_CCD_H
