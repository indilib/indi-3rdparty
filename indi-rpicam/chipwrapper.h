/*
 Raspberry Pi High Quality Camera CCD Driver for Indi.
 Copyright (C) 2020 Lars Berntzon (lars.berntzon@cecilia-data.se).
 All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef CHIPWRAPPER_H
#define CHIPWRAPPER_H

#include <cstddef>
#include <indiccdchip.h>


/**
 * @brief Wrapper method to enable mockup tests classes for a CCDChip.
 * The indi library most methods are private and non-virtual making it almost impossible to make mockup classes for unit testing.
 * This class is used by the raw12pipeline and unit-tests to access the frame buffer for storing the captured image.
 */
class ChipWrapper
{
public:
    ChipWrapper(INDI::CCDChip *c=nullptr) : chip(c)
    {
    }

    virtual int getFrameBufferSize()  {
        return chip->getFrameBufferSize();
    }

    virtual uint8_t *getFrameBuffer() {
        return chip->getFrameBuffer();
    }

    virtual int getSubX() { return chip->getSubX(); }
    virtual int getSubY() { return chip->getSubY(); }
    virtual int getSubW() { return chip->getSubW(); }
    virtual int getSubH() { return chip->getSubH(); }
    virtual int getXRes() { return chip->getXRes(); }
    virtual int getYRes() { return chip->getYRes(); }

private:
    INDI::CCDChip *chip {nullptr};
};

#endif // CHIPWRAPPER_H
