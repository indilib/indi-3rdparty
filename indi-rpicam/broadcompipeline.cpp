#include <cstring>
#include <stdexcept>
#include "broadcompipeline.h"

void BroadcomPipeline::reset()
{
    pos = -1;
    memset(&header, 0, sizeof header);
}

void BroadcomPipeline::acceptByte(uint8_t byte)
{
    pos++;

    if (pos < 9) {
        header.BRCM[pos] = byte;
        if (pos == 8) {
            if (strcmp(header.BRCM, "@BRCMo") != 0) {
                throw std::runtime_error("Did not find BRCM header");
            }
        }
    }
    else if (pos < (signed)((9 + sizeof header.omx_data))) {
        reinterpret_cast<uint8_t *>(&header.omx_data)[pos - 9] = byte;
    }
    else if (pos >= 32768) {
        forward(byte);
    }
}

