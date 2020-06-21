#ifndef PIPETEE_H
#define PIPETEE_H
#include <stdio.h>
#include <string>
#include "pipeline.h"

class PipeTee : public Pipeline
{
public:
    PipeTee(const char *filename);
    virtual ~PipeTee();
    virtual void acceptByte(uint8_t byte) override;
    virtual void reset() override;

private:
    FILE *fp {};
    std::string filename;
};

#endif // PIPETEE_H
