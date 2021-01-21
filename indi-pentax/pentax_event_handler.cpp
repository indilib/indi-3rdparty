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

#include "pentax_event_handler.h"



const char * getFormatFileExtension(ImageFormat format) {
    if (format==ImageFormat::JPEG) {
        return "jpg";
    }
    else if (format==ImageFormat::DNG) {
        return "raw";
    }
    else {
        return "pef";
    }
}

PentaxEventHandler::PentaxEventHandler(PentaxCCD *driver) {
    this->driver = driver;
}

const char * PentaxEventHandler::getDeviceName() {
    return driver->getDeviceName();
}

void PentaxEventHandler::imageStored(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const CameraImage>& image)
{
    INDI_UNUSED(sender);

    if (driver->transferFormatS[1].s != ISS_ON) {
        uint8_t * memptr = driver->PrimaryCCD.getFrameBuffer();
        size_t memsize = 0;
        int naxis = 2, w = 0, h = 0, bpp = 8;

        //write image to file
        std::ofstream o;
        char filename[32] = "/tmp/indi_pentax_";
        strcat(filename,image->getName().c_str());
        o.open(filename, std::ofstream::out | std::ofstream::binary);
        Response response = image->getData(o);
        o.close();
        if (response.getResult() == Result::Ok) {
            LOGF_DEBUG("Temp Image path: %s",filename);
        } else {
            for (const auto& error : response.getErrors()) {
                LOGF_ERROR("Error Code: %d (%s)", static_cast<int>(error->getCode()), error->getMessage().c_str());
            }
            return;
        }

        //convert it for image buffer
        if (image->getFormat()==ImageFormat::JPEG) {
            if (read_jpeg(filename, &memptr, &memsize, &naxis, &w, &h))
            {
                LOG_ERROR("Exposure failed to parse jpeg.");
                return;
            }
            LOGF_DEBUG("read_jpeg: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d)", memsize, naxis, w, h, bpp);

            driver->bufferIsBayered = false;
        }
        else {
            char bayer_pattern[8] = {};

            if (read_libraw(filename, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
            {
                LOG_ERROR("Exposure failed to parse raw image.");
                return;
            }

            LOGF_DEBUG("read_libraw: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d) bayer pattern (%s)",memsize, naxis, w, h, bpp, bayer_pattern);

            driver->bufferIsBayered = true;
        }

        driver->PrimaryCCD.setImageExtension("fits");

        if (driver->PrimaryCCD.getSubW() != 0 && (w > driver->PrimaryCCD.getSubW() || h > driver->PrimaryCCD.getSubH()))
            LOGF_WARN("Camera image size (%dx%d) is different than requested size (%d,%d). Purging configuration and updating frame size to match camera size.", w, h, driver->PrimaryCCD.getSubW(), driver->PrimaryCCD.getSubH());

        driver->PrimaryCCD.setFrame(0, 0, w, h);
        driver->PrimaryCCD.setFrameBuffer(memptr);
        driver->PrimaryCCD.setFrameBufferSize(memsize, false);
        driver->PrimaryCCD.setResolution(w, h);
        driver->PrimaryCCD.setNAxis(naxis);
        driver->PrimaryCCD.setBPP(bpp);

        //(re)move image file
        if (driver->preserveOriginalS[1].s == ISS_ON) {
            char ts[32];
            struct tm * tp;
            time_t t = image->getDateTime();
            tp = localtime(&t);
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
            std::string prefix = driver->getUploadFilePrefix();
            prefix = std::regex_replace(prefix, std::regex("XXX"), string(ts));
            char newname[255];
            snprintf(newname, 255, "%s.%s",prefix.c_str(),getFormatFileExtension(image->getFormat()));
            if (std::rename(filename, newname)) {
                LOGF_ERROR("File system error prevented saving original image to %s.  Saved to %s instead.", newname,filename);
            }
            else {
                LOGF_INFO("Saved original image to %s.", newname);
            }
        }
        else {
            std::remove(filename);
        }
    }
    else {
        driver->PrimaryCCD.setImageExtension(getFormatFileExtension(image->getFormat()));

        stringstream img;
        image->getData(img);
        driver->PrimaryCCD.setFrameBufferSize(image->getSize());
        char * memptr = (char *)driver->PrimaryCCD.getFrameBuffer();
        img.read(memptr,image->getSize());
        driver->PrimaryCCD.setFrameBuffer((unsigned char *)memptr);

    }

    LOG_INFO("Copied to frame buffer.");
}

void PentaxEventHandler::liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const unsigned char>& liveViewFrame, uint64_t frameSize)
{
    INDI_UNUSED(sender);
    std::unique_lock<std::mutex> ccdguard(driver->ccdBufferLock);
    driver->Streamer->newFrame(liveViewFrame.get(), frameSize);
    ccdguard.unlock();
}

void PentaxEventHandler::deviceDisconnected (const std::shared_ptr< const CameraDevice > &sender, DeviceInterface inf) {
    INDI_UNUSED(sender);
    INDI_UNUSED(inf);
    if (driver->Disconnect()) {
        driver->setConnected(false, IPS_IDLE);
        driver->updateProperties();
    }
}

void PentaxEventHandler::captureSettingsChanged(const std::shared_ptr<const CameraDevice>& sender, const std::vector<std::shared_ptr<const CaptureSetting>>& newSettings) {
    INDI_UNUSED(sender);
    INDI_UNUSED(newSettings);
    driver->getCaptureSettingsState();
    driver->deleteCaptureSwitches();
    driver->buildCaptureSwitches();
}
