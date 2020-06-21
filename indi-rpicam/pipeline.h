#ifndef PIPELINE_H
#define PIPELINE_H

#include <cstdint>

class Pipeline
{
public:
    Pipeline();
    void daisyChain(Pipeline *p);
    void reset_pipe();

    virtual void acceptByte(uint8_t byte) = 0;
    virtual void reset() = 0;

protected:
    void forward(uint8_t);

private:
    Pipeline *nextPipeline {};
};

#endif // PIPELINE_H
