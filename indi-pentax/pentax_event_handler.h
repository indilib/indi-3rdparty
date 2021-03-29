/*
 Pentax CCD Driver for Indi (using Ricoh Camera SDK)
 Copyright (C) 2020 Karl Rees

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

#ifndef PENTAXEVENTLISTENER_H
#define PENTAXEVENTLISTENER_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <ricoh_camera_sdk.hpp>
#pragma GCC diagnostic pop

#include <stream/streammanager.h>
#include <regex>

#include "gphoto_readimage.h"

#include "pentax_ccd.h"


using namespace Ricoh::CameraController;

class PentaxCCD;

class PentaxEventHandler : public CameraEventListener
{
public:
    PentaxEventHandler(PentaxCCD *driver);
    PentaxCCD *driver;

    const char *getDeviceName(); //so we can use the logger

    void imageStored(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const CameraImage>& image) override;

    void liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const unsigned char>& liveViewFrame, uint64_t frameSize) override;

    void deviceDisconnected (const std::shared_ptr< const CameraDevice > &sender, DeviceInterface inf);

    void captureSettingsChanged(const std::shared_ptr<const CameraDevice> &sender, const std::vector<std::shared_ptr<const CaptureSetting> > &newSettings);
};

#endif // PENTAXEVENTLISTENER_H
