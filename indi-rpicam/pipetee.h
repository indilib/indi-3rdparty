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
