#ifndef _JPEGPIPELINE_H
#define _JPEGPIPELINE_H

#include "pipeline.h"

/**
 * @brief The RawStreamReceiver class
 * Repsonsible for receiving a raw image from the MMAL subsystem. In this mode
 * the image consist of a normal JPEG-image, followed by a 32K broadcom header and then
 * the true raw data. This class accepts one byte at a time and spools past the JPEG header,
 * picks up the row pitch and then spools past the @BRCMo data.
 */
class JpegPipeline : public Pipeline
{
public:
    class Exception : public std::runtime_error
    {
    public: Exception(const char *text) : std::runtime_error(text) {}
    };

    enum class State {
        WANT_FF,
        WANT_TYPE,
        WANT_S1,
        WANT_S2,
        SKIP_BYTES,
        WANT_ENTROPY_DATA,
        ENTROPY_GOT_FF,
        END_OF_JPEG,
        INVALID} state {State::WANT_FF};

    JpegPipeline() {}

    virtual void acceptByte(uint8_t byte) override;
    virtual void reset() override;

    State getState() { return state; }
    int getPosition() { return pos; }

private:
    int pos {-1};
    int s1 {0}; // Length field, first byte MSB.
    int s2 {0}; // Length field, second byte LSB.
    int skip_bytes {0}; //Counter for skipping bytes.
    bool entropy_data_follows {false};
    int current_type {}; // For debugging only.
};

#endif // _JPEGPIPELINE_H
