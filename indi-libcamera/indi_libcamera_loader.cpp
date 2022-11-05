/*
    INDI LibCamera Driver Loader

    Copyright (C) 2022 Jasem Mutlaq (mutlaqja@ikarustech.com)

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


#include "indi_libcamera.h"
#include <unistd.h>
#include <pwd.h>

#include <map>
static class Loader
{
        std::map<int, std::shared_ptr<INDILibCamera>> cameras;
    public:
        Loader()
        {
            load();
        }

    public:
        static size_t getCountOfConnectedCameras()
        {
            //return size_t(std::max(_ASIGetNumOfConnectedCameras(), 0));
            return 1;
        }


    public:
        void load()
        {
            auto usedCameras = std::move(cameras);

            UniqueName uniqueName(usedCameras);

            //            for(const auto &cameraInfo : getConnectedCameras())
            //            {
            //                int id = cameraInfo.CameraID;

            //                // camera already created
            //                if (usedCameras.find(id) != usedCameras.end())
            //                {
            //                    std::swap(cameras[id], usedCameras[id]);
            //                    continue;
            //                }

            //                INDILibCamera *camera = new INDILibCamera();
            //                cameras[id] = std::shared_ptr<INDILibCamera>(camera);
            //            }
        }

    public:
        class UniqueName
        {
                std::map<std::string, bool> used;
            public:
                UniqueName() = default;
                UniqueName(const std::map<int, std::shared_ptr<INDILibCamera>> &usedCameras)
                {
                    for (const auto &camera : usedCameras)
                        used[camera.second->getDeviceName()] = true;
                }

                //                std::string make(const ASI_CAMERA_INFO &cameraInfo)
                //                {
                //                    std::string cameraName = "LibCamera " + std::string(cameraInfo.Name + 4);
                //                    std::string uniqueName = cameraName;

                //                    for (int index = 0; used[uniqueName] == true; )
                //                        uniqueName = cameraName + " " + std::to_string(++index);

                //                    used[uniqueName] = true;
                //                    return uniqueName;
                //                }
        };
} loader;

