/*
INDI QHY V4L2 CCD Driver

Copyright (C) 2018 Robert Lancaster (rlancaste AT gmail DOT com)

This driver was inspired by the INDI FFMpeg Driver written by
Geehalel (geehalel AT gmail DOT com), see: https://github.com/geehalel/indi-ffmpeg

This driver is free software; you can redistribute it and/or
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

#ifndef indi_qhy_v4l2_H
#define indi_qhy_v4l2_H

#include <indiccd.h>
#include <stream/streammanager.h>

//#include <ctime>
#include <thread>

// V4L2 direct access support for Multiplanar devices
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


class indi_qhy_v4l2 : public INDI::CCD
{
public:
    indi_qhy_v4l2(const std::string &name = "QHY CCD V4L2",
                  const std::string &videoPath = "",
                  const std::string &subdevPath = "",
                  const std::string &role = "",
                  double discoveredPixelSize = 0.0);
    ~indi_qhy_v4l2();
    void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) override;
protected:
    // General device functions
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    void debugTriggered(bool enabled) override;



    //Related to exposures
    bool StartExposure(float duration) override;
    bool AbortExposure() override;
    void finishExposure();
    void TimerHit() override;
    float CalcTimeLeft();
    int timerID = 0;
    bool grabImage();

    bool UpdateCCDFrame(int x, int y, int w, int h) override;

    // Custom gain and offset controls; these do not override base-class methods.
    bool SetCCDGain(double gain);
    bool SetCCDOffset(double offset);

    //Related to streaming
    virtual bool StartStreaming() override;
    virtual bool StopStreaming() override;

    bool saveConfigItems(FILE *fp) override;

private:

    //Related to exposures
    struct timeval ExpStart { 0, 0 };
    float ExposureRequest { 0 };
    bool convertINDI_RGBtoFITS_RGB(uint8_t *originalImage, uint8_t *convertedImage);

    //These are related to how we change sources
    bool ConnectToSource(const std::string &source);
    bool ChangeSource(std::string newDevice, std::string newSource, int newFramerate, std::string newInputPixelFormat, std::string newVideosize);
    bool reconnectSource();

    //These are related to updating the device list
    bool refreshInputDevices();
    bool refreshInputSources();
    ISwitch RefreshS[1];
    ISwitchVectorProperty RefreshSP;

    //webcam stacking.
    bool webcamStacking = false;
    bool gotAnImageAlready = false;
    bool loadingSettings = false;
    bool averaging = false;
    float *stackBuffer = nullptr;
    int numberOfFramesInStack = 0;
    bool addToStack();
    void copyFinalStackToPrimaryFrameBuffer();
    void setImageDataValueFromFloat(int x, int y, float value, bool roundAnswer);
    void setRGBImageDataValueFromFloat(int x, int y, int channel, float value, bool roundAnswer);
    float getImageDataFloatValue(int x, int y);
    float getRGBImageDataFloatValue(int x, int y, int channel);

    //These are our device capture settings
    std::string defaultDeviceName;
    bool use16Bit = true;
    std::string videoDevice = "";
    std::string videoSource = "";
    std::string v4l2_role = "";
    int frameRate = 0;
    std::string videoSize = "";
    std::string inputPixelFormat = "";
    std::string outputFormat = "";
    // Maximum wait time used while polling a V4L2 buffer.
    double bufferTimeout = 0;

    //The pixel size for the camera
    double pixelSize = 0;

    //Related to Options in the Control Panel
    IText InputOptionsT[6] {};
    ITextVectorProperty InputOptionsTP;
#ifdef __linux__
    IText V4L2SubdevPathT[1] {};
    ITextVectorProperty V4L2SubdevPathTP;
#endif

    ISwitch *CaptureDevices = nullptr;
    ISwitchVectorProperty CaptureDeviceSelection;
    ISwitch *CaptureSources = nullptr;
    ISwitchVectorProperty CaptureSourceSelection;
    ISwitch *FrameRates = nullptr;
    ISwitchVectorProperty FrameRateSelection;
    ISwitch *PixelFormats = nullptr;
    ISwitchVectorProperty PixelFormatSelection;
    ISwitch *VideoSizes = nullptr;
    ISwitchVectorProperty VideoSizeSelection;
    ISwitch *RapidStacking = nullptr;
    ISwitchVectorProperty RapidStackingSelection;
    ISwitch *OutputFormats = nullptr;
    ISwitchVectorProperty OutputFormatSelection;
    // Optional 16-bit byte-order correction.
    ISwitch *EndianFix = nullptr;
    ISwitchVectorProperty EndianFixSelection;
    bool swap16_on_send = false;
    ISwitch *PixelSizes = nullptr;
    ISwitchVectorProperty PixelSizeSelection;

    INumber TimeoutOptionsT[1] {};
    INumberVectorProperty TimeoutOptionsTP;
    INumber PixelSizeT[1] {};
    INumberVectorProperty PixelSizeTP;
    INumber VideoAdjustmentsT[3] {};
    INumberVectorProperty VideoAdjustmentsTP;
    // Exposure is controlled by the client through CCD_EXPOSURE.

    // V4L2 gain and offset properties.
    INumber GainT[1] {};
    INumberVectorProperty GainTP;
    INumber OffsetT[1] {};
    INumberVectorProperty OffsetTP;


    //Webcam setup, release, and frame capture
    bool flush_frame_buffer();
    bool setupStreaming();
    void freeMemory();
    bool getStreamFrame();

    //Related to streaming
    std::thread capture_thread;
    static void RunCaptureThread(indi_qhy_v4l2 *webcam);
    void run_capture();
    bool is_capturing = false;
    bool is_streaming = false;
    void start_capturing();
    void stop_capturing();

    uint8_t *buffer;
    int numBytes = 0;

    // V4L2 direct access for Multiplanar devices
    bool use_v4l2_direct = false;
    int v4l2_fd = -1;
    struct v4l2_format v4l2_fmt;
    struct v4l2_buffer *v4l2_buffers = nullptr;
    unsigned int v4l2_buffer_count = 0;
    bool v4l2_streaming = false;
    bool v4l2_is_mplane = false;                    // Selects the multi-planar capture API.
    void **v4l2_mmap_ptrs = nullptr;
    size_t *v4l2_mmap_lens = nullptr;
    bool v4l2_force_16bit = false;                  // Forces output through a 16-bit container.
    std::string v4l2_subdev_path;                   // Discovered from the device tree and media topology.
    double v4l2_subdev_exposure = 10000.0;          // Exposure in 100 microsecond units.
    int v4l2_subdev_fd = -1;
    double v4l2_subdev_exposure_min = 1.0;          // Minimum exposure in 100 microsecond units.
    double v4l2_subdev_exposure_max = 100000.0;     // Maximum exposure in 100 microsecond units.

    // V4L2 gain and offset controls.
    int32_t v4l2_subdev_gain = 64;
    int32_t v4l2_subdev_gain_min = 64;
    int32_t v4l2_subdev_gain_max = 90112;
    int32_t v4l2_subdev_offset = 0;                 // Uses the standard V4L2 black-level control.
    int32_t v4l2_subdev_offset_min = 0;
    int32_t v4l2_subdev_offset_max = 0;
    int32_t v4l2_subdev_offset_step = 1;
    uint32_t v4l2_subdev_offset_control_id = V4L2_CID_BLACK_LEVEL;
    bool v4l2_offset_supported = false;

    // Optional raw save after exposure
    bool save_raw_enable = true;
    std::string save_raw_path = "/tmp/indi_qhy_v4l2.raw";  // Destination for captured raw frames.
    IText SaveRawPathT[1] {};
    ITextVectorProperty SaveRawPathTP;
    ISwitch SaveRawS[1] {};
    ISwitchVectorProperty SaveRawSP;

    // Driver info property (for external clients setting DRIVER_INFO)
    IText DriverInfoT[4] {};
    ITextVectorProperty DriverInfoTP;

    bool ConnectToSourceV4L2(std::string source);
    bool DisconnectV4L2();
    bool getStreamFrameV4L2();
    bool flush_frame_bufferV4L2();
    bool discardInitialV4L2Frames(unsigned int count);
    bool setupV4L2Streaming();
    void freeV4L2Memory();

    // Enhanced V4L2 functions
    bool enumerateV4L2Formats();
    bool enumerateV4L2Sizes();
    bool enumerateV4L2FrameRates();
    bool setV4L2Format(uint32_t pixelformat);
    bool setV4L2Size(unsigned int width, unsigned int height);
    bool setV4L2FrameRate(unsigned int numerator, unsigned int denominator);
    bool setV4L2Exposure(double value);
    bool queryV4L2Control(unsigned int ctrl_id, struct v4l2_queryctrl *queryctrl);
    bool setV4L2Control(unsigned int ctrl_id, int32_t value);
    bool getV4L2Control(unsigned int ctrl_id, int32_t *value);
    bool ensureManualExposureMode();
    bool openV4L2Subdevice();
    void closeV4L2Subdevice();
    void updateV4L2SubdevExposureRange();
    void syncV4L2ExposureFromDuration(double duration);

    bool setV4L2Gain(int32_t gain);
    bool getV4L2Gain(int32_t *gain);
    void updateV4L2GainRange();
    bool setV4L2Offset(int32_t offset);
    bool getV4L2Offset(int32_t *offset);
    void updateV4L2OffsetRange();                       // Queries the standard black-level control.
    void updateV4L2ImageMetadata();                     // Updates bit depth and Bayer metadata from FOURCC.

    bool setV4L2Crop(int x, int y, int w, int h);
    struct v4l2_rect getV4L2Crop();
    bool v4l2_can_crop = false;
    struct v4l2_cropcap v4l2_cropcap;
    struct v4l2_crop v4l2_crop;
    // Queue all buffers before every STREAMON operation.
    bool requeueAllV4L2Buffers();

};
#endif // indi_qhy_v4l2_H
