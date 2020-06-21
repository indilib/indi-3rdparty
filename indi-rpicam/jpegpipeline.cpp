#include <stdio.h>
#include <stdexcept>
#include "jpegpipeline.h"

void JpegPipeline::reset()
{
    state = State::WANT_FF;
    pos = -1;
    s1 = 0;
    s2 = 0;
    skip_bytes = 0;
    entropy_data_follows = false;
    current_type = 0;
}

/**
 * @brief RawStreamReceiver::accept_byte Accepts next byte of input and sets state accordingly.
 * @param byte Next byte to handle.
 * @return
 */
void JpegPipeline::acceptByte(uint8_t byte)
{
    pos++;

    switch(state)
    {
    case State::END_OF_JPEG:
        forward(byte);
        return;

    case State::INVALID:
        throw Exception("State invalid");
        return;

    case State::WANT_FF:
        if (byte != 0xFF) {
            throw Exception("Expected 0xFF");
        }
        state = State::WANT_TYPE;
        return;

    case State::WANT_S1:
        s1 = byte;
        state = State::WANT_S2;
        return;

    case State::WANT_S2:
        s2 = byte;
        state = State::SKIP_BYTES;
        skip_bytes = (s1 << 8) + s2 - 2;  // -2 since we already read S1 S2
        return;

    case State::SKIP_BYTES:
        skip_bytes--;
        if (skip_bytes == 0) {
            if (entropy_data_follows) {
                state = State::WANT_ENTROPY_DATA;
            }
            else {
                state = State::WANT_FF;
            }
        }
        return;

    case State::WANT_ENTROPY_DATA:
        if (byte == 0xFF) {
            state = State::ENTROPY_GOT_FF;
        }
        return;

    case State::ENTROPY_GOT_FF:
        if (byte == 0) {
            // Just an escaped 0
            state = State::WANT_ENTROPY_DATA;
            return;
        }
        else if (byte == 0xFF) {
            // Padding
            state = State::ENTROPY_GOT_FF;
            return;
        }
        // If not FF00 and FFFF then we got a real segment type now.
        state = State::WANT_TYPE;
        entropy_data_follows = false;

        // FALL THROUGH

    case State::WANT_TYPE:
        current_type = byte;
        switch(byte)
        {
        case 0xd8: // SOI (Start of image)
            state = State::WANT_FF;
            return;

        case 0xd9: // EOI (End of image)
            state = State::END_OF_JPEG;
            return;

        case 0xda: // SOS (Start of scan)
        case 0xc0: // Baseline DCT
        case 0xc4: // Huffman Table
            entropy_data_follows = true;
            // Above sections will have entropy data following.
            // Fall through
        case 0xdb: // Quantization Table
        case 0xe0: // JFIF APP0
        case 0xe1: // JFIF APP0
            state = State::WANT_S1;
            return;

        default:
            throw Exception("Unknown JPEG segment type.");
            return;
        }
    }
}
