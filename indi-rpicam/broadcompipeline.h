#ifndef BROADCOMPIPELINE_H
#define BROADCOMPIPELINE_H
#include "pipeline.h"

// Mostly guesswork.
struct BroadcomHeader
{
    char BRCM[9];               // @BRCM
    struct
    {
        uint16_t size;          // Total size for buffer, excluding BRCM.
        uint16_t dummy1[3];     // 00 00 00 00 00 00
        char name[144];         // Driver name and version??
        uint16_t raw_width;     // Scanline size resolution x + padding.
        uint16_t dummy2[7];     // 00 * 7
        char crop_size[32];     // full, cropsize ??
        uint16_t width;         // Sensor width
        uint16_t height;        // Sensor height
        uint16_t pad_right;     // 02 00
        uint16_t pad_down;      // 02 00
        uint32_t dummy3[2];     // 9b 6f 00 00, ac 0d 00 00
        uint32_t dummy4[2];     // ff ff ff ff, 01 00 00 00
        uint16_t dummy5[8];      // 03 00, 21 00, 02 04, 0c 00, 00 00, 00 00, 02 00, 00 00
    } omx_data;
};

class BroadcomPipeline : public Pipeline
{
public:
    BroadcomPipeline() {}
    virtual void acceptByte(uint8_t byte) override;
    virtual void reset();
    BroadcomHeader header;

private:
    int pos {-1};
};

#endif // BROADCOMPIPELINE_H
