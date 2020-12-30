#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>
#include <unistd.h>

#include <mmaldriver.h>
#include <mmalcamera.h>
#include <cameracontrol.h>
#include <jpegpipeline.h>
#include <broadcompipeline.h>
#include <raw10tobayer16pipeline.h>
#include <raw12tobayer16pipeline.h>
#include <pipetee.h>
#include <chipwrapper.h>
#include <config.h>


using ::testing::_;
using ::testing::StrEq;

// {{{ MockCCD: Class for mocking the CCDChip which is used by the rawxxpipes to store the image.
class MockCCD : public ChipWrapper
{
public:
    MockCCD()
    {
        width = 4056;
        height = 3040;
        bpp = 16;
        frameBufferSize = width * height * (bpp / 8);
        frameBuffer = reinterpret_cast<uint8_t *>(calloc(frameBufferSize, 1));
    }

    virtual int getFrameBufferSize() override {
        return frameBufferSize;
    }

    virtual uint8_t* getFrameBuffer() override {
        return frameBuffer;
    }

    virtual int getXRes() override { return width; }
    virtual int getYRes() override { return height; }

private:
    int width;
    int height;
    int bpp;
    uint8_t *frameBuffer;
    int frameBufferSize;
};
// }}}

// {{{ TestCameraControl
class TestCameraControl : public CameraControl, CaptureListener
{
public:
    TestCameraControl()
    {
        add_capture_listener(this);
    }

    ~TestCameraControl()
    {
        erase_capture_listener(this);
    }

    long long testCapture(int iso, int gain, long shutter_speed, const char *fname = nullptr)
    {
        MockCCD ccd;
#ifndef USE_ISO
        fprintf(stderr, "(not using iso parameter %d)\n", iso);
#endif

        MMALCamera *camera = get_camera();

        EXPECT_NE(ccd.getFrameBuffer(), nullptr);
        fprintf(stderr, "ccd: xres=%d, yres=%d\n", ccd.getXRes(), ccd.getYRes());

        JpegPipeline raw_pipe;

        BroadcomPipeline *brcm_pipe = new BroadcomPipeline();
        raw_pipe.daisyChain(brcm_pipe);

        Raw12ToBayer16Pipeline *raw12_pipe = new Raw12ToBayer16Pipeline(brcm_pipe, &ccd);
        brcm_pipe->daisyChain(raw12_pipe);

        add_pipeline(&raw_pipe);
#ifdef USE_ISO
        camera->set_iso(iso);
#endif
        camera->set_gain(gain);
        camera->set_shutter_speed(shutter_speed);

        done = false;
        start_capture();

        // Wait for end of capture.
        while(!done) {
            fprintf(stderr, "Waiting for capture to finish...\n");
            sleep(1);
        }
        fprintf(stderr, "Capture done\n");

        // Dump raw-file if requested.
        if (fname) {
            auto out = std::ofstream(fname);
            out.write(reinterpret_cast<const char *>(ccd.getFrameBuffer()), ccd.getFrameBufferSize());
            out.close();
        }

        // Calculate some number proportional to the number of photons collected.
        long long photons = 0;
        const uint16_t *p = reinterpret_cast<const uint16_t *>(ccd.getFrameBuffer());
        const uint16_t *end = reinterpret_cast<const uint16_t *>(ccd.getFrameBuffer() + ccd.getFrameBufferSize());
        while(p < end) {
            photons += *p++;
        }

        erase_pipeline(&raw_pipe);

        return photons;
    }

    virtual void capture_complete() override
    {
        done = true;
    }

private:
    bool done {false};
};
// }}}

long long photonsbias = 0;
long long photons01s1g = 0;
long long photons01s2g = 0;
long long photons02s1g = 0;
long long photons02s2g = 0;
long long photons1s1g = 0;
long long photons1s2g = 0;
long long photons2s1g = 0;
long long photons2s2g = 0;


// Grap a picture with very low exposure to work as a dark/bias base.
void get_bias_photons()
{
    TestCameraControl c;
    photonsbias = c.testCapture(400, 1, 1L);
}

TEST(TestCameraControl, save_raw_picture)
{
    TestCameraControl c;
    long long photons;
    photons = c.testCapture(400, 2, 500000L, "out/raw.data");
}

TEST(TestCameraControl, double_exposure_time_sub_second)
{
    TestCameraControl c;
    if (photons01s1g == 0) photons01s1g = c.testCapture(400, 1, 100000L) - photonsbias;
    if (photons02s1g == 0) photons02s1g = c.testCapture(400, 1, 200000L) - photonsbias;

    int relation = (int)((100 * photons02s1g) / photons01s1g);
    EXPECT_GT(relation, 120);
    EXPECT_LT(relation, 200);
    fprintf(stderr, "0.2s exposure is %d%% brighter than 0.1s\n", relation - 100);
}

TEST(TestCameraControl, double_exposure_time_seconds)
{
    TestCameraControl c;
    // For some reason the HIQ-camera needs one extra exposure before using long exposure. But only for the first long exposure...
    fprintf(stderr, "Taking one extra 20s capture..\n");
    c.testCapture(400, 1, 20000000L);
    if (photons1s1g == 0) photons1s1g = c.testCapture(400, 1, 1000000L) - photonsbias;
    if (photons2s1g == 0) photons2s1g = c.testCapture(400, 1, 2000000L) - photonsbias;

    int relation = (int)((100 * photons2s1g) / photons1s1g);
    EXPECT_GT(relation, 120);
    EXPECT_LT(relation, 200);
    fprintf(stderr, "0.2s exposure is %d%% brighter than 0.1s\n", relation - 100);
}


TEST(TestCameraControl, double_gain)
{
    TestCameraControl c;
    if (photons01s1g == 0) photons01s1g = c.testCapture(400, 1, 100000L) - photonsbias;
    if (photons01s2g == 0) photons01s2g = c.testCapture(400, 2, 100000L) - photonsbias;

    int relation = (int)((100 * photons01s2g) / photons01s1g);
    EXPECT_GT(relation, 120);
    EXPECT_LT(relation, 200);
    fprintf(stderr, "0.2s exposure is %d%% brighter than 0.1s\n", relation - 100);
}

#ifdef USE_ISO
TEST(TestCameraControl, double_iso)
{
    TestCameraControl c;
    long long photons1, photons2;
    photons1 = c.testCapture(100, 1, 100000L) - photonsbias;
    photons2 = c.testCapture(800, 1, 100000L) - photonsbias;

    int relation = (int)((100 * photons2) / photons1);
    EXPECT_GT(relation, 120);
    EXPECT_LT(relation, 200);
    fprintf(stderr, "0.2s exposure is %d%% brighter than 0.1s\n", relation - 100);
}
#endif

int main(int argc, char **argv)
{
    fprintf(stderr, "Main started\n");
    get_bias_photons();
    fprintf(stderr, "Bias photons: %lld\n", photonsbias);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    return RUN_ALL_TESTS();
}
