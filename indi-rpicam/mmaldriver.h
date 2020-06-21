#ifndef _INDI_MMAL_H
#define _INDI_MMAL_H

#include <indiccd.h>
#include <atomic>
#include "cameracontrol.h"
#include "pixellistener.h"
#include "jpegpipeline.h"
#include "broadcompipeline.h"
#include "raw12tobayer16pipeline.h"

#undef USE_ISO
#define DEFAULT_ISO 400

class Pipeline;

class MMALDriver : public INDI::CCD, PixelListener
{
public:
	MMALDriver();
	virtual ~MMALDriver();

    // INDI::CCD Overrides
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual void pixels_received(uint8_t *buffer, size_t length) override;
    virtual void capture_complete() override;

protected:
    // INDI::CCD overrides.
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE * fp) override;
    virtual void addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip) override;
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int hor, int ver) override;
    virtual void TimerHit() override;

private:
  // Utility functions
  double CalcTimeLeft();
  void setupParams();
  void grabImage();
  void updateFrameBufferSize();
  void assert_framebuffer(INDI::CCDChip *ccd);

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

  JpegPipeline jpeg_pipe; // Start of pipeline that recieved raw data from camera.
  BroadcomPipeline brcm_pipe;  // Second in pipe.
  //    PipeTee raw_writer("/dev/shm/capture.tap"); // Only for debugging by tapping intermediate data.
  Raw12ToBayer16Pipeline raw12_pipe; // Final in pipe, converting RAW12 to Bayer 16 bits.
};

extern std::unique_ptr<MMALDriver> mmalDevice;
#endif /* _INDI_MMAL_H */
