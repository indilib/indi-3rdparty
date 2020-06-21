#ifndef PIXELLISTENER_H
#define PIXELLISTENER_H


class PixelListener
{
public:
    virtual void pixels_received(uint8_t *buffer, size_t length) = 0;
    virtual void capture_complete() = 0;
};

#endif // PIXELLISTENER_H
