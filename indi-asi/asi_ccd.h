/*
    ASI CCD Driver

    Copyright (C) 2015-2021 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)

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

#pragma once

#include "asi_base.h"

class ASICCD : public ASIBase
{
    public:
        explicit ASICCD(const ASI_CAMERA_INFO &camInfo, const std::string &cameraName,
                        const std::string &serialNumber);
    protected:
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    private:
        void loadNicknames();
        void saveNicknames();
        const std::string NICKNAME_FILE = "/.indi/ZWONicknames.xml";
        std::map<std::string, std::string> mNicknames;
};
