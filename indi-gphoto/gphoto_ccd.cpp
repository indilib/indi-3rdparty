/*
    Driver type: GPhoto Camera INDI Driver

    Copyright (C) 2009 Geoffrey Hausheer
    Copyright (C) 2013-2024 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

#include "gphoto_ccd.h"

#include "config.h"
#include "gphoto_driver.h"
#include "gphoto_readimage.h"

#include <algorithm>
#include <stream/streammanager.h>

#include <sharedblob.h>
#include <deque>
#include <memory>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FOCUS_TAB    "Focus"
#define MAX_DEVICES  5 /* Max device cameraCount */
#define FOCUS_TIMER  50
#define MAX_RETRIES  3

extern char * __progname;


typedef struct
{
    const char * exec;
    const char * driver;
    const char * model;
} CamDriverInfo;

static CamDriverInfo camInfos[] = { { "indi_gphoto_ccd", "GPhoto CCD", "GPhoto" },
    { "indi_canon_ccd", "Canon DSLR", "Canon" },
    { "indi_nikon_ccd", "Nikon DSLR", "Nikon" },
    { "indi_pentax_ccd", "Pentax DSLR", "Pentax" },
    { "indi_sony_ccd", "Sony DSLR", "Sony" },
    { "indi_fuji_ccd", "Fuji DSLR", "Fuji" },
    { nullptr, nullptr, nullptr }
};

static class Loader
{
    public:
        std::deque<std::unique_ptr<GPhotoCCD>> cameras;
        GPContext *context;

    public:
        Loader()
            : context(gp_context_new())
        {
            // Let's just create one camera for now
            if (!strcmp(__progname, "indi_gphoto_ccd"))
            {
                cameras.push_back(std::unique_ptr<GPhotoCCD>(new GPhotoCCD()));
                return;
            }

            CameraList * list;
            /* Detect all the cameras that can be autodetected... */
            int ret = gp_list_new(&list);
            if (ret < GP_OK)
            {
                // Use Legacy Mode
                IDLog("Failed to initialize list in libgphoto2\n");
                return;
            }

            const char * model, *port;
            gp_list_reset(list);
            int availableCameras = gp_camera_autodetect(list, context);
            /* Now open all cameras we autodected for usage */
            IDLog("Number of cameras detected: %d.\n", availableCameras);

            if (availableCameras == 0)
            {
                IDLog("Failed to detect any cameras. Check power and make sure camera is not mounted by other programs "
                      "and try again.\n");
                // Use Legacy Mode
#if 0
                IDLog("No cameras detected. Using legacy mode...");
                cameras.push_back(std::unique_ptr<GPhotoCCD>(new GPhotoCCD()));
#endif
                return;
            }

            int cameraIndex = 0;

            std::vector<std::string> cameraNames;

            while (availableCameras > 0)
            {
                gp_list_get_name(list, cameraIndex, &model);
                gp_list_get_value(list, cameraIndex, &port);

                IDLog("Detected camera model %s on port %s\n", model, port);

                cameraIndex++;
                availableCameras--;

                // If we're NOT using the Generic INDI GPhoto drievr
                // then let's search for multiple cameras
                if (strcmp(__progname, "indi_gphoto_ccd"))
                {
                    char prefix[MAXINDINAME];
                    char name[MAXINDINAME];
                    bool modelFound = false;

                    for (int j = 0; camInfos[j].exec != nullptr; j++)
                    {
                        if (strstr(model, camInfos[j].model))
                        {
                            strncpy(prefix, camInfos[j].driver, MAXINDINAME);

                            // If if the model was already registered for a prior camera in case we are using
                            // two identical models
                            if (std::find(cameraNames.begin(), cameraNames.end(), camInfos[j].model) == cameraNames.end())
                                snprintf(name, MAXINDIDEVICE, "%s %s", prefix, model + strlen(camInfos[j].model) + 1);
                            else
                                snprintf(name, MAXINDIDEVICE, "%s %s %d", prefix, model + strlen(camInfos[j].model) + 1,
                                         static_cast<int>(std::count(cameraNames.begin(), cameraNames.end(), camInfos[j].model)) + 1);

                            IDLog("Creating a new driver with model %s on port %s\n", model, port);
                            std::unique_ptr<GPhotoCCD> camera(new GPhotoCCD(model, port));
                            camera->setDeviceName(name);
                            cameras.push_back(std::move(camera));

                            modelFound = true;
                            // Store camera model in list to check for duplicates
                            cameraNames.push_back(camInfos[j].model);
                            break;
                        }
                    }

                    if (modelFound == false)
                    {
                        IDLog("Failed to find model %s in supported cameras.\n", model);
                        // If we are no cameras left
                        // Let us use the generic model name
                        // This is a libgphoto2 bug for some cameras which model does not correspond
                        // to the actual make of the camera but rather a generic class designation is given (e.g. PTP USB Camera)
                        if (availableCameras == 0)
                        {
                            IDLog("Falling back to generic name.\n");
                            for (int j = 0; camInfos[j].exec != nullptr; j++)
                            {
                                if (!strcmp(camInfos[j].exec, me))
                                {
                                    snprintf(name, MAXINDIDEVICE, "%s", camInfos[j].model);
                                    std::unique_ptr<GPhotoCCD> camera(new GPhotoCCD(model, port));
                                    camera->setDeviceName(name);
                                    cameras.push_back(std::move(camera));
                                }
                            }
                        }
                    }
                }
                else
                {
                    cameras.push_back(std::unique_ptr<GPhotoCCD>(new GPhotoCCD(model, port)));
                }
            }
        }

