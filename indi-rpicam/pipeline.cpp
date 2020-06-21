#include <stdexcept>
#include "pipeline.h"

Pipeline::Pipeline()
{

}

void Pipeline::daisyChain(Pipeline *p)
{
    Pipeline *last = this;
    while(last->nextPipeline != nullptr) {
        last = last->nextPipeline;
    }
    last->nextPipeline = p;
}

void Pipeline::forward(uint8_t byte)
{
    if (nextPipeline == nullptr) {
        throw std::runtime_error("No next pipeline to forward bytes to.");
    }

    nextPipeline->acceptByte(byte);
}

void Pipeline::reset_pipe()
{
    Pipeline *pipe = this;
    while(pipe != nullptr) {
        pipe->reset();
        pipe = pipe->nextPipeline;
    }
}
