#ifndef RAW12TOBAYER16PIPELINE_H
#define RAW12TOBAYER16PIPELINE_H

#include <cstddef>
#include "pipeline.h"

struct BroadcomPipeline;
namespace INDI {
class CCDChip;
};

/**
 * @brief The Raw12ToBayer16Pipeline class
 * Accepts bytes in raw12 format and writes 16 bits bayer image.
 * RAW12 format is like | {R03,R02,R01,R00,G11,G10,G09,G08} | {R11,R10,R09,R08,R07,R06,R05,R04} | {G07,G06,G05,G04,G03,G02,G01} |
 *                                   b1                                      b2                               b3
 * Odd lines are swapped R->G, G-B
 */
class Raw12ToBayer16Pipeline : public Pipeline
{
public:
    Raw12ToBayer16Pipeline(const BroadcomPipeline *bcm_pipe, INDI::CCDChip *ccd) : Pipeline(), bcm_pipe(bcm_pipe), ccd(ccd) {}

    virtual void acceptByte(uint8_t byte) override;
    virtual void reset() override;

private:
    const BroadcomPipeline *bcm_pipe;
    INDI::CCDChip *ccd;
    int x {0};
    int y {0};
    int pos {0};
    int raw_x {0};  // Position in the raw-data comming in.
    enum {b1, b2, b3} state = b1; // Which byte in the RAW12 format (see above).
};

#endif // RAW12TOBAYER16PIPELINE_H