        ~Loader()
        {
            // TODO free GPContext
        }
} loader;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GPhotoCCD::GPhotoCCD() : FI(this)
{
    memset(model, 0, MAXINDINAME);
    memset(port, 0, MAXINDINAME);

    gphotodrv        = nullptr;
    frameInitialized = false;
    on_off[0]        = strdup("On");
    on_off[1]        = strdup("Off");

    setVersion(INDI_GPHOTO_VERSION_MAJOR, INDI_GPHOTO_VERSION_MINOR);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GPhotoCCD::GPhotoCCD(const char * model, const char * port) : FI(this)
{
    strncpy(this->port, port, MAXINDINAME);
    strncpy(this->model, model, MAXINDINAME);

    gphotodrv        = nullptr;
    frameInitialized = false;
    on_off[0]        = strdup("On");
    on_off[1]        = strdup("Off");

    setVersion(INDI_GPHOTO_VERSION_MAJOR, INDI_GPHOTO_VERSION_MINOR);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GPhotoCCD::~GPhotoCCD()
{
    free(on_off[0]);
    free(on_off[1]);
    expTID = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * GPhotoCCD::getDefaultName()
{
    return "GPhoto CCD";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::initProperties()
{
    // For now let's set name to default name. In the future, we need to to support multiple devices per one driver
    if (*getDeviceName() == '\0')
        strncpy(name, getDefaultName(), MAXINDINAME);
    else
        strncpy(name, getDeviceName(), MAXINDINAME);

    setDeviceName(this->name);

    // Init parent properties first
    INDI::CCD::initProperties();

    FI::initProperties(FOCUS_TAB);

    PortTP[0].fill("PORT", "Port", port);
    PortTP.fill(getDeviceName(), "DEVICE_PORT", "Shutter Release", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
    PortTP.load();
    // In case port is empty, always revert back to the detected port
    if (PortTP[0].isEmpty())
        PortTP[0].setText(port);

    MirrorLockNP[0].fill("MIRROR_LOCK_SECONDS", "Seconds", "%1.0f", 0, 10, 1, 0);
    MirrorLockNP.fill(getDeviceName(), "MIRROR_LOCK", "Mirror Lock", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    MirrorLockNP.load();

    ISOSP.fill(getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    ExposurePresetSP.fill(getDeviceName(), "CCD_EXPOSURE_PRESETS", "Presets", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                          IPS_IDLE);

    AutoFocusSP[0].fill("Set", "Set", ISS_OFF);
    AutoFocusSP.fill(getDeviceName(), "Auto Focus", "Auto Focus", FOCUS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    const auto isNikon = strstr(getDeviceName(), "Nikon") != nullptr;

    // Nikon should use SD card by default
    CaptureTargetSP[CAPTURE_INTERNAL_RAM].fill("RAM", "RAM", ISS_ON);
    CaptureTargetSP[CAPTURE_SD_CARD].fill("SD Card", "SD Card", ISS_OFF);
    CaptureTargetSP.fill(getDeviceName(), "CCD_CAPTURE_TARGET", "Capture Target", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                         IPS_IDLE);
    CaptureTargetSP.load();

    SDCardImageSP[SD_CARD_SAVE_IMAGE].fill("Save", "Save", ISS_ON);
    SDCardImageSP[SD_CARD_DELETE_IMAGE].fill("Delete", "Delete", ISS_OFF);
    SDCardImageSP[SD_CARD_IGNORE_IMAGE].fill("Ignore", "Ignore", ISS_OFF);
    SDCardImageSP.fill(getDeviceName(), "CCD_SD_CARD_ACTION", "SD Image", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Download Timeout
    DownloadTimeoutNP[0].fill("VALUE", "Seconds", "%.f", 0, 300, 30, 60);
    DownloadTimeoutNP.fill(getDeviceName(), "CCD_DOWNLOAD_TIMEOUT", "Download Timeout", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    DownloadTimeoutNP.load();

    // Nikon should have force bulb off by default.
    ForceBULBSP[INDI_ENABLED].fill("On", "On", isNikon ? ISS_OFF : ISS_ON);
    ForceBULBSP[INDI_DISABLED].fill("Off", "Off", isNikon ? ISS_ON : ISS_OFF);
    ForceBULBSP.fill(getDeviceName(), "CCD_FORCE_BLOB", "Force BULB", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Upload File
    UploadFileTP[0].fill("PATH", "Path", nullptr);
    UploadFileTP.fill(getDeviceName(), "CCD_UPLOAD_FILE", "Upload File", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    // Most cameras have this by default, so let's set it as default.
    BayerTP[2].setText("RGGB");

    SetCCDCapability(CCD_CAN_SUBFRAME | CCD_CAN_BIN | CCD_CAN_ABORT | CCD_HAS_BAYER | CCD_HAS_STREAMING);

    Streamer->setStreamingExposureEnabled(false);

#if 0
    FI::SetCapability(FOCUSER_HAS_VARIABLE_SPEED);
    FocusSpeedN[0].min   = 0;
    FocusSpeedN[0].max   = 3;
    FocusSpeedN[0].step  = 1;
    FocusSpeedN[0].value = 3;
#endif

    FI::SetCapability(FOCUSER_CAN_REL_MOVE);

    /* JM 2014-05-20 Make PrimaryCCD.ImagePixelSizeNP writable since we can't know for now the pixel size and bit depth from gphoto */
    PrimaryCCD.getCCDInfo().setPermission(IP_RW);

    setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE);

    gphoto_set_debug(getDeviceName());
    gphoto_read_set_debug(getDeviceName());

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::ISGetProperties(const char * dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineProperty(PortTP);

    if (isConnected())
        return;

    // Read Image Info if we have not connected yet.
    double pixel = 0, pixel_x = 0, pixel_y = 0;
    IUGetConfigNumber(getDeviceName(), "CCD_INFO", "CCD_PIXEL_SIZE", &pixel);
    IUGetConfigNumber(getDeviceName(), "CCD_INFO", "CCD_PIXEL_SIZE_X", &pixel_x);
    IUGetConfigNumber(getDeviceName(), "CCD_INFO", "CCD_PIXEL_SIZE_Y", &pixel_y);

    auto nvp = PrimaryCCD.getCCDInfo();

    if (!nvp.isValid())
        return;

    // Load the necessary pixel size information
    // The maximum resolution and bits per pixel depend on the capture itself.
    // while the pixel size data remains constant.
    if (pixel > 0)
        nvp[INDI::CCDChip::CCD_PIXEL_SIZE].setValue(pixel);
    if (pixel_x > 0)
        nvp[INDI::CCDChip::CCD_PIXEL_SIZE_X].setValue(pixel_x);
    if (pixel_y > 0)
        nvp[INDI::CCDChip::CCD_PIXEL_SIZE_Y].setValue(pixel_y);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (ExposurePresetSP.count() > 0)
            defineProperty(ExposurePresetSP);
        if (ISOSP.count() > 0)
            defineProperty(ISOSP);

        defineProperty(AutoFocusSP);

        if (m_CanFocus)
            FI::updateProperties();

        if (CaptureTargetSP.getState() == IPS_OK)
        {
            defineProperty(CaptureTargetSP);
        }

        defineProperty(SDCardImageSP);

        imageBP = getBLOB("CCD1");

        // Dummy values until first capture is done
        //SetCCDParams(1280, 1024, 8, 5.4, 5.4);

        if (isSimulation() == false)
        {
            ShowExtendedOptions();

            if (strstr(gphoto_get_manufacturer(gphotodrv), "Canon"))
                defineProperty(MirrorLockNP);
        }

        if (isSimulation())
            isTemperatureSupported = false;
        else
            isTemperatureSupported = gphoto_supports_temperature(gphotodrv);

        if (isTemperatureSupported)
        {
            TemperatureNP.setPermission(IP_RO);
            defineProperty(TemperatureNP);
        }

        defineProperty(ForceBULBSP);
        defineProperty(DownloadTimeoutNP);
    }
    else
    {
        if (ExposurePresetSP.count() > 0)
            deleteProperty(ExposurePresetSP);
        if (ISOSP.count() > 0)
            deleteProperty(ISOSP);

        deleteProperty(MirrorLockNP);
        deleteProperty(AutoFocusSP);

        if (m_CanFocus)
            FI::updateProperties();

        if (CaptureTargetSP.getState() != IPS_IDLE)
        {
            deleteProperty(CaptureTargetSP);
        }

        if (isTemperatureSupported)
            deleteProperty(TemperatureNP);

        deleteProperty(SDCardImageSP);

        deleteProperty(ForceBULBSP);
        deleteProperty(DownloadTimeoutNP);

        HideExtendedOptions();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PortTP.isNameMatch(dev))
        {
            auto previousPort = PortTP[0].getText();
            PortTP.setState(IPS_OK);
            PortTP.update(texts, names, n);
            PortTP.apply();

            // Port changes requires a driver restart.
            if (!previousPort || strcmp(previousPort, PortTP[0].getText()))
            {
                saveConfig(PortTP);
                LOG_INFO("Please restart the driver for this change to have effect.");
            }
            return true;
        }
        else if (UploadFileTP.isNameMatch(name))
        {
            struct stat buffer;
            if (stat (texts[0], &buffer) == 0)
            {
                UploadFileTP.update(texts, names, n);
                UploadFileTP.setState(IPS_OK);
            }
            else
            {
                LOGF_ERROR("File %s does not exist. Check path again.", texts[0]);
                UploadFileTP.setState(IPS_ALERT);
            }

            UploadFileTP.apply();
            return true;
        }
        else if (CamOptions.find(name) != CamOptions.end())
        {
            cam_opt * opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_TEXT)
            {
                LOGF_ERROR("ERROR: Property '%s'is not a string", name);
                return false;
            }
            if (opt->widget->readonly)
            {
                LOGF_WARN("WARNING: Property %s is read-only", name);
                IDSetText(&opt->prop.text, nullptr);
                return false;
            }

            if (IUUpdateText(&opt->prop.text, texts, names, n) < 0)
                return false;
            char *text = texts[0];
            char buf[256];
            if(strcmp("eoszoomposition", name) == 0)
            {
                int x = 0, y = 0;
                LOGF_DEBUG("%s %s", name, text);
                sscanf(text, "%d,%d", &x, &y);
                x *= 5;
                y *= 5;
                sprintf(buf, "%d,%d", x, y);
                text = buf;
                LOGF_DEBUG("%s adjusted %s %s (%d,%d)", name, text, buf, x, y);
            }
            gphoto_set_widget_text(gphotodrv, opt->widget, text);
            opt->prop.num.s = IPS_OK;
            IDSetText(&opt->prop.text, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ISOSP.isNameMatch(name))
        {
            if (!ISOSP.update(states, names, n))
                return false;

            for (size_t i = 0; i < ISOSP.count(); i++)
            {
                if (ISOSP[i].getState() == ISS_ON)
                {
                    if (isSimulation() == false)
                        gphoto_set_iso(gphotodrv, i);
                    ISOSP.setState(IPS_OK);
                    ISOSP.apply();
                    saveConfig(ISOSP);
                    return true;
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////
        // Force BULB
        // This force driver to _always_ capture in bulb mode and never use predefined exposures unless the exposures are less
        // than a second.
        ///////////////////////////////////////////////////////////////////////////////////////////////
        if (ForceBULBSP.isNameMatch(name))
        {
            if (!ForceBULBSP.update(states, names, n))
                return false;

            ForceBULBSP.setState(IPS_OK);
            if (ForceBULBSP[INDI_ENABLED].getState() == ISS_ON)
            {
                if (isSimulation() == false)
                    gphoto_force_bulb(gphotodrv, true);
                LOG_INFO("Force BULB is enabled. All expsures shall be captured in BULB mode except for subsecond captures.");
            }
            else
            {
                if (isSimulation() == false)
                    gphoto_force_bulb(gphotodrv, false);
                LOG_INFO("Force BULB is disabled. Exposures shall utilize camera predefined exposures time first before attempting BULB.");
            }

            ForceBULBSP.apply();
            saveConfig(ForceBULBSP);
            return true;
        }

        if (ExposurePresetSP.isNameMatch(name))
        {
            if (!ExposurePresetSP.update(states, names, n))
                return false;

            ExposurePresetSP.setState(IPS_OK);
            ExposurePresetSP.apply();

            auto currentSwitch = ExposurePresetSP.findOnSwitch();
            if (strcmp(currentSwitch->label, "bulb"))
            {
                LOGF_INFO("Preset %s seconds selected.", currentSwitch->label);

                float duration;
                int num, denom;
                if (sscanf(currentSwitch->label, "%d/%d", &num, &denom) == 2)
                {
                    duration = (static_cast<double>(num)) / (static_cast<double>(denom));
                    StartExposure(duration);
                }
                else if ((duration = strtod(currentSwitch->label, nullptr)))
                {
                    // Fuji returns long exposure values ( > 60s) with m postfix
                    if (currentSwitch->label[strlen(currentSwitch->label) - 1] == 'm')
                    {
                        duration *= 60;
                    }
                    StartExposure(duration);
                }
            }

            return true;
        }

        // Autofocus
        if (AutoFocusSP.isNameMatch(name))
        {
            AutoFocusSP.reset();
            char errMsg[MAXRBUF] = {0};
            if (gphoto_auto_focus(gphotodrv, errMsg) == GP_OK)
                AutoFocusSP.setState(IPS_OK);
            else
            {
                AutoFocusSP.setState(IPS_ALERT);
                LOGF_ERROR("%s", errMsg);
            }

            AutoFocusSP.apply();
            return true;
        }


        // Capture target
        if (CaptureTargetSP.isNameMatch(name))
        {
            auto onSwitch =  IUFindOnSwitchName(states, names, n);
            int captureTarget = (!strcmp(onSwitch,
                                         CaptureTargetSP[CAPTURE_INTERNAL_RAM].getName()) ? CAPTURE_INTERNAL_RAM : CAPTURE_SD_CARD);
            int ret = gphoto_set_capture_target(gphotodrv, captureTarget);
            if (ret == GP_OK)
            {
                CaptureTargetSP.setState(IPS_OK);
                CaptureTargetSP.update(states, names, n);
                LOGF_INFO("Capture target set to %s", (captureTarget == CAPTURE_INTERNAL_RAM) ? "Internal RAM" : "SD Card");
                saveConfig(CaptureTargetSP);
            }
            else
            {
                CaptureTargetSP.setState(IPS_ALERT);
                LOGF_ERROR("Failed to set capture target set to %s", (captureTarget == CAPTURE_INTERNAL_RAM) ? "Internal RAM" : "SD Card");
            }

            CaptureTargetSP.apply();
            return true;
        }

        if (SDCardImageSP.isNameMatch(name))
        {
            SDCardImageSP.update(states, names, n);
            SDCardImageSP.setState(IPS_OK);
            const auto index = SDCardImageSP.findOnSwitchIndex();
            switch (index)
            {
                case SD_CARD_SAVE_IMAGE:
                    LOG_INFO("Images downloaded from camera will saved in the camera internal storage.");
                    break;
                case SD_CARD_DELETE_IMAGE:
                    LOG_INFO("Images downloaded from camera will not be stored on the camera internal storage.");
                    break;
                case SD_CARD_IGNORE_IMAGE:
                    LOG_INFO("Images should only remain in the camera internal storage and will not be downloaded at all.");

                    // Upload mode should always be local, no images uploaded.
                    if (UploadSP[UPLOAD_LOCAL].getState() != ISS_ON)
                    {
                        UploadSP.reset();
                        UploadSP[UPLOAD_LOCAL].setState(ISS_ON);
                        UploadSP.setState(IPS_OK);
                        UploadSP.apply();
                    }

                    // Capture target should always be SD card.
                    if (CaptureTargetSP[CAPTURE_SD_CARD].getState() != ISS_ON)
                    {
                        CaptureTargetSP.reset();
                        CaptureTargetSP.setState(IPS_OK);
                        CaptureTargetSP[CAPTURE_SD_CARD].setState(ISS_ON);
                        gphoto_set_capture_target(gphotodrv, CAPTURE_SD_CARD);
                        CaptureTargetSP.apply();
                    }
                    break;
            }

            gphoto_handle_sdcard_image(gphotodrv, static_cast<CameraImageHandling>(index));
            SDCardImageSP.apply();
            saveConfig(SDCardImageSP);
            return true;
        }

        if (strstr(name, "FOCUS"))
        {
            return FI::processSwitch(dev, name, states, names, n);
        }

        if (CamOptions.find(name) != CamOptions.end())
        {
            cam_opt * opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_RADIO && opt->widget->type != GP_WIDGET_MENU &&
                    opt->widget->type != GP_WIDGET_TOGGLE)
            {
                LOGF_ERROR("ERROR: Property '%s'is not a switch (%d)", name, opt->widget->type);
                return false;
            }

            if (opt->widget->readonly)
            {
                LOGF_WARN("WARNING: Property %s is read-only", name);
                IDSetSwitch(&opt->prop.sw, nullptr);
                return false;
            }

            if (IUUpdateSwitch(&opt->prop.sw, states, names, n) < 0)
                return false;

            if (opt->widget->type == GP_WIDGET_TOGGLE)
            {
                gphoto_set_widget_num(gphotodrv, opt->widget, opt->item.sw[ON_S].s == ISS_ON);
            }
            else
            {
                for (int i = 0; i < opt->prop.sw.nsp; i++)
                {
                    if (opt->item.sw[i].s == ISS_ON)
                    {
                        gphoto_set_widget_num(gphotodrv, opt->widget, i);
                        break;
                    }
                }
            }

            opt->prop.sw.s = IPS_OK;
            IDSetSwitch(&opt->prop.sw, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);

        if (MirrorLockNP.isNameMatch(name))
        {
            MirrorLockNP.update(values, names, n);
            MirrorLockNP.setState(IPS_OK);
            MirrorLockNP.apply();
            saveConfig(MirrorLockNP);
            return true;
        }

        // Download Timeout
        if (DownloadTimeoutNP.isNameMatch(name))
        {
            DownloadTimeoutNP.update(values, names, n);
            DownloadTimeoutNP.setState(IPS_OK);
            DownloadTimeoutNP.apply();
            saveConfig(DownloadTimeoutNP);
            gphoto_set_download_timeout(gphotodrv, DownloadTimeoutNP[0].getValue());
            return true;
        }

        if (CamOptions.find(name) != CamOptions.end())
        {
            cam_opt * opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_RANGE)
            {
                LOGF_ERROR("ERROR: Property '%s'is not a string", name);
                return false;
            }
            if (opt->widget->readonly)
            {
                LOGF_WARN("WARNING: Property %s is read-only", name);
                return false;
            }
            if (IUUpdateNumber(&opt->prop.num, values, names, n) < 0)
                return false;
            gphoto_set_widget_num(gphotodrv, opt->widget, values[0]);
            opt->prop.num.s = IPS_OK;
            IDSetNumber(&opt->prop.num, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::Connect()
{
    int setidx;
    char ** options;
    int max_opts;
    const char * shutter_release_port = nullptr;
    LOGF_DEBUG("Mirror lock value: %f", MirrorLockNP[0].getValue());

    const auto port = PortTP[0].getText();
    // Do not set automatically detected USB device ids as the shutter port
    if (port && strlen(port) && strstr(port, "usb:") == nullptr)
    {
        shutter_release_port = port;
    }

    if (isSimulation() == false)
    {
        // If no port is specified, connect to first camera detected on bus
        if (port[0] == '\0')
            gphotodrv = gphoto_open(camera, loader.context, nullptr, nullptr, shutter_release_port);
        else
        {
            // Connect to specific model on specific USB device end point.
            gphotodrv = gphoto_open(camera, loader.context, model, port, shutter_release_port);
            // Otherwise, try to specify the model only without the USB device end point.
            if (gphotodrv == nullptr)
                gphotodrv = gphoto_open(camera, loader.context, model, nullptr, shutter_release_port);
        }
        if (gphotodrv == nullptr)
        {
            LOG_ERROR("Can not open camera: Power OK? If camera is auto-mounted as external disk "
                      "storage, please unmount it and disable auto-mount.");
            return false;
        }
    }

    if (isSimulation())
    {
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, true);
    }
    else
    {
        double min_exposure = 0.001, max_exposure = 3600;

        gphoto_get_minmax_exposure(gphotodrv, &min_exposure, &max_exposure);
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min_exposure, max_exposure, 1, true);
    }

    const char * fmts[] = { "Custom" };
    //setidx             = 0;
    max_opts           = 1;
    options            = const_cast<char **>(fmts);

    if (!isSimulation())
    {
        //setidx  = gphoto_get_format_current(gphotodrv);
        options = gphoto_get_formats(gphotodrv, &max_opts);
    }

    if (max_opts > 0)
    {
        for (int i = 0; i < max_opts; i++)
        {
            std::string label = options[i];
            if (label.find('+') != std::string::npos ||
                    label.find("sRAW") != std::string::npos ||
                    label.find("mRAW") != std::string::npos)
            {
                continue;
            }

            uint8_t index = CaptureFormatSP.size();
            auto isRAW = strcasestr(label.c_str(), "RAW") != nullptr;
            char name[MAXINDINAME] = {0};
            snprintf(name, MAXINDINAME, "FORMAT_%d", index + 1);
            CaptureFormat format = {name, label, 8, isRAW};
            addCaptureFormat(format);

            m_CaptureFormatMap[index] = i;
        }
    }

    const char * isos[] = { "100", "200", "400", "800" };
    setidx   = 0;
    max_opts = 4;
    options  = const_cast<char **>(isos);

    if (!isSimulation())
    {
        setidx  = gphoto_get_iso_current(gphotodrv);
        options = gphoto_get_iso(gphotodrv, &max_opts);
    }

    createSwitch(ISOSP, "ISO", options, max_opts, setidx);

    const char * exposureList[] =
    {
        "1/2000",
        "1/1000",
        "1/500",
        "1/200",
        "1/100",
        "1/50",
        "1/8",
        "1/4",
        "1/2",
        "1",
        "2",
        "5",
        "bulb"
    };
    setidx   = 0;
    max_opts = NARRAY(exposureList);
    options  = const_cast<char **>(exposureList);

    if (!isSimulation())
    {
        setidx   = 0;
        max_opts = 0;
        options  = gphoto_get_exposure_presets(gphotodrv, &max_opts);
    }

    if (max_opts > 0)
    {
        createSwitch(ExposurePresetSP, "EXPOSURE_PRESET", options, max_opts, setidx);
    }

    // Get Capture target
    int captureTarget = -1;

    if (!isSimulation() && gphoto_get_capture_target(gphotodrv, &captureTarget) == GP_OK)
    {
        const auto isNikon = strstr(getDeviceName(), "Nikon") != nullptr;
        // Nikon should be SD Card by default.
        if (captureTarget == 0 && isNikon)
        {
            gphoto_set_capture_target(gphotodrv, CAPTURE_SD_CARD);
            captureTarget = CAPTURE_SD_CARD;
        }
        CaptureTargetSP.reset();
        CaptureTargetSP[CAPTURE_INTERNAL_RAM].setState(captureTarget == 0 ? ISS_ON : ISS_OFF);
        CaptureTargetSP[CAPTURE_SD_CARD].setState(captureTarget == 1 ? ISS_ON : ISS_OFF);
        CaptureTargetSP.setState(IPS_OK);
    }

    if (isSimulation())
        m_CanFocus = false;
    else
        m_CanFocus = gphoto_can_focus(gphotodrv);

    LOGF_INFO("%s is online.", getDeviceName());

    if (!isSimulation() && gphoto_get_manufacturer(gphotodrv) && gphoto_get_model(gphotodrv))
    {
        LOGF_INFO("Detected %s Model %s.", gphoto_get_manufacturer(gphotodrv),
                  gphoto_get_model(gphotodrv));
    }

    frameInitialized = false;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::Disconnect()
{
    if (isSimulation())
        return true;
    gphoto_close(gphotodrv);
    gphotodrv        = nullptr;
    frameInitialized = false;
    LOGF_INFO("%s is offline.", getDeviceName());
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::StartExposure(float duration)
{
    if (PrimaryCCD.getPixelSizeX() == 0.0)
    {
        LOG_INFO("Please update the CCD Information in the Image Info section before "
                 "proceeding. The camera resolution shall be updated after the first exposure "
                 "is complete.");
        return false;
    }

    if (InExposure)
    {
        LOG_ERROR("GPhoto driver is already exposing.");
        return false;
    }

    /* start new exposure with last ExpValues settings.
     * ExpGo goes busy. set timer to read when done
     */

    if (isSimulation() == false)
        gphoto_set_format(gphotodrv, m_CaptureFormatMap[CaptureFormatSP.findOnSwitchIndex()]);

    // Microseconds
    uint32_t exp_us = static_cast<uint32_t>(ceil(duration * 1e6));

    PrimaryCCD.setExposureDuration(duration);

    if (MirrorLockNP[0].getValue() > 0)
        LOGF_INFO("Starting %g seconds exposure (+%g seconds mirror lock).", duration, MirrorLockNP[0].getValue());
    else
        LOGF_INFO("Starting %g seconds exposure.", duration);

    if (isSimulation() == false && gphoto_start_exposure(gphotodrv, exp_us, MirrorLockNP[0].getValue()) < 0)
    {
        LOG_ERROR("Error starting exposure");
        return false;
    }

    ExposureRequest = duration;
    gettimeofday(&ExpStart, nullptr);
    InExposure = true;

    SetTimer(getCurrentPollingPeriod());

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::AbortExposure()
{
    if (!isSimulation())
        gphoto_abort_exposure(gphotodrv);
    InExposure = false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    if (EncodeFormatSP[FORMAT_FITS].getState() != ISS_ON && EncodeFormatSP[FORMAT_XISF].getState() != ISS_ON)
    {
        LOG_ERROR("Subframing is only supported in FITS/XISF encode mode.");
        return false;
    }

    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::UpdateCCDBin(int hor, int ver)
{

    if(hor == 1 && ver == 1)
    {
        binning = false;
    }
    else
    {
        // only for fits output
        if (EncodeFormatSP[FORMAT_FITS].getState() != ISS_ON && EncodeFormatSP[FORMAT_XISF].getState() != ISS_ON)
        {
            LOG_ERROR("Binning is only supported in FITS/XISF transport mode.");
            return false;
        }

        binning = true;

    }

    return INDI::CCD::UpdateCCDBin(hor, ver);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double GPhotoCCD::CalcTimeLeft()
{
    struct timeval now, diff;
    gettimeofday(&now, nullptr);

    timersub(&now, &ExpStart, &diff);
    double timesince = diff.tv_sec + diff.tv_usec / 1000000.0;
    return (ExposureRequest - timesince);
}

void GPhotoCCD::TimerHit()
{
    if (isConnected() == false)
        return;

    if (InExposure)
    {
        int timerID   = -1;
        double timeleft = CalcTimeLeft();

        if (timeleft < 0)
            timeleft = 0;

        PrimaryCCD.setExposureLeft(timeleft);

        if (timeleft < 1.0)
        {
            if (timeleft > 0.25)
                timerID = SetTimer(timeleft * 900);
            else
            {
                PrimaryCCD.setExposureLeft(0);
                InExposure = false;
                // grab and save image
                bool rc = grabImage();
                if (rc == false)
                {
                    PrimaryCCD.setExposureFailed();
                }
            }

            if (isTemperatureSupported)
            {
                double cameraTemperature = static_cast<double>(gphoto_get_last_sensor_temperature(gphotodrv));
                if (fabs(cameraTemperature - TemperatureNP[0].getValue()) > 0.01)
                {
                    // Check if we are getting bogus temperature values and set property to alert
                    // unless it is already set
                    if (cameraTemperature < MINUMUM_CAMERA_TEMPERATURE)
                    {
                        if (TemperatureNP.getState() != IPS_ALERT)
                        {
                            TemperatureNP.setState(IPS_ALERT);
                            TemperatureNP.apply();
                        }
                    }
                    else
                    {
                        TemperatureNP.setState(IPS_OK);
                        TemperatureNP[0].setValue(cameraTemperature);
                        TemperatureNP.apply();
                    }
                }
            }
        }

        if (InExposure && timerID == -1)
            SetTimer(getCurrentPollingPeriod());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::UpdateExtendedOptions(void * p)
{
    GPhotoCCD * cam = static_cast<GPhotoCCD *>(p);
    cam->UpdateExtendedOptions();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::UpdateExtendedOptions(bool force)
{
    if (!expTID)
    {
        for (auto &option : CamOptions)
        {
            cam_opt * opt = option.second;

            if (force || gphoto_widget_changed(opt->widget))
            {
                gphoto_read_widget(opt->widget);
                UpdateWidget(opt);
            }
        }
    }

    optTID = IEAddTimer(1000, GPhotoCCD::UpdateExtendedOptions, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::grabImage()
{
    uint8_t * memptr = PrimaryCCD.getFrameBuffer();
    size_t memsize = 0;
    int naxis = 2, w = 0, h = 0, bpp = 8;
    const auto uploadFile = UploadFileTP[0].getText();

    if (SDCardImageSP[SD_CARD_IGNORE_IMAGE].getState() == ISS_ON)
    {
        PrimaryCCD.setFrameBufferSize(0);
        ExposureComplete(&PrimaryCCD);
        gphoto_read_exposure_fd(gphotodrv, -1);
    }
    else if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON || EncodeFormatSP[FORMAT_XISF].getState() == ISS_ON)
    {
        char filename[MAXRBUF] = "/tmp/indi_XXXXXX";
        const char *extension = "unknown";
        if (isSimulation())
        {
            if (uploadFile == nullptr || !uploadFile[0])
            {
                LOG_WARN("You must specify simulation file path under Options.");
                return false;
            }

            strncpy(filename, uploadFile, MAXRBUF);
            const char *found = strchr(filename, '.');
            if (found == nullptr)
            {
                LOGF_ERROR("Upload filename %s is invalid.", uploadFile);
                return false;
            }
            extension =  found + 1;
        }
        else
        {
            int fd = mkstemp(filename);
            int ret = gphoto_read_exposure_fd(gphotodrv, fd);
            if (ret != GP_OK || fd == -1)
            {
                if (fd == -1)
                    LOGF_ERROR("Exposure failed to save image. Cannot create temp file %s", tmpfile);
                else
                {
                    LOGF_ERROR("Exposure failed to save image... %s", gp_result_as_string(ret));
                    // As suggested on INDI forums, this result could be misleading.
                    if (ret == GP_ERROR_DIRECTORY_NOT_FOUND)
                        LOG_INFO("Make sure BULB switch is ON in the camera. Try setting AF switch to OFF.");
                }
                unlink(filename);
                return false;
            }

            extension = gphoto_get_file_extension(gphotodrv);
        }

        if (!strcmp(extension, "unknown"))
        {
            LOG_ERROR("Exposure failed.");
            return false;
        }

        // We're done exposing
        if (ExposureRequest > 3)
            LOG_INFO("Exposure done, downloading image...");

        if (strcasecmp(extension, "jpg") == 0 || strcasecmp(extension, "jpeg") == 0)
        {
            if (read_jpeg(filename, &memptr, &memsize, &naxis, &w, &h))
            {
                LOG_ERROR("Exposure failed to parse jpeg.");
                if (!isSimulation())
                    unlink(filename);
                return false;
            }

            LOGF_DEBUG("read_jpeg: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d)", memsize, naxis, w, h, bpp);

            SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
        }
        else
        {
            char bayer_pattern[8] = {};
            auto libraw_ok = false;

            // In case the file read operation fails due to some disk delay (unlikely)
            // Try again before giving up.
            for (int i = 0; i < 2; i++)
            {
                // On error, try again in 500ms
                if (read_libraw(filename, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
                    usleep(500000);
                else
                {
                    libraw_ok = true;
                    break;
                }
            }

            if (libraw_ok == false)
            {
                LOG_ERROR("Exposure failed to parse raw image.");
                if (!isSimulation())
                    unlink(filename);
                return false;
            }

            LOGF_DEBUG("read_libraw: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d) bayer pattern (%s)",
                       memsize, naxis, w, h, bpp, bayer_pattern);

            if (!isSimulation())
                unlink(filename);

            BayerTP[2].setText(bayer_pattern);
            BayerTP.apply();
            SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        }

        if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON)
            PrimaryCCD.setImageExtension("fits");
        else
            PrimaryCCD.setImageExtension("xisf");

        uint16_t subW = PrimaryCCD.getSubW();
        uint16_t subH = PrimaryCCD.getSubH();

        // If subframing is requested
        // If either axis is less than the image resolution
        // then we subframe, given the OTHER axis is within range as well.
        if ( (subW > 0 && subH > 0) && ((subW < w && subH <= h) || (subH < h && subW <= w)))
        {

            uint16_t subX = PrimaryCCD.getSubX();
            uint16_t subY = PrimaryCCD.getSubY();

            // Align all boundaries to be even
            // This should fix issues with subframed bayered images.
            //            subX -= subX % 2;
            //            subY -= subY % 2;
            //            subW -= subW % 2;
            //            subH -= subH % 2;

            int subFrameSize     = subW * subH * bpp / 8 * ((naxis == 3) ? 3 : 1);
            int oneFrameSize     = subW * subH * bpp / 8;

            int lineW  = subW * bpp / 8;

            LOGF_DEBUG("Subframing... subFrameSize: %d - oneFrameSize: %d - subX: %d - subY: %d - subW: %d - subH: %d",
                       subFrameSize, oneFrameSize,
                       subX, subY, subW, subH);

            if (naxis == 2)
            {
                // JM 2020-08-29: Using memmove since regions are overlaping
                // as proposed by Camiel Severijns on INDI forums.
                for (int i = subY; i < subY + subH; i++)
                    memmove(memptr + (i - subY) * lineW, memptr + (i * w + subX) * bpp / 8, lineW);
            }
            else
            {
                uint8_t * subR = memptr;
                uint8_t * subG = memptr + oneFrameSize;
                uint8_t * subB = memptr + oneFrameSize * 2;

                uint8_t * startR = memptr;
                uint8_t * startG = memptr + (w * h * bpp / 8);
                uint8_t * startB = memptr + (w * h * bpp / 8 * 2);

                for (int i = subY; i < subY + subH; i++)
                {
                    memcpy(subR + (i - subY) * lineW, startR + (i * w + subX) * bpp / 8, lineW);
                    memcpy(subG + (i - subY) * lineW, startG + (i * w + subX) * bpp / 8, lineW);
                    memcpy(subB + (i - subY) * lineW, startB + (i * w + subX) * bpp / 8, lineW);
                }
            }

            PrimaryCCD.setFrameBuffer(memptr);
            PrimaryCCD.setFrameBufferSize(memsize, false);
            PrimaryCCD.setResolution(w, h);
            PrimaryCCD.setFrame(subX, subY, subW, subH);
            PrimaryCCD.setNAxis(naxis);
            PrimaryCCD.setBPP(bpp);

            // binning if needed
            if(binning)
            {

                // binBayerFrame implemented since 1.9.4
#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 9 && INDI_VERSION_RELEASE >=4
                PrimaryCCD.binBayerFrame();
#else
                PrimaryCCD.binFrame();
#endif
            }

            ExposureComplete(&PrimaryCCD);

            // Restore old pointer and release memory
            //PrimaryCCD.setFrameBuffer(memptr);
            //PrimaryCCD.setFrameBufferSize(memsize, false);
            //delete [] (subframeBuf);
        }
        else
        {
            if (PrimaryCCD.getSubW() != 0 && (w > PrimaryCCD.getSubW() || h > PrimaryCCD.getSubH()))
                LOGF_WARN("Camera image size (%dx%d) is less than requested size (%d,%d). Purge configuration and update frame size to match camera size.",
                          w, h, PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

            PrimaryCCD.setFrameBuffer(memptr);
            PrimaryCCD.setFrameBufferSize(memsize, false);
            PrimaryCCD.setResolution(w, h);
            PrimaryCCD.setFrame(0, 0, w, h);
            PrimaryCCD.setNAxis(naxis);
            PrimaryCCD.setBPP(bpp);

            // binning if needed
            if(binning)
            {
                // binBayerFrame implemented since 1.9.4
#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 9 && INDI_VERSION_RELEASE >=4
                PrimaryCCD.binBayerFrame();
#else
                PrimaryCCD.binFrame();
#endif
            }

            ExposureComplete(&PrimaryCCD);
        }
    }

    // Read Native image AS IS
    else
    {
        if (isSimulation())
        {
            int fd = open(uploadFile, O_RDONLY);
            struct stat sb;

            // Get file size
            if (fstat(fd, &sb) == -1)
            {
                LOGF_ERROR("Error opening file %s: %s", uploadFile, strerror(errno));
                close(fd);
                return false;
            }

            // Copy file to memory using mmap
            memsize = sb.st_size;
            void *mmap_mem = mmap(nullptr, memsize, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mmap_mem == nullptr)
            {
                LOGF_ERROR("Error reading file %s: %s", uploadFile, strerror(errno));
                close(fd);
                return false;
            }

            // Guard CCD Buffer content until we finish copying mmap buffer to it
            std::unique_lock<std::mutex> guard(ccdBufferLock);
            // If CCD Buffer size is different, allocate memory to file size
            if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(memsize))
            {
                PrimaryCCD.setFrameBufferSize(memsize);
                memptr = PrimaryCCD.getFrameBuffer();
            }

            // Copy mmap buffer to ccd buffer
            memcpy(memptr, mmap_mem, memsize);

            // Release mmap memory
            munmap(mmap_mem, memsize);
            // Close file
            close(fd);
            // Set extension (eg. cr2..etc)
            PrimaryCCD.setImageExtension(strchr(uploadFile, '.') + 1);
            // We are ready to unlock
            guard.unlock();

        }
        else
        {
            int rc = gphoto_read_exposure(gphotodrv);
            if (rc != 0)
            {
                LOG_ERROR("Failed to expose.");
                if (strstr(gphoto_get_manufacturer(gphotodrv), "Canon") && MirrorLockNP[0].getValue() == 0.0)
                    DEBUG(INDI::Logger::DBG_WARNING,
                          "If your camera mirror lock is enabled, you must set a value for the mirror locking duration.");
                return false;
            }

            // We're done exposing
            if (ExposureRequest > 3)
                LOG_DEBUG("Exposure done, downloading image...");
            const char * gphotoFileData = nullptr;
            unsigned long gphotoFileSize = 0;
            gphoto_get_buffer(gphotodrv, &gphotoFileData, &gphotoFileSize);
            memsize = gphotoFileSize;
            // We copy the obtained memory pointer to avoid freeing some gphoto memory
            memptr = static_cast<uint8_t *>(IDSharedBlobRealloc(memptr, gphotoFileSize));
            if (memptr == nullptr)
                memptr = static_cast<uint8_t *>(IDSharedBlobAlloc(gphotoFileSize));
            if (memptr == nullptr)
            {
                LOG_ERROR("Failed to allocate memory to load file from camera.");
                PrimaryCCD.setExposureFailed();
                return false;
            }
            memcpy(memptr, gphotoFileData, gphotoFileSize);

            gphoto_get_dimensions(gphotodrv, &w, &h);

            PrimaryCCD.setImageExtension(gphoto_get_file_extension(gphotodrv));
            if (w > 0 && h > 0)
                PrimaryCCD.setFrame(0, 0, w, h);
            PrimaryCCD.setFrameBuffer(memptr);
            PrimaryCCD.setFrameBufferSize(memsize, false);
            if (w > 0 && h > 0)
                PrimaryCCD.setResolution(w, h);
            PrimaryCCD.setNAxis(naxis);
            PrimaryCCD.setBPP(bpp);

        }

        ExposureComplete(&PrimaryCCD);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::createSwitch(INDI::PropertySwitch &property, const char *baseName, char ** options, int max_opts,
                             int setidx)
{
    char sw_name[MAXINDINAME];
    char sw_label[MAXINDILABEL];
    ISState sw_state;

    property.resize(0);
    for (int i = 0; i < max_opts; i++)
    {
        snprintf(sw_name, MAXINDINAME, "%s%d", baseName, i);
        strncpy(sw_label, options[i], MAXINDILABEL);
        sw_state = (i == setidx) ? ISS_ON : ISS_OFF;

        INDI::WidgetSwitch node;
        node.fill(sw_name, sw_label, sw_state);
        property.push(std::move(node));
    }
    property.shrink_to_fit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ISwitch * GPhotoCCD::createLegacySwitch(const char * basestr, char ** options, int max_opts, int setidx)
{
    int i;
    ISwitch * sw     = static_cast<ISwitch *>(calloc(max_opts, sizeof(ISwitch)));
    ISwitch * one_sw = sw;

    char sw_name[MAXINDINAME] = {0};
    char sw_label[MAXINDILABEL] = {0};
    ISState sw_state;

    for (i = 0; i < max_opts; i++)
    {
        snprintf(sw_name, MAXINDINAME, "%s%d", basestr, i);
        strncpy(sw_label, options[i], MAXINDILABEL);
        sw_state = (i == setidx) ? ISS_ON : ISS_OFF;

        IUFillSwitch(one_sw++, sw_name, sw_label, sw_state);
    }
    return sw;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::UpdateWidget(cam_opt * opt)
{
    switch (opt->widget->type)
    {
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU:
            for (int i = 0; i < opt->widget->choice_cnt; i++)
                opt->item.sw[i].s = opt->widget->value.index == i ? ISS_ON : ISS_OFF;
            IDSetSwitch(&opt->prop.sw, nullptr);
            break;
        case GP_WIDGET_TEXT:
            free(opt->item.text.text);
            opt->item.text.text = strdup(opt->widget->value.text);
            IDSetText(&opt->prop.text, nullptr);
            break;
        case GP_WIDGET_TOGGLE:
            if (opt->widget->value.toggle)
            {
                opt->item.sw[0].s = ISS_ON;
                opt->item.sw[1].s = ISS_OFF;
            }
            else
            {
                opt->item.sw[0].s = ISS_OFF;
                opt->item.sw[1].s = ISS_ON;
            }
            IDSetSwitch(&opt->prop.sw, nullptr);
            break;
        case GP_WIDGET_RANGE:
            opt->item.num.value = opt->widget->value.num;
            IDSetNumber(&opt->prop.num, nullptr);
            break;
        case GP_WIDGET_DATE:
            free(opt->item.text.text);
            opt->item.text.text = static_cast<char *>(malloc(MAXINDILABEL));
            strftime(opt->item.text.text, MAXINDILABEL, "%FT%TZ", gmtime(reinterpret_cast<time_t *>(&opt->widget->value.date)));
            IDSetText(&opt->prop.text, nullptr);
            break;
        default:
            delete opt;
            return;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::AddWidget(gphoto_widget * widget)
{
    IPerm perm;

    if (!widget || ((widget->type == GP_WIDGET_RADIO || widget->type == GP_WIDGET_MENU) && widget->choice_cnt > MAX_SWITCHES))
        return;

    perm = widget->readonly ? IP_RO : IP_RW;

    cam_opt * opt = new cam_opt();
    opt->widget  = widget;

    switch (widget->type)
    {
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU:
            opt->item.sw = createLegacySwitch(widget->name, widget->choices, widget->choice_cnt, widget->value.index);
            IUFillSwitchVector(&opt->prop.sw, opt->item.sw, widget->choice_cnt, getDeviceName(), widget->name,
                               widget->name, widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
            defineProperty(&opt->prop.sw);
            break;
        case GP_WIDGET_TEXT:
            IUFillText(&opt->item.text, widget->name, widget->name, widget->value.text);
            IUFillTextVector(&opt->prop.text, &opt->item.text, 1, getDeviceName(), widget->name, widget->name,
                             widget->parent, perm, 60, IPS_IDLE);
            defineProperty(&opt->prop.text);
            break;
        case GP_WIDGET_TOGGLE:
            opt->item.sw = createLegacySwitch(widget->name, static_cast<char **>(on_off), 2, widget->value.toggle ? 0 : 1);
            IUFillSwitchVector(&opt->prop.sw, opt->item.sw, 2, getDeviceName(), widget->name, widget->name,
                               widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
            defineProperty(&opt->prop.sw);
            break;
        case GP_WIDGET_RANGE:
            IUFillNumber(&opt->item.num, widget->name, widget->name, "%5.2f", widget->min, widget->max, widget->step,
                         widget->value.num);
            IUFillNumberVector(&opt->prop.num, &opt->item.num, 1, getDeviceName(), widget->name, widget->name,
                               widget->parent, perm, 60, IPS_IDLE);
            defineProperty(&opt->prop.num);
            break;
        case GP_WIDGET_DATE:
        {
            char ts[MAXINDITSTAMP] = {0};
            strftime(ts, MAXINDILABEL, "%FT%TZ", gmtime(reinterpret_cast<time_t *>(&opt->widget->value.date)));
            IUFillText(&opt->item.text, widget->name, widget->name, ts);
            IUFillTextVector(&opt->prop.text, &opt->item.text, 1, getDeviceName(), widget->name, widget->name,
                             widget->parent, perm, 60, IPS_IDLE);
            defineProperty(&opt->prop.text);
        }
        break;
        default:
            delete opt;
            return;
    }

    CamOptions[widget->name] = opt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::ShowExtendedOptions(void)
{
    gphoto_widget_list * iter;

    iter = gphoto_find_all_widgets(gphotodrv);
    while (iter)
    {
        gphoto_widget * widget = gphoto_get_widget_info(gphotodrv, &iter);
        AddWidget(widget);
    }

    gphoto_show_options(gphotodrv);

    optTID = IEAddTimer(1000, GPhotoCCD::UpdateExtendedOptions, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::HideExtendedOptions(void)
{
    if (optTID)
    {
        IERmTimer(optTID);
        optTID = 0;
    }

    std::vector<std::string> extendedPropertyNames;

    while (CamOptions.begin() != CamOptions.end())
    {
        cam_opt * opt = (*CamOptions.begin()).second;

        switch (opt->widget->type)
        {
            case GP_WIDGET_RADIO:
            case GP_WIDGET_MENU:
            case GP_WIDGET_TOGGLE:
                free(opt->item.sw);
                break;
            case GP_WIDGET_TEXT:
            case GP_WIDGET_DATE:
                free(opt->item.text.text);
                break;
            default:
                break;
        }

        extendedPropertyNames.push_back((*CamOptions.begin()).first);
        delete opt;
        CamOptions.erase(CamOptions.begin());
    }

    for (const auto &oneName : extendedPropertyNames)
        deleteProperty(oneName.c_str());
}

#if 0
IPState GPhotoCCD::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (isSimulation() || speed == 0)
        return IPS_OK;

    // Set speed accordng to duration
    if (duration >= 1000)
        speed = 3;
    else if (duration >= 250)
        speed = 2;
    else
        speed = 1;

    if (dir == FOCUS_INWARD)
        focusSpeed = speed * -1;
    else
        focusSpeed = speed;

    LOGF_DEBUG("Setting focuser speed to %d", focusSpeed);

    FocusTimerNP.s = IPS_BUSY;
    IDSetNumber(&FocusTimerNP, nullptr);

    /*while (duration-->0)
    {
       if ( gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
       {
           LOGF_ERROR("Focusing failed: %s", errMsg);
           return IPS_ALERT;
       }
    }*/

    // If we have a view finder, let's turn it on
    if (Streamer->isBusy() == false)
        gphoto_set_view_finder(gphotodrv, true);

    //SetTimer(FOCUS_TIMER);

    char errMsg[MAXRBUF];
    if (gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
    {
        LOGF_ERROR("Focusing failed: %s", errMsg);
        return IPS_ALERT;
    }
    else if (Streamer->isBusy() == false)
        gphoto_set_view_finder(gphotodrv, false);

    FocusTimerN[0].value = 0;
    return IPS_OK;
}

bool GPhotoCCD::SetFocuserSpeed(int speed)
{
    if (speed >= FocusSpeedN[0].min && speed <= FocusSpeedN[0].max)
        return true;

    return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState GPhotoCCD::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    INDI_UNUSED(dir);

    // Reduce by a factor of 10
    double adaptiveTicks = ticks / 10.0;

    double largeStep = adaptiveTicks / (FOCUS_HIGH_MED_RATIO * FOCUS_MED_LOW_RATIO);
    double medStep   = (largeStep - rint(largeStep)) * FOCUS_HIGH_MED_RATIO;
    double lowStep   = (medStep - rint(medStep)) * FOCUS_MED_LOW_RATIO;

    m_TargetLargeStep = rint(fabs(largeStep));
    m_TargetMedStep = rint(fabs(medStep));
    m_TargetLowStep = rint(fabs(lowStep));

    if (m_FocusTimerID > 0)
        RemoveTimer(m_FocusTimerID);

    m_FocusTimerID = IEAddTimer(FOCUS_TIMER, &GPhotoCCD::UpdateFocusMotionHelper, this);

    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::UpdateFocusMotionHelper(void *context)
{
    static_cast<GPhotoCCD*>(context)->UpdateFocusMotionCallback();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::UpdateFocusMotionCallback()
{
    char errmsg[MAXRBUF] = {0};
    int focusSpeed = -1;

    if (m_TargetLargeStep > 0)
    {
        m_TargetLargeStep--;
        focusSpeed = FocusMotionSP.findOnSwitchIndex() == FOCUS_INWARD ? -3 : 3;
    }
    else if (m_TargetMedStep > 0)
    {
        m_TargetMedStep--;
        focusSpeed = FocusMotionSP.findOnSwitchIndex() == FOCUS_INWARD ? -2 : 2;
    }
    else if (m_TargetLowStep > 0)
    {
        m_TargetLowStep--;
        focusSpeed = FocusMotionSP.findOnSwitchIndex() == FOCUS_INWARD ? -1 : 1;
    }

    if (gphoto_manual_focus(gphotodrv, focusSpeed, errmsg) != GP_OK)
    {
        LOGF_ERROR("Focusing failed: %s", errmsg);
        FocusRelPosNP.setState(IPS_ALERT);
        FocusRelPosNP.apply();
        return;
    }

    if (m_TargetLargeStep == 0 && m_TargetMedStep == 0 && m_TargetLowStep == 0)
    {
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
    }
    else
        m_FocusTimerID = IEAddTimer(FOCUS_TIMER, &GPhotoCCD::UpdateFocusMotionHelper, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::StartStreaming()
{
    if (gphoto_start_preview(gphotodrv) == GP_OK)
    {
        Streamer->setPixelFormat(INDI_RGB);
        std::unique_lock<std::mutex> guard(liveStreamMutex);
        m_RunLiveStream = true;
        guard.unlock();
        m_LiveViewThread = std::thread(&GPhotoCCD::streamLiveView, this);
        return true;
    }

    return false;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::StopStreaming()
{
    std::unique_lock<std::mutex> guard(liveStreamMutex);
    m_RunLiveStream = false;
    guard.unlock();
    m_LiveViewThread.join();
    return (gphoto_stop_preview(gphotodrv) == GP_OK);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::streamLiveView()
{
    //char errMsg[MAXRBUF];
    const char * previewData = nullptr;
    unsigned long int previewSize = 0;
    CameraFile * previewFile = nullptr;

    int rc = gp_file_new(&previewFile);
    if (rc != GP_OK)
    {
        LOGF_ERROR("Error creating gphoto file: %s", gp_result_as_string(rc));
        return;
    }

    char errMsg[MAXRBUF] = {0};
    while (true)
    {
        std::unique_lock<std::mutex> guard(liveStreamMutex);
        if (m_RunLiveStream == false)
            break;
        guard.unlock();

        rc = gphoto_capture_preview(gphotodrv, previewFile, errMsg);
        if (rc != GP_OK)
        {
            //LOGF_DEBUG("%s", errMsg);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (rc >= GP_OK)
        {
            rc = gp_file_get_data_and_size(previewFile, &previewData, &previewSize);
            if (rc != GP_OK)
            {
                LOGF_ERROR("Error getting preview image data and size: %s", gp_result_as_string(rc));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        uint8_t * inBuffer = reinterpret_cast<uint8_t *>(const_cast<char *>(previewData));

        //        if (streamSubframeS[1].s == ISS_ON)
        //        {
        //            if (liveVideoWidth <= 0)
        //            {
        //                read_jpeg_size(inBuffer, previewSize, &liveVideoWidth, &liveVideoHeight);
        //                Streamer->setSize(liveVideoWidth, liveVideoHeight);
        //            }

        //            std::unique_lock<std::mutex> ccdguard(ccdBufferLock);
        //            Streamer->newFrame(inBuffer, previewSize);
        //            ccdguard.unlock();
        //            continue;
        //        }

        uint8_t * ccdBuffer      = PrimaryCCD.getFrameBuffer();
        size_t size             = 0;
        int w = 0, h = 0, naxis = 0;

        // Read jpeg from memory
        std::unique_lock<std::mutex> ccdguard(ccdBufferLock);
        rc = read_jpeg_mem(inBuffer, previewSize, &ccdBuffer, &size, &naxis, &w, &h);

        if (rc != 0)
        {
            LOG_ERROR("Error getting live video frame.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (liveVideoWidth <= 0)
        {
            liveVideoWidth = w;
            liveVideoHeight = h;
            Streamer->setSize(liveVideoWidth, liveVideoHeight);
        }

        PrimaryCCD.setFrameBuffer(ccdBuffer);

        // We are done with writing to CCD buffer
        ccdguard.unlock();

        if (naxis != PrimaryCCD.getNAxis())
        {
            if (naxis == 1)
                Streamer->setPixelFormat(INDI_MONO);

            PrimaryCCD.setNAxis(naxis);
        }

        if (PrimaryCCD.getSubW() != w || PrimaryCCD.getSubH() != h)
        {
            Streamer->setSize(w, h);
            PrimaryCCD.setBin(1, 1);
            PrimaryCCD.setFrame(0, 0, w, h);
        }

        if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(size))
            PrimaryCCD.setFrameBufferSize(size, false);

        Streamer->newFrame(ccdBuffer, size);
    }

    gp_file_unref(previewFile);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::saveConfigItems(FILE * fp)
{
    // First save Device Port
    if (PortTP[0].getText())
        PortTP.save(fp);

    // Second save the CCD Info property
    PrimaryCCD.getCCDInfo().save(fp);

    // Save regular CCD properties
    INDI::CCD::saveConfigItems(fp);

    // Mirror Locking
    MirrorLockNP.save(fp);

    // Download Timeout
    DownloadTimeoutNP.save(fp);

    // Capture Target
    if (CaptureTargetSP.getState() == IPS_OK)
    {
        CaptureTargetSP.save(fp);
    }

    // SD Card Behavior
    if (CaptureTargetSP.getState() == IPS_OK || strstr(getDeviceName(), "Fuji"))
    {
        SDCardImageSP.save(fp);
    }

    // ISO Settings
    if (ISOSP.count() > 0)
        ISOSP.save(fp);

    // Force BULB Mode
    ForceBULBSP.save(fp);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    if (ISOSP.count() > 0)
    {
        auto onISO = ISOSP.findOnSwitch();
        if (onISO)
        {
            int isoSpeed = atoi(onISO->getLabel());
            if (isoSpeed > 0)
                fitsKeywords.push_back({"ISOSPEED", isoSpeed, "ISO Speed"});
        }
    }

    if (isTemperatureSupported)
    {
        fitsKeywords.push_back({"CCD-TEMP", TemperatureNP[0].getValue(), 3, "CCD Temperature (Celsius)"});
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::UpdateCCDUploadMode(CCD_UPLOAD_MODE mode)
{
    if (!isSimulation())
        gphoto_set_upload_settings(gphotodrv, mode);

    // Reject changes to upload mode while we are ignoring the image download.
    if (SDCardImageSP[SD_CARD_IGNORE_IMAGE].getState() == ISS_ON && mode != UPLOAD_LOCAL)
        return false;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPhotoCCD::simulationTriggered(bool enabled)
{
    if (enabled)
        defineProperty(UploadFileTP);
    else
        deleteProperty(UploadFileTP);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GPhotoCCD::SetCaptureFormat(uint8_t index)
{
    INDI_UNUSED(index);
    // We need to get frame W and H if format changes
    frameInitialized = false;
    return true;
}
