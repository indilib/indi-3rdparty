#include <stdio.h>
#include "pipetee.h"

PipeTee::PipeTee(const char *filename) : filename(filename)
{
    reset();
}

PipeTee::~PipeTee()
{
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

void PipeTee::acceptByte(uint8_t byte)
{
    fwrite(&byte, 1, 1, fp);
    forward(byte);
}

void PipeTee::reset()
{
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }

    fp = fopen(filename.c_str(), "w");
}
