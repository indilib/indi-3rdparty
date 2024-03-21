/*
    INDI LibCamera Driver

    Copyright (C) 2022 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include <indisinglethreadpool.h>
#include <stdint.h>

#include "core/rpicam_app.hpp"
#include "core/rpicam_encoder.hpp"
#include "core/still_options.hpp"

#include <vector>

#include <indiccd.h>
#include <inditimer.h>

class RPiCamINDIApp : public RPiCamApp
{
public:
    RPiCamINDIApp() : RPiCamApp(std::make_unique<StillOptions>()) {}

    StillOptions *GetOptions() const
    {
        return static_cast<StillOptions *>(options_.get());
    }
};

class SingleWorker;
class INDILibCamera : public INDI::CCD
{
public:
    INDILibCamera(uint8_t index, const libcamera::ControlList &list);

    virtual const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;

    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;

    static void default_signal_handler(int signal_number);

protected:

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    // Streaming
    virtual bool StartStreaming() override;
    virtual bool StopStreaming() override;

    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;

    // specific keywords
    virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

    // Save config
    virtual bool saveConfigItems(FILE *fp) override;

    /** Get the current Bayer string used */
    const char *getBayerString() const;
    INDI_PIXEL_FORMAT bayerToPixelFormat(const char *bayer);
    int getColorspaceFlags(std::string const &codec);

protected:
    INDI::SingleThreadPool m_Worker;
    void workerStreamVideo(const std::atomic_bool &isAboutToQuit, double framerate);
    void workerExposure(const std::atomic_bool &isAboutToQuit, float duration);
    void outputReady(void *mem, size_t size, int64_t timestamp_us, bool keyframe);
    void metadataReady(libcamera::ControlList &metadata);
    bool SetCaptureFormat(uint8_t index) override;
    void initSwitch(INDI::PropertySwitch &switchSP, int n, const char **names);

    void configureStillOptions(StillOptions *options, double duration);
    void configureVideoOptions(VideoOptions *options, double framerate);


protected:
    /** Get initial parameters from camera */
    void setup();

    enum
    {
        CAPTURE_DNG,
        CAPTURE_JPG
    };

    bool processRAW(const char *filename, uint8_t **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel, char *bayer_pattern);

    bool processRAWMemory(unsigned char *inBuffer, unsigned long inSize, uint8_t **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel, char *bayer_pattern);

    bool processJPEG(const char *filename, uint8_t **memptr, size_t *memsize, int *naxis, int *w, int *h);

    int processJPEGMemory(unsigned char *inBuffer, unsigned long inSize, uint8_t **memptr, size_t *memsize, int *naxis, int *w, int *h);

    void shutdownVideo();

private:

    enum
    {
        AdjustBrightness = 0, AdjustContrast, AdjustSaturation, AdjustSharpness, AdjustQuality, AdjustExposureValue,
        //AdjustMeteringMode, AdjustExposureMode, AdjustAwbMode,
        AdjustAwbRed, AdjustAwbBlue
    };

    INDI::PropertySwitch AdjustExposureModeSP {0}, AdjustAwbModeSP {0}, AdjustMeteringModeSP {0}, AdjustDenoiseModeSP {0} ;
    INDI::PropertyNumber AdjustmentNP {AdjustAwbBlue+1};
    INDI::PropertyNumber GainNP {1};

    // std::unique_ptr<RPiCamApp> m_CameraApp;
    // std::unique_ptr<RPiCamEncoder> m_CameraEncoder;

    int m_LiveVideoWidth {-1}, m_LiveVideoHeight {-1};
    uint8_t m_CameraIndex;
    libcamera::ControlList m_ControlList;

};
