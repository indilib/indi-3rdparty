/*
    PlayerOne CCD Driver

    Copyright (C) 2021 Hiroshi Saito (hiro3110g@gmail.com)

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

#include "indipropertyswitch.h"
#include "playerone_base.h"

#include <map>

#define PLAYERONE_PREFIX    "PlayerOne "

class POASingleCamera : public POABase
{
    public:
        explicit POASingleCamera();

        virtual const char *getDefaultName() override;

        virtual bool Connect() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;

        bool ISNewSwitch(const char *dev, const char *name, ISState * states, char *names[], int n) override;

        // PlayerOne Camera List management
        bool loadCamerasList();
        bool saveCamerasList();

        // Config
        bool initCameraFromConfig();
        static size_t getCountOfConnectedCameras();
        static std::vector<POACameraProperties> getConnectedCameras();

        // Utility
        std::string GetHomeDirectory() const;

    private:
        /** Additional Properties to INDI::CCD */
        INDI::PropertySwitch  CamerasSP {0};
    private:
        const std::string CamerasListFile;
        std::map<std::string, std::string> m_ConfigCameras;
        bool m_ConfigCameraFound {false};
};
