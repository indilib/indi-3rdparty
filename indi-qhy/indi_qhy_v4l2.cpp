/*
INDI QHY V4L2 CCD Driver

Copyright (C) 2018 Robert Lancaster (rlancaste AT gmail DOT com)

This driver was inspired by the INDI FFMpeg Driver written by
Geehalel (geehalel AT gmail DOT com), see: https://github.com/geehalel/indi-ffmpeg

This driver is free software; you can redistribute it and/or
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

#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <eventloop.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits.h>
#include <map>
#include <queue>
#include <set>
#ifdef __linux__
#include <dirent.h>
#include <linux/media.h>
#include <sys/select.h>
#include <sys/sysmacros.h>
#endif

#include "indi_qhy_v4l2.h"
#include "config.h"

#ifdef __linux__
#ifndef QHY_V4L2_DIRECT_DEVICE
#define QHY_V4L2_DIRECT_DEVICE "V4L2 Direct"
#endif
#endif

#ifdef __linux__
namespace
{
struct V4L2CameraDescriptor
{
    std::string name;
    std::string videoNode;
    std::string subdevNode;
    std::string role;
    double pixelSize = 0.0;
};

struct V4L2FormatInfo
{
    int bitDepth = 8;
    bool bayer = false;
    const char *cfa = nullptr;
};

V4L2FormatInfo describeV4L2Format(uint32_t fourcc)
{
    switch (fourcc)
    {
        case v4l2_fourcc('R', 'G', '1', '2'):
            return {12, true, "SRGGB"};
        case v4l2_fourcc('B', 'A', '1', '2'):
            return {12, true, "SBGGR"};
        case v4l2_fourcc('G', 'B', '1', '2'):
            return {12, true, "SGBRG"};
        case v4l2_fourcc('B', 'G', '1', '2'):
            return {12, true, "SGRBG"};
        case v4l2_fourcc('R', 'G', '1', '0'):
            return {10, true, "SRGGB"};
        case v4l2_fourcc('B', 'A', '1', '0'):
            return {10, true, "SBGGR"};
        case v4l2_fourcc('G', 'B', '1', '0'):
            return {10, true, "SGBRG"};
        case v4l2_fourcc('B', 'G', '1', '0'):
            return {10, true, "SGRBG"};
        case v4l2_fourcc('R', 'G', '1', '4'):
            return {14, true, "SRGGB"};
        case v4l2_fourcc('B', 'A', '1', '4'):
            return {14, true, "SBGGR"};
        case v4l2_fourcc('G', 'B', '1', '4'):
            return {14, true, "SGBRG"};
        case v4l2_fourcc('B', 'G', '1', '4'):
            return {14, true, "SGRBG"};
        case v4l2_fourcc('R', 'G', '1', '6'):
            return {16, true, "SRGGB"};
        case v4l2_fourcc('B', 'A', '1', '6'):
            return {16, true, "SBGGR"};
        case v4l2_fourcc('G', 'B', '1', '6'):
            return {16, true, "SGBRG"};
        case v4l2_fourcc('B', 'G', '1', '6'):
            return {16, true, "SGRBG"};
        case v4l2_fourcc('Y', '1', '0', ' '):
            return {10, false, nullptr};
        case v4l2_fourcc('Y', '1', '2', ' '):
            return {12, false, nullptr};
        case v4l2_fourcc('Y', '1', '4', ' '):
            return {14, false, nullptr};
        case V4L2_PIX_FMT_Y16:
            return {16, false, nullptr};
        case V4L2_PIX_FMT_RGB24:
        case V4L2_PIX_FMT_YUYV:
            return {8, false, nullptr};
        default:
            return {};
    }
}

double readDeviceTreePixelSize(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return 0.0;

    unsigned char buffer[64] {};
    const ssize_t bytes = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (bytes <= 0)
        return 0.0;

    const std::string text(reinterpret_cast<const char *>(buffer),
                           strnlen(reinterpret_cast<const char *>(buffer), static_cast<size_t>(bytes)));
    char *end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end != text.c_str() && parsed > 0.0)
        return parsed > 100.0 ? parsed / 1000.0 : parsed;

    if (bytes == 4)
    {
        const uint32_t raw = (static_cast<uint32_t>(buffer[0]) << 24) |
                             (static_cast<uint32_t>(buffer[1]) << 16) |
                             (static_cast<uint32_t>(buffer[2]) << 8) |
                             static_cast<uint32_t>(buffer[3]);
        if (raw > 0)
            return raw / 1000.0;
    }
    return 0.0;
}

std::string readDeviceTreeString(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return {};

    char buffer[256] {};
    const ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    return bytes > 0 ? std::string(buffer, strnlen(buffer, static_cast<size_t>(bytes))) : std::string {};
}

bool hasPrefix(const char *value, const char *prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

std::string findSensorSubdev(const std::string &i2cDevice)
{
    DIR *dir = opendir("/sys/class/video4linux");
    if (!dir)
        return {};

    std::string result;
    const std::string suffix = "/" + i2cDevice;
    while (const dirent *entry = readdir(dir))
    {
        if (!hasPrefix(entry->d_name, "v4l-subdev"))
            continue;

        const std::string link = std::string("/sys/class/video4linux/") + entry->d_name + "/device";
        char target[PATH_MAX] {};
        const ssize_t length = readlink(link.c_str(), target, sizeof(target) - 1);
        if (length <= 0)
            continue;

        const std::string resolved(target, static_cast<size_t>(length));
        if (resolved.size() >= suffix.size() && resolved.compare(resolved.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            result = std::string("/dev/") + entry->d_name;
            break;
        }
    }
    closedir(dir);
    return result;
}

std::map<std::pair<unsigned int, unsigned int>, std::string> enumerateVideoNodes()
{
    std::map<std::pair<unsigned int, unsigned int>, std::string> nodes;
    DIR *dir = opendir("/dev");
    if (!dir)
        return nodes;

    while (const dirent *entry = readdir(dir))
    {
        if (!hasPrefix(entry->d_name, "video"))
            continue;

        const char *number = entry->d_name + strlen("video");
        if (*number == '\0' || strspn(number, "0123456789") != strlen(number))
            continue;

        const std::string path = std::string("/dev/") + entry->d_name;
        struct stat st {};
        if (stat(path.c_str(), &st) == 0 && S_ISCHR(st.st_mode))
            nodes[{ major(st.st_rdev), minor(st.st_rdev) }] = path;
    }
    closedir(dir);
    return nodes;
}

long long captureNodeScore(const std::string &path)
{
    const int fd = open(path.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        return -1;

    long long score = -1;
    struct v4l2_capability cap {};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
    {
        const unsigned int devCaps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
        const bool capture = (devCaps & V4L2_CAP_VIDEO_CAPTURE) || (devCaps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
        if (capture && (devCaps & V4L2_CAP_STREAMING))
        {
            struct v4l2_format fmt {};
            fmt.type = (devCaps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
            {
                const unsigned int width = (fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ? fmt.fmt.pix_mp.width : fmt.fmt.pix.width;
                const unsigned int height = (fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ? fmt.fmt.pix_mp.height : fmt.fmt.pix.height;
                if (width > 0 && height > 0)
                {
                    const uint32_t fourcc = (fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ?
                                             fmt.fmt.pix_mp.pixelformat : fmt.fmt.pix.pixelformat;
                    score = static_cast<long long>(width) * height;
                    if (describeV4L2Format(fourcc).bitDepth >= 10)
                        score += (1LL << 50);
                }
            }
        }
    }
    close(fd);
    return score;
}

std::string findSensorVideoNode(const std::string &i2cDevice,
                                const std::map<std::pair<unsigned int, unsigned int>, std::string> &videoNodes)
{
    DIR *dir = opendir("/dev");
    if (!dir)
        return {};

    std::vector<std::string> mediaNodes;
    while (const dirent *entry = readdir(dir))
    {
        if (hasPrefix(entry->d_name, "media"))
            mediaNodes.emplace_back(std::string("/dev/") + entry->d_name);
    }
    closedir(dir);
    std::sort(mediaNodes.begin(), mediaNodes.end());

    for (const auto &mediaNode : mediaNodes)
    {
        const int fd = open(mediaNode.c_str(), O_RDONLY);
        if (fd < 0)
            continue;

        std::map<uint32_t, media_entity_desc> entities;
        media_entity_desc entity {};
        entity.id = MEDIA_ENT_ID_FLAG_NEXT;
        while (ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &entity) == 0)
        {
            entities[entity.id] = entity;
            entity.id |= MEDIA_ENT_ID_FLAG_NEXT;
        }

        uint32_t sensorId = 0;
        for (const auto &[id, desc] : entities)
        {
            if (std::string(desc.name).find(i2cDevice) != std::string::npos)
            {
                sensorId = id;
                break;
            }
        }
        if (sensorId == 0)
        {
            close(fd);
            continue;
        }

        std::map<uint32_t, std::vector<uint32_t>> graph;
        for (const auto &[id, desc] : entities)
        {
            std::vector<media_pad_desc> pads(desc.pads);
            std::vector<media_link_desc> links(desc.links);
            media_links_enum enumeration {};
            enumeration.entity = id;
            enumeration.pads = pads.data();
            enumeration.links = links.data();
            if (ioctl(fd, MEDIA_IOC_ENUM_LINKS, &enumeration) < 0)
                continue;

            for (const auto &link : links)
            {
                if (link.flags & MEDIA_LNK_FL_ENABLED)
                    graph[link.source.entity].push_back(link.sink.entity);
            }
        }

        std::queue<uint32_t> pending;
        std::set<uint32_t> visited;
        pending.push(sensorId);
        visited.insert(sensorId);
        std::string bestNode;
        long long bestScore = -1;
        while (!pending.empty())
        {
            const uint32_t id = pending.front();
            pending.pop();
            const auto descriptor = entities.find(id);
            if (descriptor != entities.end())
            {
                const auto node = videoNodes.find({ descriptor->second.dev.major, descriptor->second.dev.minor });
                if (node != videoNodes.end())
                {
                    const long long score = captureNodeScore(node->second);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestNode = node->second;
                    }
                }
            }

            for (const uint32_t next : graph[id])
            {
                if (visited.insert(next).second)
                    pending.push(next);
            }
        }
        close(fd);
        if (!bestNode.empty())
            return bestNode;
    }
    return {};
}

std::string roleTitle(const std::string &role)
{
    if (role == "main")
        return "Main";
    if (role == "guide")
        return "Guide";
    return role;
}

std::vector<V4L2CameraDescriptor> discoverV4L2Cameras()
{
    std::vector<V4L2CameraDescriptor> cameras;
    const auto videoNodes = enumerateVideoNodes();
    DIR *dir = opendir("/sys/bus/i2c/devices");
    if (!dir)
        return cameras;

    while (const dirent *entry = readdir(dir))
    {
        const std::string base = std::string("/sys/bus/i2c/devices/") + entry->d_name + "/of_node/";
        const std::string configuredName = readDeviceTreeString(base + "qhy,indi-name");
        if (configuredName.empty())
            continue;

        const std::string role = readDeviceTreeString(base + "qhy,indi-role");
        const std::string subdev = findSensorSubdev(entry->d_name);
        const std::string video = findSensorVideoNode(entry->d_name, videoNodes);
        if (subdev.empty() || video.empty())
        {
            fprintf(stderr, "QHY V4L2: skip sensor %s (role=%s): sensor pipeline is incomplete (video=%s, subdev=%s)\n",
                    entry->d_name, role.empty() ? "unspecified" : role.c_str(), video.c_str(), subdev.c_str());
            continue;
        }

        std::string deviceName = "QHY CCD " + configuredName;
        if (!role.empty())
            deviceName += " " + roleTitle(role);
        else
            deviceName += " Camera";

        cameras.push_back({ deviceName,
                            video, subdev, role,
                            readDeviceTreePixelSize(base + "qhy,pixel-size-um") });
    }
    closedir(dir);
    std::sort(cameras.begin(), cameras.end(), [](const auto &left, const auto &right)
    {
        return left.role < right.role;
    });
    return cameras;
}

class V4L2CameraLoader
{
public:
    V4L2CameraLoader()
    {
        const auto cameras = discoverV4L2Cameras();
        for (const auto &camera : cameras)
        {
            fprintf(stderr, "QHY V4L2: create %s (%s, %s)\n", camera.name.c_str(),
                    camera.videoNode.c_str(), camera.subdevNode.c_str());
            devices.emplace_back(std::make_unique<indi_qhy_v4l2>(camera.name, camera.videoNode,
                                                                  camera.subdevNode, camera.role,
                                                                  camera.pixelSize));
        }
    }

private:
    std::vector<std::unique_ptr<indi_qhy_v4l2>> devices;
};

V4L2CameraLoader qhyV4L2CameraLoader;
}
#else
static std::unique_ptr<indi_qhy_v4l2> qhy_v4l2(new indi_qhy_v4l2());
#endif


indi_qhy_v4l2::indi_qhy_v4l2(const std::string &name, const std::string &videoPath,
                             const std::string &subdevPath, const std::string &role,
                             double discoveredPixelSize)
    : defaultDeviceName(name), v4l2_role(role), pixelSize(discoveredPixelSize),
      v4l2_subdev_path(subdevPath)
{
    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MINOR);
    buffer = nullptr;

    //setting default values

#ifdef __linux__
    videoDevice = QHY_V4L2_DIRECT_DEVICE;
    videoSource = videoPath;
    inputPixelFormat = "";
#else
    videoDevice = "";
    videoSource = "";
#endif

    frameRate = 30;
    videoSize = "";
    webcamStacking = false;
    averaging = false;
    outputFormat = "8 bit RGB";

    bufferTimeout = 10000;
    if (pixelSize < 0.0)
        pixelSize = 0.0;
}

indi_qhy_v4l2::~indi_qhy_v4l2()
{
    freeMemory();
}


/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool indi_qhy_v4l2::Connect()
{
    bool rc = false;

    ISwitchVectorProperty *connect = getSwitch("CONNECTION");
    if (connect)
        connect->s = IPS_BUSY;

    DEBUGF(INDI::Logger::DBG_SESSION, "Trying to connect to: %s, on device: %s with %s at %u frames per second",
           videoSource.c_str(), videoDevice.c_str(), videoSize.c_str(), frameRate);

    rc = ConnectToSource(videoSource);
    return rc;
}

// V4L2 is the only supported capture backend on Linux.

bool indi_qhy_v4l2::ConnectToSource(const std::string &source)
{
#ifdef __linux__
    if (isConnected())
        DisconnectV4L2();
    const std::string sourcePath = source.empty() ? videoSource : source;
    if (!ConnectToSourceV4L2(sourcePath))
        return false;

    updateV4L2ImageMetadata();
    DEBUG(INDI::Logger::DBG_SESSION, "Connection Successful (V4L2 Direct).");
    return true;
#else
    return false;
#endif
}

//This should be run if the source was somehow disconnected to try to reconnect it.
//It will make 10 attempts.
//It returns true if it was successful.
bool indi_qhy_v4l2::reconnectSource()
{
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        if (ConnectToSource(videoSource))
            return true;
    }
    return false;
}

//This is the method that should be called to change the streaming device, source, framerate, or video size
//If it was already connected, it will attempt a connection with the new settings and if it is not successful, it will revert to the old ones.
//It should be safe to use while streaming or between image captures because it will pause them and return them to normal afterwards.
bool indi_qhy_v4l2::ChangeSource(std::string newDevice, std::string newSource, int newFramerate, std::string newInputPixelFormat, std::string newVideosize)
{
    //This will pause the streaming while it attempts the new connection settings.
    bool was_streaming = false;
    if(is_streaming)
    {
        was_streaming = true;
        StopStreaming();
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "New Connection Settings: %s, on device: %s with %s at %u frames per second",
           newSource.c_str(), newDevice.c_str(), newVideosize.c_str(), newFramerate);

    //This is the case if the source is not currently connected yet.
    if(isConnected() == false)
        DEBUG(INDI::Logger::DBG_SESSION, "Not connected now, accepting settings.  It will be tested on connection");
    if(isConnected() == false || loadingSettings)
    {
        videoDevice = newDevice;
        videoSource = newSource;
        frameRate = newFramerate;
        videoSize = newVideosize;
        return true;
    }

    //This is an attempt to connect, if it is already connected.  If it is not successful, it goes back to the old settings.
    if(ConnectToSource(newSource) == false)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Connection was NOT successful");
        DEBUGF(INDI::Logger::DBG_SESSION, "Changing back to: %s, on device: %s with %s at %u frames per second",
               videoSource.c_str(), videoDevice.c_str(), videoSize.c_str(), frameRate);
        ConnectToSource(videoSource);
        if(was_streaming)
            StartStreaming();
        return false;
    }

    //This is what happens if the connection was successful, it saves the settings and continues.
    DEBUG(INDI::Logger::DBG_SESSION, "Due to success, Saving settings.");
    videoDevice = newDevice;
    videoSource = newSource;
    frameRate = newFramerate;
    inputPixelFormat = newInputPixelFormat;
    videoSize = newVideosize;

    //If it was streaming, we need to reinitialize that.
    if(was_streaming)
        StartStreaming();
    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool indi_qhy_v4l2::Disconnect()
{
    if (isConnected())
    {
        DisconnectV4L2();

        DEBUG(INDI::Logger::DBG_SESSION, "QHY CCD disconnected successfully!");
    }
    return true;
}
/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * indi_qhy_v4l2::getDefaultName()
{
    return defaultDeviceName.c_str();
}
/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool indi_qhy_v4l2::initProperties()
{
    loadingSettings = true;
    // Must init parent properties first!
    INDI::CCD::initProperties();

    setDefaultPollingPeriod(10);

    DEBUG(INDI::Logger::DBG_SESSION, "QHY CCD Driver initialized");

#ifdef __linux__
    // Gain and offset properties are available only for the V4L2 backend.
    IUFillNumber(&GainT[0], "GAIN", "Gain", "%.0f",
                 static_cast<double>(v4l2_subdev_gain_min),
                 static_cast<double>(v4l2_subdev_gain_max),
                 1.0, static_cast<double>(v4l2_subdev_gain));
    IUFillNumberVector(&GainTP, GainT, 1, getDeviceName(), "CCD_GAIN",
                       "Gain", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&OffsetT[0], "OFFSET", "Offset", "%.0f",
                 static_cast<double>(v4l2_subdev_offset_min),
                 static_cast<double>(v4l2_subdev_offset_max),
                 static_cast<double>(v4l2_subdev_offset_step),
                 static_cast<double>(v4l2_subdev_offset));
    IUFillNumberVector(&OffsetTP, OffsetT, 1, getDeviceName(), "CCD_OFFSET",
                       "Offset", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);
#endif

    CaptureFormat rgb = {"INDI_RGB", "RGB", 8, true};
    addCaptureFormat(rgb);

    RapidStacking = new ISwitch[3];
    IUFillSwitch(&RapidStacking[0], "Integration", "Integration", ISS_OFF);
    IUFillSwitch(&RapidStacking[1], "Average", "Average", ISS_OFF);
    IUFillSwitch(&RapidStacking[2], "Off", "Off", ISS_ON);

    IUFillSwitchVector(&RapidStackingSelection, RapidStacking, 3, getDeviceName(), "RAPID_STACKING_OPTION", "Rapid Stacking",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineProperty(&RapidStackingSelection);

    OutputFormats = new ISwitch[3];
    IUFillSwitch(&OutputFormats[0], "16 bit Grayscale", "16 bit Grayscale", ISS_OFF);
    IUFillSwitch(&OutputFormats[1], "16 bit RGB", "16 bit RGB", ISS_OFF);
    IUFillSwitch(&OutputFormats[2], "8 bit RGB", "8 bit RGB", ISS_ON);

    IUFillSwitchVector(&OutputFormatSelection, OutputFormats, 3, getDeviceName(), "OUTPUT_FORMAT_OPTION", "Output Format",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineProperty(&OutputFormatSelection);

    IUFillNumber(&TimeoutOptionsT[0], "BUFFER_TIMEOUT", "Buffer", "%.0f", 0 , 10000000, 1, bufferTimeout);
    IUFillNumberVector(&TimeoutOptionsTP, TimeoutOptionsT, NARRAY(TimeoutOptionsT), getDeviceName(), "TIMEOUT_OPTIONS",
                     "Timeouts (us)", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    defineProperty(&TimeoutOptionsTP);

    IUFillNumber(&PixelSizeT[0], "PIXEL_SIZE_um", "Pixel Size (µm)", "%.3f", 0 , 50, 0.1, pixelSize);
    IUFillNumberVector(&PixelSizeTP, PixelSizeT, NARRAY(PixelSizeT), getDeviceName(), "PIXEL_SIZE",
                     "Pixel Size", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    defineProperty(&PixelSizeTP);
    // Optional raw save controls
    IUFillSwitch(&SaveRawS[0], "ENABLE", "Enable Save", ISS_OFF);
    IUFillSwitchVector(&SaveRawSP, SaveRawS, 1, getDeviceName(), "AUTO_SAVE_RAW", "Auto Save Raw", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineProperty(&SaveRawSP);

    IUFillText(&SaveRawPathT[0], "SAVE_RAW_PATH", "Raw Path", save_raw_path.c_str());
    IUFillTextVector(&SaveRawPathTP, SaveRawPathT, 1, getDeviceName(), "SAVE_RAW_PATH", "Raw Save Path", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&SaveRawPathTP);


    PixelSizes = new ISwitch[15];
    IUFillSwitch(&PixelSizes[0], "2.20", "NexImage 5 - 2.2", ISS_OFF);
    IUFillSwitch(&PixelSizes[1], "3.30", "Logitech Webcam Pro 9000 - 3.3", ISS_OFF);
    IUFillSwitch(&PixelSizes[2], "3.00", "SVBONY SV105 - 3.0", ISS_OFF);
    IUFillSwitch(&PixelSizes[3], "4.00", "SVBONY SV205 - 4.0", ISS_OFF);
    IUFillSwitch(&PixelSizes[4], "1.67", "NexImage 10 - 1.67", ISS_OFF);
    IUFillSwitch(&PixelSizes[5], "3.75", "NexImage Burst - 3.75", ISS_OFF);
    IUFillSwitch(&PixelSizes[6], "3.75", "Skyris 132 - 3.75", ISS_OFF);
    IUFillSwitch(&PixelSizes[7], "2.80", "Skyris 236 - 2.8", ISS_OFF);
    IUFillSwitch(&PixelSizes[8], "3.75", "iOptron iGuider or iPolar - 3.75", ISS_OFF);
    IUFillSwitch(&PixelSizes[9], "1.55", "Raspberry Pi HQ Camera - 1.55", ISS_OFF);
    IUFillSwitch(&PixelSizes[10], "2.8", "Logitech HD C270 - 2.8", ISS_OFF);
    IUFillSwitch(&PixelSizes[11], "2.9", "IMX290 USB 2.0 Camera Board - 2.9", ISS_OFF);
    IUFillSwitch(&PixelSizes[12], "2.9", "Spinel 2MP IMX290 H264 Camera - 2.9", ISS_OFF);
    IUFillSwitch(&PixelSizes[13], "3.0", "Microsoft LifeCam Cinema TM - 3.0", ISS_OFF);
    IUFillSwitch(&PixelSizes[14], "2.9", "OpenAstroGuider - 2.9", ISS_OFF);

    IUFillSwitchVector(&PixelSizeSelection, PixelSizes, 15, getDeviceName(), "PIXEL_SIZE_SELECTION", "Camera Pixel Sizes (µm)",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
     defineProperty(&PixelSizeSelection);

    IUFillSwitch(&RefreshS[0], "Scan Ports", "Scan Sources", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, getDeviceName(), "INPUT_SCAN", "Refresh", CONNECTION_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    defineProperty(&RefreshSP);

    IUFillText(&InputOptionsT[0], "CAPTURE_DEVICE_TEXT", "Capture Device", videoDevice.c_str());
    IUFillText(&InputOptionsT[1], "CAPTURE_SOURCE_TEXT", "Capture Source", videoSource.c_str());
    IUFillText(&InputOptionsT[2], "CAPTURE_FRAME_RATE", "Frame Rate", "30");
    IUFillText(&InputOptionsT[3], "INPUT_PIXEL_FORMAT", "Input Pixel Format", inputPixelFormat.c_str());
    IUFillText(&InputOptionsT[4], "CAPTURE_VIDEO_SIZE", "Video Size", videoSize.c_str());
    IUFillTextVector(&InputOptionsTP, InputOptionsT, NARRAY(InputOptionsT), getDeviceName(), "INPUT_OPTIONS", "Input Options",
                     CONNECTION_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&InputOptionsTP);




#ifdef __linux__
    IUFillText(&V4L2SubdevPathT[0], "SUBDEV_PATH", "Sub-device", v4l2_subdev_path.c_str());
    IUFillTextVector(&V4L2SubdevPathTP, V4L2SubdevPathT, 1, getDeviceName(), "V4L2_SUBDEVICE_PATH",
                     "Sensor Control", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&V4L2SubdevPathTP);

    // CCD_EXPOSURE is synchronized to the V4L2 subdevice internally.
#endif

    FrameRates = new ISwitch[7];
    IUFillSwitch(&FrameRates[0], "30", "30 fps", ISS_ON);
    IUFillSwitch(&FrameRates[1], "25", "25 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[2], "20", "20 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[3], "15", "15 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[4], "10", "10 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[5], "5", "5 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[6], "1", "1 fps", ISS_OFF);

    IUFillSwitchVector(&FrameRateSelection, FrameRates, 7, getDeviceName(), "CAPTURE_FRAME_RATE", "Frame Rate",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    PixelFormats = new ISwitch[6];
    IUFillSwitch(&PixelFormats[0], "uyvy422", "uyvy422", ISS_ON);
    IUFillSwitch(&PixelFormats[1], "yuyv422", "yuyv422", ISS_OFF);
    IUFillSwitch(&PixelFormats[2], "yuv420p", "yuv420p", ISS_OFF);
    IUFillSwitch(&PixelFormats[3], "nv12", "nv12", ISS_OFF);
    IUFillSwitch(&PixelFormats[4], "0rgb", "0rgb", ISS_OFF);
    IUFillSwitch(&PixelFormats[5], "bgr0", "bgr0", ISS_OFF);

    IUFillSwitchVector(&PixelFormatSelection, PixelFormats, 6, getDeviceName(), "INPUT_PIXEL_FORMAT", "PixelFormat",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    VideoSizes = new ISwitch[7];
    IUFillSwitch(&VideoSizes[0], "320x240", "320x240", ISS_OFF);
    IUFillSwitch(&VideoSizes[1], "640x480", "640x480", ISS_ON);
    IUFillSwitch(&VideoSizes[2], "800x600", "800x600", ISS_OFF);
    IUFillSwitch(&VideoSizes[3], "1024x768", "1024x768", ISS_OFF);
    IUFillSwitch(&VideoSizes[4], "1280x720", "1280x720", ISS_OFF);
    IUFillSwitch(&VideoSizes[5], "1280x1024", "1280x1024", ISS_OFF);
    IUFillSwitch(&VideoSizes[6], "1600x1200", "1600x1200", ISS_OFF);

    IUFillSwitchVector(&VideoSizeSelection, VideoSizes, 7, getDeviceName(), "CAPTURE_VIDEO_SIZE", "Video Size",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    IUFillNumber(&VideoAdjustmentsT[0], "BRIGHTNESS", "Brightness", "%.3f", -2.00, 2.00, 0.1, 0.00);
    IUFillNumber(&VideoAdjustmentsT[1], "CONTRAST", "Contrast", "%.3f", 0.00, 2.00, 0.1, 1.00);
    IUFillNumber(&VideoAdjustmentsT[2], "SATURATION", "Saturation", "%.3f", 0.00, 8.00, 0.1, 1.00);
    IUFillNumberVector(&VideoAdjustmentsTP, VideoAdjustmentsT, NARRAY(VideoAdjustmentsT), getDeviceName(), "VIDEO_ADJUSTMENTS",
                       "Video Adjustment Options", IMAGE_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&VideoAdjustmentsTP);

    // Use the V4L2 sensor range when a direct V4L2 device is connected.
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    uint32_t cap = 0;
    cap |= CCD_HAS_STREAMING;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_CAN_ABORT;
    SetCCDCapability(cap);

    // DRIVER_INFO property (for clients that set it)
    IUFillText(&DriverInfoT[0], "DRIVER_NAME", "Name", getDefaultName());
    IUFillText(&DriverInfoT[1], "DRIVER_EXEC", "Exec", "indi_qhy_ccd");
    IUFillText(&DriverInfoT[2], "DRIVER_VERSION", "Version", "0.2");
    IUFillText(&DriverInfoT[3], "DRIVER_INTERFACE", "Interface", "2");
    IUFillTextVector(&DriverInfoTP, DriverInfoT, 4, getDeviceName(), "DRIVER_INFO", "Driver Info", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&DriverInfoTP);

    loadConfig(true, RapidStackingSelection.name);
    loadConfig(true, OutputFormatSelection.name);
    loadConfig(true, PixelSizeTP.name);
    loadConfig(true, InputOptionsTP.name);
    loadConfig(true, SaveRawSP.name);
    loadConfig(true, SaveRawPathTP.name);
#ifdef __linux__
    loadConfig(true, V4L2SubdevPathTP.name);
#endif

    refreshInputDevices();
    refreshInputSources();

    loadingSettings = false;
    return true;
}

// The direct backend exposes one capture-device selector. Actual video nodes
// are enumerated as capture sources below.

bool indi_qhy_v4l2::refreshInputDevices()
{
#ifdef __linux__
    if (CaptureDevices)
    {
        delete[] CaptureDevices;
        CaptureDevices = nullptr;
        deleteProperty(CaptureDeviceSelection.name);
    }

    CaptureDevices = new ISwitch[1];
    IUFillSwitch(&CaptureDevices[0], QHY_V4L2_DIRECT_DEVICE, QHY_V4L2_DIRECT_DEVICE, ISS_ON);
    IUFillSwitchVector(&CaptureDeviceSelection, CaptureDevices, 1, getDeviceName(), "CAPTURE_DEVICE",
                       "Capture Device", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineProperty(&CaptureDeviceSelection);
    return true;
#else
    return false;
#endif
}

bool indi_qhy_v4l2::refreshInputSources()
{
#ifdef __linux__
    if (CaptureSources)
    {
        delete[] CaptureSources;
        CaptureSources = nullptr;
        deleteProperty(CaptureSourceSelection.name);
    }

    std::vector<std::string> v4l2_nodes;
    DIR *dir = opendir("/dev");
    if (dir)
    {
        struct dirent *entry = nullptr;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strncmp(entry->d_name, "video", 5) == 0)
                v4l2_nodes.emplace_back(std::string("/dev/") + entry->d_name);
        }
        closedir(dir);
    }
    std::sort(v4l2_nodes.begin(), v4l2_nodes.end());

    const int sourceNum = static_cast<int>(v4l2_nodes.size());
    if (sourceNum > 0)
    {
        CaptureSources = new ISwitch[sourceNum];
        for (int i = 0; i < sourceNum; ++i)
        {
            const char *node = v4l2_nodes[i].c_str();
            IUFillSwitch(&CaptureSources[i], node, node, videoSource == node ? ISS_ON : ISS_OFF);
        }
        IUFillSwitchVector(&CaptureSourceSelection, CaptureSources, sourceNum, getDeviceName(),
                           "CAPTURE_SOURCE", "Capture Source", CONNECTION_TAB, IP_RW,
                           ISR_1OFMANY, 60, IPS_IDLE);
        defineProperty(&CaptureSourceSelection);
    }

    defineProperty(&InputOptionsTP);
    defineProperty(&FrameRateSelection);
    defineProperty(&PixelFormatSelection);
    defineProperty(&VideoSizeSelection);
    return true;
#else
    return false;
#endif
}

void indi_qhy_v4l2::ISGetProperties(const char *dev)
{
    loadingSettings = true;
    INDI::CCD::ISGetProperties(dev);
    // Ensure DRIVER_INFO is published for clients pushing it early
    defineProperty(&DriverInfoTP);
    loadingSettings = false;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool indi_qhy_v4l2::updateProperties()
{
    loadingSettings = true;

    // Call parent update properties first
    INDI::CCD::updateProperties();

    // Ensure DRIVER_INFO is always visible (for clients that set it)
    defineProperty(&DriverInfoTP);

#ifdef __linux__
    // Publish gain and offset properties after connecting.
    if (isConnected())
    {
        if (use_v4l2_direct)
        {
            // Refresh the hardware ranges before publishing the properties.
            GainT[0].min = static_cast<double>(v4l2_subdev_gain_min);
            GainT[0].max = static_cast<double>(v4l2_subdev_gain_max);
            GainT[0].value = static_cast<double>(v4l2_subdev_gain);

            OffsetT[0].min = static_cast<double>(v4l2_subdev_offset_min);
            OffsetT[0].max = static_cast<double>(v4l2_subdev_offset_max);
            OffsetT[0].value = static_cast<double>(v4l2_subdev_offset);

            defineProperty(&GainTP);
            if (v4l2_offset_supported)
                defineProperty(&OffsetTP);
            else
                deleteProperty(OffsetTP.name);

            DEBUGF(INDI::Logger::DBG_SESSION, "Published Gain range: %d - %d, Offset range: %d - %d",
                   v4l2_subdev_gain_min, v4l2_subdev_gain_max,
                   v4l2_subdev_offset_min, v4l2_subdev_offset_max);
        }
        else
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Gain/Offset controls available only in V4L2 Direct mode");
        }
    }
    else
    {
        // Remove hardware-dependent properties on disconnect.
        deleteProperty(GainTP.name);
        deleteProperty(OffsetTP.name);
    }
#endif

    loadingSettings = false;
    return true;
}

bool indi_qhy_v4l2::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
        return true;

    DEBUGF(INDI::Logger::DBG_SESSION, "Setting number %s", name);

    if (!strcmp(name, PixelSizeTP.name) )
    {
        IUUpdateNumber(&PixelSizeTP, values, names, n);
        pixelSize = IUFindNumber( &PixelSizeTP, "PIXEL_SIZE_um" )->value;
        DEBUGF(INDI::Logger::DBG_SESSION, "New Pixel Size: %f", pixelSize);
        IDSetNumber(&PixelSizeTP, nullptr);
        PixelSizeTP.s = IPS_OK;
        return true;
    }

    if (!strcmp(name, TimeoutOptionsTP.name) )
    {
        IUUpdateNumber(&TimeoutOptionsTP, values, names, n);
        bufferTimeout = IUFindNumber( &TimeoutOptionsTP, "BUFFER_TIMEOUT" )->value;
        DEBUGF(INDI::Logger::DBG_SESSION, "New V4L2 buffer timeout: %.0f us", bufferTimeout);
        IDSetNumber (&TimeoutOptionsTP, nullptr);
        TimeoutOptionsTP.s = IPS_OK;
        return true;
    }

    // StartExposure() synchronizes CCD_EXPOSURE to the V4L2 subdevice.

#ifdef __linux__
    if (!strcmp(name, GainTP.name))
    {
        IUUpdateNumber(&GainTP, values, names, n);
        if (SetCCDGain(GainT[0].value))
        {
            v4l2_subdev_gain = static_cast<int32_t>(GainT[0].value);
            GainTP.s = IPS_OK;
        }
        else
        {
            GainTP.s = IPS_ALERT;
        }
        IDSetNumber(&GainTP, nullptr);
        return true;
    }

    if (!strcmp(name, OffsetTP.name))
    {
        IUUpdateNumber(&OffsetTP, values, names, n);
        if (SetCCDOffset(OffsetT[0].value))
        {
            v4l2_subdev_offset = static_cast<int32_t>(OffsetT[0].value);
            OffsetTP.s = IPS_OK;
        }
        else
        {
            OffsetTP.s = IPS_ALERT;
        }
        IDSetNumber(&OffsetTP, nullptr);
        return true;
    }
#endif

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool indi_qhy_v4l2::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
        return true;

    IText *videoDeviceText = &InputOptionsT[0];
    IText *videoSourceText = &InputOptionsT[1];
    IText *frameRateText = &InputOptionsT[2];
    IText *pixelFormatText = &InputOptionsT[3];
    IText *videoSizeText = &InputOptionsT[4];

    ISwitchVectorProperty *svp = getSwitch(name);
    if(!svp)
        return INDI::CCD::ISNewSwitch(dev, name, states, names, n);

    if (!strcmp(svp->name, CaptureDeviceSelection.name))
    {
        IUUpdateSwitch(&CaptureDeviceSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&CaptureDeviceSelection);
        if (sp)
        {
            //If the videodevice is the same no need to disconnect and update sources
            if(videoDevice != sp->name)
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "Setting device to: %s, Refreshing Sources", sp->name);

                videoDevice = sp->name;
                if(isConnected())
                {
                    DEBUG(INDI::Logger::DBG_SESSION, "Disconnecting now.");
                    DEBUG(INDI::Logger::DBG_SESSION, "Please select a new source to connect to and then Press Connect.");
                    if(Disconnect())
                        setConnected(false, IPS_IDLE);
                }
                IUSaveText(videoDeviceText, sp->name);
                refreshInputSources();
            }
            IDSetText(&InputOptionsTP, nullptr);
            CaptureDeviceSelection.s = IPS_OK;
            IDSetSwitch(&CaptureDeviceSelection, nullptr);
            return true;
        }
        return false;
    }
    if (!strcmp(svp->name, CaptureSourceSelection.name))
    {
        IUUpdateSwitch(&CaptureSourceSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&CaptureSourceSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting source to: %s", sp->name);
            //If they are the same, just set it, if not check if the source can be changed
            if(videoSource == sp->name || ChangeSource(videoDevice, sp->name, frameRate, inputPixelFormat, videoSize))
            {
                IUSaveText(videoSourceText, sp->name);
                IDSetText(&InputOptionsTP, nullptr);
                CaptureSourceSelection.s = IPS_OK;
                IDSetSwitch(&CaptureSourceSelection, nullptr);
                return true;
            }
        }
        return false;
    }
    if (!strcmp(svp->name, FrameRateSelection.name))
    {
        IUUpdateSwitch(&FrameRateSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&FrameRateSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting frame rate to: %u frames per second", atoi(sp->name));
            //If they are the same, just set it, if not check if the frameRate can be changed
            if(frameRate == atoi(sp->name) || ChangeSource(videoDevice, videoSource, atoi(sp->name), inputPixelFormat, videoSize))
            {
                IUSaveText(frameRateText, sp->name);
                IDSetText(&InputOptionsTP, nullptr);
                FrameRateSelection.s = IPS_OK;
                IDSetSwitch(&FrameRateSelection, nullptr);
                return true;
            }
        }
        return false;
    }
    if (!strcmp(svp->name, PixelFormatSelection.name))
    {
        IUUpdateSwitch(&PixelFormatSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&PixelFormatSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting Input Pixel Format to: %s", sp->name);
            //If they are the same, just set it, if not check if the frameRate can be changed
            if(inputPixelFormat == sp->name || ChangeSource(videoDevice, videoSource, frameRate, sp->name, videoSize))
            {
                IUSaveText(pixelFormatText, sp->name);
                IDSetText(&InputOptionsTP, nullptr);
                PixelFormatSelection.s = IPS_OK;
                IDSetSwitch(&PixelFormatSelection, nullptr);
                return true;
            }
        }
        return false;
    }
    if (!strcmp(svp->name, VideoSizeSelection.name))
    {
        IUUpdateSwitch(&VideoSizeSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&VideoSizeSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting video size to: %s", sp->name);
            //If they are the same, just set it, if not check if the video Size can be changed
            if(videoSize == sp->name || ChangeSource(videoDevice, videoSource, frameRate, inputPixelFormat, sp->name))
            {
                IUSaveText(videoSizeText, sp->name);
                IDSetText(&InputOptionsTP, nullptr);
                VideoSizeSelection.s = IPS_OK;
                IDSetSwitch(&VideoSizeSelection, nullptr);
                return true;
            }
        }
        return false;
    }

    if (!strcmp(svp->name, RapidStackingSelection.name))
    {
        IUUpdateSwitch(&RapidStackingSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&RapidStackingSelection);
        if (sp)
        {
            if(!strcmp(sp->name, "Integration"))
            {
                webcamStacking = true;
                averaging = false;
            }
            if(!strcmp(sp->name, "Average"))
            {
                webcamStacking = true;
                averaging = true;
            }
            if(!strcmp(sp->name, "Off"))
            {
                webcamStacking = false;
                averaging = false;
            }
            RapidStackingSelection.s = IPS_OK;
            IDSetSwitch(&RapidStackingSelection, nullptr);
            return true;
        }
        return false;
    }

    if (!strcmp(svp->name, OutputFormatSelection.name))
    {
        IUUpdateSwitch(&OutputFormatSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&OutputFormatSelection);
        if (sp)
        {
            outputFormat = sp->name;
            OutputFormatSelection.s = IPS_OK;
            IDSetSwitch(&OutputFormatSelection, nullptr);
            return true;
        }
        return false;
    }

    if (!strcmp(svp->name, PixelSizeSelection.name))
    {
        IUUpdateSwitch(&PixelSizeSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&PixelSizeSelection);
        if (sp)
        {
            pixelSize = atof(sp->name);
            PixelSizeT[0].value = pixelSize;
            PixelSizeSelection.s = IPS_OK;
            IDSetSwitch(&PixelSizeSelection, nullptr);
            IDSetNumber(&PixelSizeTP, nullptr);
            return true;
        }
        return false;
    }

    if (!strcmp(svp->name, SaveRawSP.name))
    {
        IUUpdateSwitch(&SaveRawSP, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&SaveRawSP);
        if (sp)
        {
            save_raw_enable = !strcmp(sp->name, "ENABLE");
            SaveRawSP.s = IPS_OK;
            IDSetSwitch(&SaveRawSP, nullptr);
            return true;
        }
        return false;
    }

    if (!strcmp(name, RefreshSP.name))
    {
        bool a = refreshInputDevices();
        bool b = refreshInputSources();
        RefreshSP.s = (a && b) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&RefreshSP, nullptr);
        RefreshS[0].s = ISS_OFF;
        return true;
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool indi_qhy_v4l2::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
        return true;

    if (!strcmp(name, InputOptionsTP.name) )
    {
        InputOptionsTP.s = IPS_OK;

        IText *videoDeviceText = IUFindText( &InputOptionsTP, names[0] );
        IText *videoSourceText = IUFindText( &InputOptionsTP, names[1] );
        IText *frameRateText = IUFindText( &InputOptionsTP, names[2] );
        IText *pixelFormatText = IUFindText( &InputOptionsTP, names[3] );
        IText *videoSizeText = IUFindText( &InputOptionsTP, names[4] );

        if (!videoDeviceText || !videoSourceText || !frameRateText || !pixelFormatText || !videoSizeText)
            return false;

        if(ChangeSource(texts[0], texts[1], atoi(texts[2]), texts[3], texts[4]))
        {
            IUSaveText(videoDeviceText, texts[0]);
            IUSaveText(videoSourceText, texts[1]);
            IUSaveText(frameRateText, texts[2]);
            IUSaveText(pixelFormatText, texts[3]);
            IUSaveText(videoSizeText, texts[4]);
            IDSetText (&InputOptionsTP, nullptr);
            return true;
        }
    }

    if (!strcmp(name, DriverInfoTP.name))
    {
        DriverInfoTP.s = IPS_OK;
        for (int i = 0; i < n; i++)
        {
            IText *t = IUFindText(&DriverInfoTP, names[i]);
            if (t)
                IUSaveText(t, texts[i]);
        }
        IDSetText(&DriverInfoTP, nullptr);
        return true;
    }

    if (!strcmp(name, SaveRawPathTP.name) )
    {
        SaveRawPathTP.s = IPS_OK;
        IText *pathT = IUFindText(&SaveRawPathTP, names[0]);
        if (!pathT)
            return false;
        save_raw_path = texts[0];
        IUSaveText(pathT, texts[0]);
        IDSetText(&SaveRawPathTP, nullptr);
        return true;
    }

#ifdef __linux__
    if (!strcmp(name, V4L2SubdevPathTP.name))
    {
        V4L2SubdevPathTP.s = IPS_OK;
        IText *pathT = IUFindText(&V4L2SubdevPathTP, names[0]);
        if (!pathT)
            return false;
        v4l2_subdev_path = texts[0];
        IUSaveText(pathT, texts[0]);
        closeV4L2Subdevice();
        if (openV4L2Subdevice())
        {
            updateV4L2SubdevExposureRange();
            setV4L2Exposure(v4l2_subdev_exposure);
        }
        IDSetText(&V4L2SubdevPathTP, nullptr);
        return true;
    }
#endif

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

//This sets up a single exposure, or if rapid stacking is requested,
//It can also set up a series of exposures till the time runs out.
bool indi_qhy_v4l2::StartExposure(float duration)
{
    DEBUGF(INDI::Logger::DBG_SESSION, "StartExposure called with duration=%.3f, use_v4l2_direct=%d, v4l2_streaming=%d",
           duration, use_v4l2_direct, v4l2_streaming);

    //If the QHY CCD is currently streaming, it cannot capture single exposures
    if (is_streaming || is_capturing)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Device is currently streaming.");
        return false;
    }

    //This resets the stack buffer
    if(webcamStacking)
    {
        if(stackBuffer)
            free(stackBuffer);
        stackBuffer = nullptr;
    }

    // V4L2 direct path: set output characteristics based on negotiated FOURCC
    if (use_v4l2_direct)
    {
        syncV4L2ExposureFromDuration(duration);

        uint32_t fourcc = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat;
        const auto info = describeV4L2Format(fourcc);
        if (fourcc == V4L2_PIX_FMT_RGB24 || fourcc == V4L2_PIX_FMT_YUYV)
        {
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(3);
            v4l2_force_16bit = false;
        }
        else if (info.bitDepth >= 10)
        {
            // RAW formats are delivered in a 16-bit container to INDI/FITS.
            PrimaryCCD.setBPP(16);
            PrimaryCCD.setNAxis(2);
            v4l2_force_16bit = true;
        }
        else
        {
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(2);
            v4l2_force_16bit = false;
        }
    }
    //Set up the stream, if there is an error, return
    if(!setupStreaming())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Error Setting up streaming from camera");
        if (use_v4l2_direct)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 setup failed: fd=%d, buffers=%p, mmap_ptrs=%p, count=%u, streaming=%d",
                   v4l2_fd, v4l2_buffers, v4l2_mmap_ptrs, v4l2_buffer_count, v4l2_streaming);
        }
        return false;
    }

    // For V4L2 direct sources, start stream once at the beginning of exposure
    if (use_v4l2_direct)
    {
        // Queue every buffer before each STREAMON operation.
        requeueAllV4L2Buffers();
        DEBUGF(INDI::Logger::DBG_SESSION, "StartExposure: v4l2_streaming flag is %s, v4l2_fd=%d",
               v4l2_streaming ? "true" : "false", v4l2_fd);
        // Start stream - this is the ONLY place where stream should be started for exposure
        if (!v4l2_streaming)
        {
            enum v4l2_buf_type type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
            DEBUGF(INDI::Logger::DBG_SESSION, "Calling VIDIOC_STREAMON with fd=%d, type=%d", v4l2_fd, type);
            int ret = ioctl(v4l2_fd, VIDIOC_STREAMON, &type);
            DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_STREAMON returned %d, errno=%d (%s)",
                   ret, errno, ret < 0 ? strerror(errno) : "success");
            if (ret < 0)
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "Failed to start V4L2 stream in StartExposure: %s", strerror(errno));
                return false;
            }
            v4l2_streaming = true;
            DEBUG(INDI::Logger::DBG_SESSION, "V4L2 stream started for exposure");
            // Wait a bit for stream to stabilize after starting
            usleep(100000); // 100ms
            if (!discardInitialV4L2Frames(2))
                LOG_WARN("V4L2 initial frame discard did not complete");
        }
        else
        {
            LOG_WARN("V4L2 stream already running at StartExposure - this should not happen");
        }
    }

    //This will ensure that we get the current frame, not some old frame still in the buffer
    if(!flush_frame_buffer())
        DEBUG(INDI::Logger::DBG_SESSION, "Issue in flushing buffer");

    // For V4L2 direct sources, try to synchronously grab at least one fresh frame
    if (use_v4l2_direct)
    {
        // Stream should still be running here
        if (!v4l2_streaming)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "V4L2 stream stopped unexpectedly after flush");
            return false;
        }

        // Try to grab an initial frame, but don't fail if it doesn't work
        // TimerHit will continue trying
        bool initialGrabSuccess = false;
        int tries = 0;
        while (tries < 5 && !initialGrabSuccess)
        {
            if (grabImage())
            {
                initialGrabSuccess = true;
                break;
            }
            usleep((useconds_t)bufferTimeout);
            tries++;
        }
        if (!initialGrabSuccess)
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Initial frame grab failed, will continue in TimerHit");
        }
        // Don't reset gotAnImageAlready here - let TimerHit handle it
    }


    //This sets up the exposure time settings
    ExposureRequest = duration;
    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&ExpStart, nullptr);
    timerID = SetTimer(getCurrentPollingPeriod());
    gotAnImageAlready = false;
    InExposure = true;
    return true;
}

bool indi_qhy_v4l2::AbortExposure()
{
    DEBUG(INDI::Logger::DBG_SESSION, "AbortExposure called");

    // Stop V4L2 stream if it's running
    if (use_v4l2_direct && v4l2_streaming)
    {
        enum v4l2_buf_type type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type) < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Failed to stop V4L2 stream in AbortExposure: %s", strerror(errno));
        }
        else
        {
            DEBUG(INDI::Logger::DBG_SESSION, "V4L2 stream stopped in AbortExposure");
        }
        // Always reset the flag
        v4l2_streaming = false;
    }

    if(stackBuffer)
        free(stackBuffer);
    InExposure = false;
    return true;
}

/**************************************************************************************
** INDI gain control
***************************************************************************************/
bool indi_qhy_v4l2::SetCCDGain(double gain)
{
#ifdef __linux__
    if (use_v4l2_direct)
    {
        int32_t gain_value = static_cast<int32_t>(gain);
        if (setV4L2Gain(gain_value))
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "CCD Gain set to %.0f", gain);
            return true;
        }
        return false;
    }
#endif
    DEBUG(INDI::Logger::DBG_WARNING, "Gain control only available in V4L2 Direct mode");
    return false;
}

/**************************************************************************************
** INDI offset control
***************************************************************************************/
bool indi_qhy_v4l2::SetCCDOffset(double offset)
{
#ifdef __linux__
    if (use_v4l2_direct)
    {
        int32_t offset_value = static_cast<int32_t>(offset);
        if (setV4L2Offset(offset_value))
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "CCD Offset set to %.0f", offset);
            return true;
        }
        return false;
    }
#endif
    DEBUG(INDI::Logger::DBG_WARNING, "Offset control only available in V4L2 Direct mode");
    return false;
}

//This calculates the time left in the exposure.
float indi_qhy_v4l2::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

//This method is called repeatedly during the exposure
//If qhy_v4l2 stacking is happening, it repeatedly calls for images
//It also updates the reported exposure time left
//Finally when time is up, it copies any stack to the primary buffer
//And calls finish exposure to do any subframing necessary.

void indi_qhy_v4l2::TimerHit()
{
    float timeleft;

    if (InExposure)
    {
        if (!isConnected())
        {
            InExposure = false;
            return; //  No need to reset timer if we are not connected anymore
        }

        // For V4L2 direct, stream should already be running from StartExposure
        // If it's not, something went wrong
        if (use_v4l2_direct && !v4l2_streaming)
        {
            LOG_ERROR("V4L2 stream stopped unexpectedly in TimerHit");
            // Don't try to restart - let the exposure fail
        }

        timeleft = CalcTimeLeft();
        PrimaryCCD.setExposureLeft(timeleft);

        // Try to grab image if stacking is enabled or we haven't got one yet
        if(webcamStacking || !gotAnImageAlready)
        {
            if (grabImage())
            {
                // Image grabbed successfully
                DEBUG(INDI::Logger::DBG_DEBUG, "Image grabbed successfully in TimerHit");
            }
            else
            {
                // If grab failed, log but continue
                DEBUGF(INDI::Logger::DBG_DEBUG, "Failed to grab image in TimerHit (timeleft=%.3f), will retry", timeleft);
            }
        }

        // The time left in the "exposure" is less than the time it takes to make an actual exposure
        // or the time left is less than the polling period, so get it now.
        if (timeleft <= 0 || timeleft < (1.0 / frameRate) || timeleft < getCurrentPollingPeriod()/1000.0)
        {
            if(webcamStacking)
                copyFinalStackToPrimaryFrameBuffer();
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            LOG_INFO("Download complete.");
            finishExposure();
            freeMemory();
            // Verify stream was stopped
            if (use_v4l2_direct)
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "After finishExposure: v4l2_streaming=%d", v4l2_streaming);
                if (v4l2_streaming)
                {
                    LOG_ERROR("WARNING: v4l2_streaming still true after finishExposure, forcing reset");
                    v4l2_streaming = false;
                }
            }
            // Stream is stopped in finishExposure() after exposure completes
            // It will be restarted when the next exposure begins
            return;
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

// Downloads the image from the QHY CCD.
//If the image is an RGB, it converts it to Fits RGB
//If rapid stacking is happening, it adds the image to the stack.

bool indi_qhy_v4l2::grabImage()
{
    if(getStreamFrame())
    {
        if (!buffer)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Buffer not allocated in grabImage");
            return false;
        }
        if(PrimaryCCD.getNAxis() == 3)
            convertINDI_RGBtoFITS_RGB(buffer, PrimaryCCD.getFrameBuffer());
        else
            memcpy(PrimaryCCD.getFrameBuffer(), buffer, numBytes);
        if(webcamStacking)
            addToStack();
        gotAnImageAlready = true;
        return true;
    }
    else
    {
        // Don't free memory on failure, just return false
        // Memory will be freed when exposure completes or aborts
        return false;
    }
}

//This adds each image to the running stack
bool indi_qhy_v4l2::addToStack()
{
    if(!stackBuffer)
    {
        stackBuffer = (float *)malloc(numBytes * sizeof(float_t));
        numberOfFramesInStack = 0;
    }

    int w = PrimaryCCD.getXRes()  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    int h = PrimaryCCD.getYRes();

    for (int i = 0; i < w * h; i++)
    {
        int x = i % w;
        int y = i / w;
        if(x >= 0 && y >= 0 && x < w && y < h)
        {
            if(numberOfFramesInStack == 0)
                stackBuffer[i] = getImageDataFloatValue(x, y);
            else
                stackBuffer[i] += getImageDataFloatValue(x, y);
        }
    }
    numberOfFramesInStack++;
    return true;
}

//This gets the pixel value at an x, y position in the image
float indi_qhy_v4l2::getImageDataFloatValue(int x, int y)
{
    int w = PrimaryCCD.getXRes()  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    uint8_t *primaryBuffer = PrimaryCCD.getFrameBuffer();
    if(PrimaryCCD.getBPP() == 8)
        return (float) primaryBuffer[y * w + x];
    else if(PrimaryCCD.getBPP() == 16)
    {
        uint16_t *newBuffer = reinterpret_cast<uint16_t *>(primaryBuffer);
        return (float) newBuffer[y * w + x];
    }
    else
        return 0;
}
//This sets the pixel value at an x, y position in the image
void indi_qhy_v4l2::setImageDataValueFromFloat(int x, int y, float value, bool roundAnswer)
{
    uint8_t *primaryBuffer = PrimaryCCD.getFrameBuffer();
    int w = PrimaryCCD.getXRes() * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    if(!stackBuffer)
        return;
    if(PrimaryCCD.getBPP() == 8)
    {
        int max = std::numeric_limits<uint8_t>::max();
        if (value > max)
            value = max;
        if(roundAnswer)
            primaryBuffer[y * w + x] = round(value);
        else
            primaryBuffer[y * w + x] = value;
    }
    else if(PrimaryCCD.getBPP() == 16)
    {
        int max = std::numeric_limits<uint16_t>::max();
        if (value > max)
            value = max;
        uint16_t *newBuffer = reinterpret_cast<uint16_t *>(primaryBuffer);
        if(roundAnswer)
            newBuffer[y * w + x] = round(value);
        else
            newBuffer[y * w + x] = value;
    }
}

//This will take the final image stack and copy it back to the primary buffer for final download.
void indi_qhy_v4l2::copyFinalStackToPrimaryFrameBuffer()
{
    int w = PrimaryCCD.getXRes()  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    int h = PrimaryCCD.getYRes();
    for (int i = 0; i < w * h; i++)
    {
        int x = i % w;
        int y = i / w;
        if(x >= 0 && y >= 0 && x < w && y < h)
        {
            if(averaging)
                setImageDataValueFromFloat(x, y, round(stackBuffer[i] / numberOfFramesInStack), true);
            else //Integrating
                setImageDataValueFromFloat(x, y, round(stackBuffer[i]), true);
        }
    }

    LOGF_INFO("Final Image is a stack of %u exposures.", numberOfFramesInStack);
}

//This will crop the image to a subframe if desired.
//Then it will send the final image.
void indi_qhy_v4l2::finishExposure()
{
    // Stop V4L2 streaming FIRST, before any image processing
    // This ensures proper timing for the next exposure
    if (use_v4l2_direct && v4l2_streaming)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "finishExposure: stopping stream, v4l2_fd=%d", v4l2_fd);
        enum v4l2_buf_type type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        int ret = ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_STREAMOFF returned %d, errno=%d (%s)",
               ret, errno, ret < 0 ? strerror(errno) : "success");
        if (ret < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Failed to stop V4L2 stream: %s", strerror(errno));
        }
        else
        {
            DEBUG(INDI::Logger::DBG_SESSION, "V4L2 stream stopped after exposure");
        }
        v4l2_streaming = false;

    }

    // Now proceed with image processing and sending
    uint8_t *memptr = PrimaryCCD.getFrameBuffer();
    int w = PrimaryCCD.getXRes();
    int h = PrimaryCCD.getYRes();
    int bpp = PrimaryCCD.getBPP();
    int naxis = PrimaryCCD.getNAxis();
    uint16_t subW = PrimaryCCD.getSubW();
    uint16_t subH = PrimaryCCD.getSubH();

    if ( (subW > 0 && subH > 0) && ((subW < w && subH <= h) || (subH < h && subW <= w)))
    {
        int subX   = PrimaryCCD.getSubX();
        int subY = PrimaryCCD.getSubY();

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

            uint8_t *startR = memptr;
            uint8_t *startG = memptr + (w * h * bpp / 8);
            uint8_t *startB = memptr + (w * h * bpp / 8 * 2);

            for (int i = subY; i < subY + subH; i++)
            {
                memcpy(subR + (i - subY) * lineW, startR + (i * w + subX) * bpp / 8, lineW);
                memcpy(subG + (i - subY) * lineW, startG + (i * w + subX) * bpp / 8, lineW);
                memcpy(subB + (i - subY) * lineW, startB + (i * w + subX) * bpp / 8, lineW);
            }
        }

        PrimaryCCD.setFrameBuffer(memptr);
        PrimaryCCD.setFrameBufferSize(subFrameSize, false);
        PrimaryCCD.setResolution(w, h);
        PrimaryCCD.setFrame(subX, subY, subW, subH);
        PrimaryCCD.setNAxis(naxis);
        PrimaryCCD.setBPP(bpp);

        // Optional raw save
        if (save_raw_enable)
        {
            FILE *fp = fopen(save_raw_path.c_str(), "wb");
            if (fp)
            {
                fwrite(memptr, 1, subFrameSize, fp);
                fclose(fp);
            }
        }

        PrimaryCCD.setImageExtension("fits");
        ExposureComplete(&PrimaryCCD);

        // Restore old pointer and release memory
        PrimaryCCD.setFrameBuffer(memptr);
        PrimaryCCD.setFrameBufferSize(numBytes, false);
    }
    else
    {
        // Optional raw save (full frame)
        if (save_raw_enable)
        {
            FILE *fp = fopen(save_raw_path.c_str(), "wb");
            if (fp)
            {
                fwrite(memptr, 1, numBytes, fp);
                fclose(fp);
            }
        }
        PrimaryCCD.setImageExtension("fits");
        ExposureComplete(&PrimaryCCD);
    }
    // Reset 16-bit request for next operations
    v4l2_force_16bit = false;
}

bool indi_qhy_v4l2::UpdateCCDFrame(int x, int y, int w, int h)
{
    PrimaryCCD.setFrame(x, y, w, h);

    // If using V4L2 direct and crop is supported, set hardware crop
    if (use_v4l2_direct && v4l2_can_crop)
    {
        // Stop streaming temporarily to change crop
        bool was_streaming = v4l2_streaming;
        if (was_streaming)
        {
            enum v4l2_buf_type type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);
            v4l2_streaming = false;
        }

        // Set crop rectangle
        if (setV4L2Crop(x, y, w, h))
        {
            // Update actual frame size from crop
            struct v4l2_rect crop_rect = getV4L2Crop();
            PrimaryCCD.setFrame(crop_rect.left, crop_rect.top, crop_rect.width, crop_rect.height);
        }

        // Restart streaming if it was active
        if (was_streaming)
        {
            enum v4l2_buf_type type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(v4l2_fd, VIDIOC_STREAMON, &type) == 0)
                v4l2_streaming = true;
        }
    }

    return true;
}

void indi_qhy_v4l2::debugTriggered(bool enabled)
{
    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 debug logging %s\n", enabled ? "enabled" : "disabled");
}

//These next several methods handle streaming starting and stopping.

void indi_qhy_v4l2::start_capturing()
{
    if (is_capturing) return;
    is_capturing = true;
    capture_thread = std::thread(RunCaptureThread, this);
}

void indi_qhy_v4l2::stop_capturing()
{
    if (!is_capturing) return;
    is_capturing = false;
    if (std::this_thread::get_id() != capture_thread.get_id())
        capture_thread.join();
}

bool indi_qhy_v4l2::StartStreaming()
{
    if (is_streaming) return true;
    if (!is_capturing) start_capturing();
    is_streaming = true;
    return true;
}

bool indi_qhy_v4l2::StopStreaming()
{
    if (!is_streaming) return true;
    stop_capturing();
    is_streaming = false;
    return true;
}

void indi_qhy_v4l2::RunCaptureThread(indi_qhy_v4l2 *qhy_v4l2)
{
    qhy_v4l2->run_capture();
}

//This is the loop that runs during streaming
//Note that it ONLY supports RGB24 aka INDI_RGB format.
void indi_qhy_v4l2::run_capture()
{
    // Direct V4L2 streaming path
    if (use_v4l2_direct)
    {
        if(!setupStreaming())
            return;
        // Decide pixel format based on device FOURCC
        uint32_t fourcc = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat;
        const auto info = describeV4L2Format(fourcc);
        if (fourcc == V4L2_PIX_FMT_RGB24)
        {
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(3);
            Streamer->setPixelFormat(INDI_RGB);
        }
        else if (info.bitDepth >= 10)
        {
            PrimaryCCD.setBPP(16);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(INDI_MONO);
        }
        else
        {
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(INDI_MONO);
        }

        int w = v4l2_is_mplane ? (int)v4l2_fmt.fmt.pix_mp.width : (int)v4l2_fmt.fmt.pix.width;
        int h = v4l2_is_mplane ? (int)v4l2_fmt.fmt.pix_mp.height : (int)v4l2_fmt.fmt.pix.height;
        Streamer->setSize(w, h);
        PrimaryCCD.setFrame(0, 0, w, h);

        if(!flush_frame_buffer())
            DEBUG(INDI::Logger::DBG_SESSION, "V4L2 Issue in flushing buffer");

        while (is_capturing && is_streaming)
        {
            if(getStreamFrame())
                Streamer->newFrame(buffer, numBytes);
            else
            {
                is_capturing = false;
                is_streaming = false;
            }
        }

        freeMemory();
        DEBUG(INDI::Logger::DBG_SESSION, "Capture thread releasing device (V4L2).");
        return;
    }

    // No non-V4L2 capture backend is supported.
}

//This converts an image from INDI_RGB to FITS_RGB so the FITSViewer can read it.
bool indi_qhy_v4l2::convertINDI_RGBtoFITS_RGB(uint8_t *originalImage, uint8_t *convertedImage)
{
    if(PrimaryCCD.getBPP() == 8)
    {
        int size =  numBytes / 3;
        uint8_t *r, *g, *b;
        r = (convertedImage);
        g = (convertedImage + size);
        b = (convertedImage + size * 2);
        for(int i = 0; i < numBytes; i += 3)
        {
            *r++ = *originalImage++;
            *g++ = *originalImage++;
            *b++ = *originalImage++;
        }
    }
    else if(PrimaryCCD.getBPP() == 16)
    {
        uint16_t *bigOriginalImage = reinterpret_cast<uint16_t *>(originalImage);
        uint16_t *bigConvertedImage = reinterpret_cast<uint16_t *>(convertedImage);
        int size =  numBytes / 2 / 3;
        uint16_t *r, *g, *b;
        r = (bigConvertedImage);
        g = (bigConvertedImage + size);
        b = (bigConvertedImage + size * 2);
        for(int i = 0; i < numBytes / 2; i += 3)
        {
            *r++ = *bigOriginalImage++;
            *g++ = *bigOriginalImage++;
            *b++ = *bigOriginalImage++;
        }
    }
    return true;
}

//This sets up the V4L2 CCD to get images
//It is used for both the streaming and exposing algorithms

bool indi_qhy_v4l2::setupStreaming()
{
    return setupV4L2Streaming();
}

//This gets one image from the camera.
//It is used for both the streaming and exposing algorithms

bool indi_qhy_v4l2::getStreamFrame()
{
    return getStreamFrameV4L2();
}

//This will clear out the frame buffer of any unread frames.
//That way we are sure to get the latest frames when exposing

bool indi_qhy_v4l2::flush_frame_buffer()
{
    return flush_frame_bufferV4L2();
}

//This frees up the resources used for streaming/exposing
void indi_qhy_v4l2::freeMemory()
{
    // Mmap buffers remain owned by the connected V4L2 device and are released
    // by DisconnectV4L2(). Only the temporary converted-frame buffer is freed here.
    if(buffer)
        free(buffer);
    buffer = nullptr;
}

#ifdef __linux__
static inline void yuyv_to_rgb24_line(const uint8_t *src, uint8_t *dst, int width)
{
    for (int x = 0; x < width; x += 2)
    {
        int y0 = src[0];
        int u  = src[1] - 128;
        int y1 = src[2];
        int v  = src[3] - 128;
        src += 4;

        auto clamp = [](int c) { return (uint8_t)(c < 0 ? 0 : (c > 255 ? 255 : c)); };
        int r_add = (int)(1.402 * v);
        int g_add = (int)(-0.344136 * u - 0.714136 * v);
        int b_add = (int)(1.772 * u);

        // pixel 0
        int c0 = y0;
        *dst++ = clamp(c0 + r_add);
        *dst++ = clamp(c0 + g_add);
        *dst++ = clamp(c0 + b_add);
        // pixel 1
        int c1 = y1;
        *dst++ = clamp(c1 + r_add);
        *dst++ = clamp(c1 + g_add);
        *dst++ = clamp(c1 + b_add);
    }
}

bool indi_qhy_v4l2::ConnectToSourceV4L2(std::string source)
{
    use_v4l2_direct = false;

    v4l2_fd = open(source.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (v4l2_fd < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to open %s: %s", source.c_str(), strerror(errno));
        return false;
    }

    struct v4l2_capability cap = {};
    if (ioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_QUERYCAP failed: %s", strerror(errno));
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    unsigned int dev_caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    v4l2_is_mplane = (dev_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
    if (!((dev_caps & V4L2_CAP_VIDEO_CAPTURE) || (dev_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)))
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Device is not a video capture device.");
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }
    if (!(dev_caps & V4L2_CAP_STREAMING))
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Device does not support streaming I/O.");
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    // Read current format
    memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
    v4l2_fmt.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2_fd, VIDIOC_G_FMT, &v4l2_fmt) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_G_FMT failed: %s", strerror(errno));
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    // Re-submit the complete current format even when no size was configured.
    // This lets the V4L2 driver reinitialize its negotiated format.
    unsigned int reqw = 0, reqh = 0;
    const bool hasRequestedSize = sscanf(videoSize.c_str(), "%ux%u", &reqw, &reqh) == 2 && reqw > 0 && reqh > 0;
    if (hasRequestedSize)
    {
        if (!v4l2_is_mplane)
        {
            v4l2_fmt.fmt.pix.width = reqw;
            v4l2_fmt.fmt.pix.height = reqh;
        }
        else
        {
            v4l2_fmt.fmt.pix_mp.width = reqw;
            v4l2_fmt.fmt.pix_mp.height = reqh;
            // Let driver compute these
            v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
            v4l2_fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 0;
        }
    }

    unsigned int fmtWidth = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.width : v4l2_fmt.fmt.pix.width;
    unsigned int fmtHeight = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.height : v4l2_fmt.fmt.pix.height;
    DEBUGF(INDI::Logger::DBG_SESSION, "Applying V4L2 format %ux%u fourcc=0x%08x",
           fmtWidth, fmtHeight,
           v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat);
    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &v4l2_fmt) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_S_FMT failed for %ux%u: %s",
               fmtWidth, fmtHeight, strerror(errno));
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }
    // Re-read actual negotiated format
    memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
    v4l2_fmt.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2_fd, VIDIOC_G_FMT, &v4l2_fmt) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_G_FMT failed after S_FMT: %s",
               strerror(errno));
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    const unsigned int negotiatedWidth = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.width : v4l2_fmt.fmt.pix.width;
    const unsigned int negotiatedHeight = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.height : v4l2_fmt.fmt.pix.height;
    if (negotiatedWidth > 0 && negotiatedHeight > 0)
    {
        char negotiatedSize[32] {};
        snprintf(negotiatedSize, sizeof(negotiatedSize), "%ux%u", negotiatedWidth, negotiatedHeight);
        videoSize = negotiatedSize;
        IUSaveText(&InputOptionsT[4], videoSize.c_str());
    }

    struct v4l2_requestbuffers req = {};
    req.count = 4;
    req.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(v4l2_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_REQBUFS failed: %s", strerror(errno));
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    v4l2_buffer_count = req.count;
    v4l2_buffers = (struct v4l2_buffer*)calloc(v4l2_buffer_count, sizeof(struct v4l2_buffer));
    if (!v4l2_buffers)
    {
        close(v4l2_fd);
        v4l2_fd = -1;
        return false;
    }

    for (unsigned int i = 0; i < v4l2_buffer_count; i++)
    {
        struct v4l2_buffer buf = {};
        buf.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        struct v4l2_plane planes[1];
        memset(planes, 0, sizeof(planes));
        if (v4l2_is_mplane)
        {
            buf.length = 1;
            buf.m.planes = planes;
        }
        if (ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_QUERYBUF failed: %s", strerror(errno));
            freeV4L2Memory();
            close(v4l2_fd);
            v4l2_fd = -1;
            return false;
        }

        size_t length = v4l2_is_mplane ? planes[0].length : buf.length;
        unsigned long offset = v4l2_is_mplane ? planes[0].m.mem_offset : buf.m.offset;
        void *start = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, v4l2_fd, offset);
        if (start == MAP_FAILED)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "mmap failed: %s", strerror(errno));
            freeV4L2Memory();
            close(v4l2_fd);
            v4l2_fd = -1;
            return false;
        }
        v4l2_buffers[i] = buf;
        if (v4l2_mmap_ptrs == nullptr) { v4l2_mmap_ptrs = (void**)calloc(req.count, sizeof(void*)); }
        if (v4l2_mmap_lens == nullptr) { v4l2_mmap_lens = (size_t*)calloc(req.count, sizeof(size_t)); }
        v4l2_mmap_ptrs[i] = start;
        v4l2_mmap_lens[i] = length;

        if (v4l2_is_mplane)
        {
            planes[0].bytesused = planes[0].length;
            buf.m.planes = planes;
            buf.length = 1;
        }
        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_QBUF failed: %s", strerror(errno));
            freeV4L2Memory();
            close(v4l2_fd);
            v4l2_fd = -1;
            return false;
        }
    }

    // Don't start streaming on connect - stream will be started when exposure begins
    v4l2_streaming = false;
    use_v4l2_direct = true;
    videoSource = source;

    // Check crop capabilities
    v4l2_cropcap.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_can_crop = (ioctl(v4l2_fd, VIDIOC_CROPCAP, &v4l2_cropcap) == 0);
    if (v4l2_can_crop)
    {
        v4l2_crop.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_crop.c = v4l2_cropcap.defrect;
        ioctl(v4l2_fd, VIDIOC_S_CROP, &v4l2_crop);
        DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 Crop capabilities: bounds=(%d,%d,%d,%d) defrect=(%d,%d,%d,%d)",
               v4l2_cropcap.bounds.left, v4l2_cropcap.bounds.top,
               v4l2_cropcap.bounds.width, v4l2_cropcap.bounds.height,
               v4l2_cropcap.defrect.left, v4l2_cropcap.defrect.top,
               v4l2_cropcap.defrect.width, v4l2_cropcap.defrect.height);
    }

    // Enumerate device capabilities (for debugging and information)
    enumerateV4L2Formats();
    enumerateV4L2Sizes();
    enumerateV4L2FrameRates();

#ifdef __linux__
    if (openV4L2Subdevice())
    {
        updateV4L2SubdevExposureRange();
        setV4L2Exposure(v4l2_subdev_exposure);

        updateV4L2GainRange();
        getV4L2Gain(&v4l2_subdev_gain);
        updateV4L2OffsetRange();
        if (v4l2_offset_supported)
            getV4L2Offset(&v4l2_subdev_offset);
    }
#endif

    return true;
}

bool indi_qhy_v4l2::DisconnectV4L2()
{
    if (v4l2_fd >= 0)
    {
        if (v4l2_streaming)
        {
    enum v4l2_buf_type type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);
            v4l2_streaming = false;
        }
        freeV4L2Memory();
        close(v4l2_fd);
        v4l2_fd = -1;
    }
    closeV4L2Subdevice();
    use_v4l2_direct = false;
    // Parameters intentionally persist across reconnects. Uncomment this block to restore defaults.
    // v4l2_subdev_exposure = 10000.0;
    // v4l2_subdev_gain = 64;
    // v4l2_subdev_offset = 0;
    return true;
}

void indi_qhy_v4l2::updateV4L2ImageMetadata()
{
#ifdef __linux__
    if (!use_v4l2_direct || v4l2_fd < 0)
        return;

    const uint32_t fourcc = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat
                                           : v4l2_fmt.fmt.pix.pixelformat;
    const int width = v4l2_is_mplane ? static_cast<int>(v4l2_fmt.fmt.pix_mp.width)
                                     : static_cast<int>(v4l2_fmt.fmt.pix.width);
    const int height = v4l2_is_mplane ? static_cast<int>(v4l2_fmt.fmt.pix_mp.height)
                                      : static_cast<int>(v4l2_fmt.fmt.pix.height);
    const auto info = describeV4L2Format(fourcc);

    SetCCDParams(width, height, info.bitDepth, pixelSize, pixelSize);

    uint32_t cap = CCD_HAS_STREAMING | CCD_CAN_SUBFRAME | CCD_CAN_ABORT;
    if (info.bayer)
    {
        cap |= CCD_HAS_BAYER;
        int offsetX = 0;
        int offsetY = 0;
        struct v4l2_selection selection {};
        selection.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
                                          V4L2_BUF_TYPE_VIDEO_CAPTURE;
        selection.target = V4L2_SEL_TGT_CROP;
        if (ioctl(v4l2_fd, VIDIOC_G_SELECTION, &selection) == 0)
        {
            offsetX = selection.r.left & 1;
            offsetY = selection.r.top & 1;
        }
        BayerTP[CFA_OFFSET_X].setText(std::to_string(offsetX).c_str());
        BayerTP[CFA_OFFSET_Y].setText(std::to_string(offsetY).c_str());
        BayerTP[CFA_TYPE].setText(info.cfa);
    }
    SetCCDCapability(cap);

    DEBUGF(INDI::Logger::DBG_SESSION,
           "V4L2 metadata: %dx%d fourcc=%c%c%c%c bpp=%d bayer=%s cfa=%s pixel_size=%.3f",
           width, height,
           fourcc & 0xff, (fourcc >> 8) & 0xff,
           (fourcc >> 16) & 0xff, (fourcc >> 24) & 0xff,
           info.bitDepth, info.bayer ? "yes" : "no",
           info.cfa ? info.cfa : "", pixelSize);
#endif
}

bool indi_qhy_v4l2::setupV4L2Streaming()
{
    if (v4l2_fd < 0)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "V4L2 file descriptor invalid in setupV4L2Streaming");
        return false;
    }

    // Ensure mmap buffers are still valid (they should be, but check anyway)
    if (!v4l2_buffers || !v4l2_mmap_ptrs || v4l2_buffer_count == 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 buffers not initialized: buffers=%p, mmap_ptrs=%p, count=%u",
               v4l2_buffers, v4l2_mmap_ptrs, v4l2_buffer_count);
        return false;
    }

    int w = v4l2_is_mplane ? (int)v4l2_fmt.fmt.pix_mp.width : (int)v4l2_fmt.fmt.pix.width;
    int h = v4l2_is_mplane ? (int)v4l2_fmt.fmt.pix_mp.height : (int)v4l2_fmt.fmt.pix.height;
    uint32_t fourcc = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat;
    const auto info = describeV4L2Format(fourcc);
    const bool isRaw = info.bitDepth >= 10;

    if (v4l2_force_16bit && isRaw)
        numBytes = w * h * 2;
    else if (fourcc == V4L2_PIX_FMT_RGB24)
        numBytes = w * h * 3;
    else if (fourcc == V4L2_PIX_FMT_YUYV)
        numBytes = w * h * 3; // we convert to RGB24 for streaming
    else
        numBytes = w * h;     // 8-bit mono fallback

    if (isRaw)
    {
        const uint32_t bytesPerLine = v4l2_is_mplane ?
            v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline : v4l2_fmt.fmt.pix.bytesperline;
        if (bytesPerLine < static_cast<uint32_t>(w * 2))
        {
            DEBUGF(INDI::Logger::DBG_WARNING,
                   "Packed RAW V4L2 format is not supported yet: bytesperline=%u, expected at least %d",
                   bytesPerLine, w * 2);
            return false;
        }
    }

    // Only allocate buffer if not already allocated or size changed
    if (!buffer || PrimaryCCD.getFrameBufferSize() != numBytes)
    {
        if (buffer)
            free(buffer);
        buffer = (uint8_t*)malloc(numBytes);
        if (!buffer)
            return false;
    }

    PrimaryCCD.setFrameBufferSize(numBytes);
    PrimaryCCD.setResolution(w, h);

    // DO NOT start stream here - it will be started explicitly in StartExposure()
    // This function only sets up buffers and parameters

    return true;
}

bool indi_qhy_v4l2::getStreamFrameV4L2()
{
    if (v4l2_fd < 0)
        return false;

    // Stream should already be running when this is called during exposure
    // Do NOT start stream here
    if (!v4l2_streaming)
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "getStreamFrameV4L2 called but stream not running");
        return false;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(v4l2_fd, &fds);
    struct timeval tv = {0, 0};
    tv.tv_sec = 0;
    tv.tv_usec = (suseconds_t)bufferTimeout;
    int r = select(v4l2_fd + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0)
        return false;

    struct v4l2_buffer buf = {};
    buf.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    struct v4l2_plane planes[1];
    memset(planes, 0, sizeof(planes));
    if (v4l2_is_mplane)
    {
        buf.length = 1;
        buf.m.planes = planes;
    }
    if (ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0)
    {
        if (errno == EAGAIN)
            return false;
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_DQBUF failed: %s", strerror(errno));
        return false;
    }

    void *start = v4l2_mmap_ptrs[buf.index];
    size_t length = v4l2_is_mplane ? planes[0].bytesused : buf.bytesused;
    int w = v4l2_is_mplane ? (int)v4l2_fmt.fmt.pix_mp.width : (int)v4l2_fmt.fmt.pix.width;
    int h = v4l2_is_mplane ? (int)v4l2_fmt.fmt.pix_mp.height : (int)v4l2_fmt.fmt.pix.height;
    uint32_t fourcc = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat;
    if (fourcc == V4L2_PIX_FMT_RGB24)
    {
        size_t expected = (size_t)w * h * 3;
        if (length >= expected)
        {
            memcpy(buffer, start, expected);
            numBytes = (int)expected;
        }
        else
        {
            memcpy(buffer, start, length);
            numBytes = (int)length;
        }
    }
    else if (fourcc == V4L2_PIX_FMT_YUYV)
    {
        const uint8_t *src = reinterpret_cast<const uint8_t*>(start);
        uint8_t *dst = buffer;
        for (int y = 0; y < h; y++)
            yuyv_to_rgb24_line(src + y * w * 2, dst + y * w * 3, w);
        numBytes = w * h * 3;
    }
    else if (describeV4L2Format(fourcc).bitDepth >= 10)
    {
        if (v4l2_force_16bit)
        {
            // Preserve the 16-bit container and copy by bytesperline to account for stride.
            size_t expected = (size_t)w * h * 2;
            uint32_t src_bpl = 0;
            if (!v4l2_is_mplane)
                src_bpl = v4l2_fmt.fmt.pix.bytesperline;
            else
                src_bpl = v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
            if (src_bpl == 0)
                src_bpl = w * 2;

            if (length >= (size_t)src_bpl * h)
            {
                const uint8_t *src8 = reinterpret_cast<const uint8_t*>(start);
                uint8_t *dst8 = buffer;
                size_t dst_bpl = (size_t)w * 2;
                for (int row = 0; row < h; row++)
                {
                    memcpy(dst8 + row * dst_bpl, src8 + row * src_bpl, dst_bpl);
                }
                numBytes = (int)expected;
            }
            else if (length >= expected)
            {
                memcpy(buffer, start, expected);
                numBytes = (int)expected;
            }
            else
            {
                DEBUG(INDI::Logger::DBG_SESSION, "RAW V4L2 container smaller than expected");
                if (v4l2_is_mplane)
                {
                    buf.m.planes = planes;
                    buf.length = 1;
                }
                ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
                return false;
            }
        }
        else
        {
            // 8-bit mono preview/stream
            if (length >= (size_t)w * h * 2)
            {
                const uint16_t *src16 = reinterpret_cast<const uint16_t*>(start);
                uint8_t *dst = buffer;
                int pixels = w * h;
                for (int i = 0; i < pixels; i++)
                {
                    uint16_t v = src16[i];
                    *dst++ = (uint8_t)((v >> 4) & 0xFF);
                }
                numBytes = w * h;
            }
            else if (length >= (size_t)w * h)
            {
                memcpy(buffer, start, (size_t)w * h);
                numBytes = w * h;
            }
            else
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Unexpected RAW V4L2 buffer size");
                if (v4l2_is_mplane)
                {
                    buf.m.planes = planes;
                    buf.length = 1;
                }
                ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
                return false;
            }
        }
    }
    else
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Unsupported V4L2 pixel format");
        if (v4l2_is_mplane)
        {
            buf.m.planes = planes;
            buf.length = 1;
        }
        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
        return false;
    }

    if (v4l2_is_mplane)
    {
        buf.m.planes = planes;
        buf.length = 1;
    }
    if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_QBUF failed: %s", strerror(errno));
        return false;
    }
    return true;
}

bool indi_qhy_v4l2::flush_frame_bufferV4L2()
{
    if (v4l2_fd < 0)
        return true;

    // This function should only be called when stream is already running
    // Do NOT start stream here - it should be started in StartExposure before calling this
    if (!v4l2_streaming)
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "flush_frame_bufferV4L2 called but stream not running, skipping flush");
        return true; // Not an error, just nothing to flush
    }

    int cleared = 0;
    int max_clear = 10; // Limit number of frames to clear to avoid infinite loop
    while (cleared < max_clear)
    {
        struct v4l2_buffer buf = {};
        buf.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        struct v4l2_plane planes[1];
        memset(planes, 0, sizeof(planes));
        if (v4l2_is_mplane)
        {
            buf.length = 1;
            buf.m.planes = planes;
        }

        // Use select with timeout to avoid blocking
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(v4l2_fd, &fds);
        struct timeval tv = {0, 10000}; // 10ms timeout
        int r = select(v4l2_fd + 1, &fds, nullptr, nullptr, &tv);
        if (r <= 0)
            break; // No data available or timeout

        if (ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0)
        {
            if (errno == EAGAIN)
                break; // No buffer available
            // For other errors, log but continue
            DEBUGF(INDI::Logger::DBG_DEBUG, "VIDIOC_DQBUF failed in flush: %s", strerror(errno));
            break;
        }

        if (v4l2_is_mplane)
        {
            buf.m.planes = planes;
            buf.length = 1;
        }
        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "VIDIOC_QBUF failed in flush: %s", strerror(errno));
            break;
        }
        cleared++;
    }
    if (cleared > 0)
        DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 Buffer Cleared of %u stale frames.", cleared);
    return true;
}

bool indi_qhy_v4l2::discardInitialV4L2Frames(unsigned int count)
{
    if (v4l2_fd < 0 || !v4l2_streaming)
        return false;

    unsigned int discarded = 0;
    while (discarded < count)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(v4l2_fd, &fds);
        struct timeval tv = {1, 0};
        int ready = select(v4l2_fd + 1, &fds, nullptr, nullptr, &tv);
        if (ready <= 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION,
                   "Timed out waiting for initial V4L2 frame %u/%u",
                   discarded, count);
            return false;
        }

        struct v4l2_buffer buf = {};
        buf.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        struct v4l2_plane planes[1] = {};
        if (v4l2_is_mplane)
        {
            buf.length = 1;
            buf.m.planes = planes;
        }

        if (ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION,
                   "VIDIOC_DQBUF failed while discarding initial frame: %s",
                   strerror(errno));
            return false;
        }

        if (v4l2_is_mplane)
        {
            buf.length = 1;
            buf.m.planes = planes;
        }
        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION,
                   "VIDIOC_QBUF failed while discarding initial frame: %s",
                   strerror(errno));
            return false;
        }
        discarded++;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,
           "Discarded %u initial V4L2 frames", discarded);
    return true;
}

void indi_qhy_v4l2::freeV4L2Memory()
{
    if (v4l2_buffers)
    {
        for (unsigned int i = 0; i < v4l2_buffer_count; i++)
        {
            void *start = v4l2_mmap_ptrs ? v4l2_mmap_ptrs[i] : nullptr;
            if (start && v4l2_mmap_lens && v4l2_mmap_lens[i] > 0)
                munmap(start, v4l2_mmap_lens[i]);
        }
        free(v4l2_buffers);
        v4l2_buffers = nullptr;
        v4l2_buffer_count = 0;
    }
    if (v4l2_mmap_ptrs)
    {
        free(v4l2_mmap_ptrs);
        v4l2_mmap_ptrs = nullptr;
    }
    if (v4l2_mmap_lens)
    {
        free(v4l2_mmap_lens);
        v4l2_mmap_lens = nullptr;
    }
    if (buffer)
    {
        free(buffer);
        buffer = nullptr;
    }
}

// Enumerate V4L2 pixel formats
bool indi_qhy_v4l2::enumerateV4L2Formats()
{
    if (v4l2_fd < 0)
        return false;

    struct v4l2_fmtdesc fmt_desc;
    memset(&fmt_desc, 0, sizeof(fmt_desc));
    fmt_desc.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

    DEBUG(INDI::Logger::DBG_SESSION, "V4L2 Supported Formats:");
    for (fmt_desc.index = 0; ioctl(v4l2_fd, VIDIOC_ENUM_FMT, &fmt_desc) == 0; fmt_desc.index++)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "  [%u] %s (0x%08x %c%c%c%c)", fmt_desc.index, fmt_desc.description,
               fmt_desc.pixelformat,
               (fmt_desc.pixelformat) & 0xFF,
               (fmt_desc.pixelformat >> 8) & 0xFF,
               (fmt_desc.pixelformat >> 16) & 0xFF,
               (fmt_desc.pixelformat >> 24) & 0xFF);
    }
    return true;
}

// Enumerate V4L2 frame sizes for current format
bool indi_qhy_v4l2::enumerateV4L2Sizes()
{
    if (v4l2_fd < 0)
        return false;

    struct v4l2_frmsizeenum frm_size;
    memset(&frm_size, 0, sizeof(frm_size));
    frm_size.pixel_format = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat;

    DEBUG(INDI::Logger::DBG_SESSION, "V4L2 Supported Sizes:");
    for (frm_size.index = 0; ioctl(v4l2_fd, VIDIOC_ENUM_FRAMESIZES, &frm_size) == 0; frm_size.index++)
    {
        switch (frm_size.type)
        {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                DEBUGF(INDI::Logger::DBG_SESSION, "  [%u] %ux%u", frm_size.index,
                       frm_size.discrete.width, frm_size.discrete.height);
                break;
            case V4L2_FRMSIZE_TYPE_STEPWISE:
                DEBUGF(INDI::Logger::DBG_SESSION, "  Stepwise: %u-%u (step %u) x %u-%u (step %u)",
                       frm_size.stepwise.min_width, frm_size.stepwise.max_width, frm_size.stepwise.step_width,
                       frm_size.stepwise.min_height, frm_size.stepwise.max_height, frm_size.stepwise.step_height);
                break;
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                DEBUGF(INDI::Logger::DBG_SESSION, "  Continuous: %u-%u x %u-%u",
                       frm_size.stepwise.min_width, frm_size.stepwise.max_width,
                       frm_size.stepwise.min_height, frm_size.stepwise.max_height);
                break;
        }
    }
    return true;
}

// Enumerate V4L2 frame rates for current format and size
bool indi_qhy_v4l2::enumerateV4L2FrameRates()
{
    if (v4l2_fd < 0)
        return false;

    struct v4l2_frmivalenum frm_ival;
    memset(&frm_ival, 0, sizeof(frm_ival));
    frm_ival.pixel_format = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.pixelformat : v4l2_fmt.fmt.pix.pixelformat;
    frm_ival.width = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.width : v4l2_fmt.fmt.pix.width;
    frm_ival.height = v4l2_is_mplane ? v4l2_fmt.fmt.pix_mp.height : v4l2_fmt.fmt.pix.height;

    DEBUG(INDI::Logger::DBG_SESSION, "V4L2 Supported Frame Rates:");
    for (frm_ival.index = 0; ioctl(v4l2_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frm_ival) == 0; frm_ival.index++)
    {
        switch (frm_ival.type)
        {
            case V4L2_FRMIVAL_TYPE_DISCRETE:
                DEBUGF(INDI::Logger::DBG_SESSION, "  [%u] %u/%u fps", frm_ival.index,
                       frm_ival.discrete.denominator, frm_ival.discrete.numerator);
                break;
            case V4L2_FRMIVAL_TYPE_STEPWISE:
                DEBUGF(INDI::Logger::DBG_SESSION, "  Stepwise: %u/%u - %u/%u (step %u/%u)",
                       frm_ival.stepwise.min.denominator, frm_ival.stepwise.min.numerator,
                       frm_ival.stepwise.max.denominator, frm_ival.stepwise.max.numerator,
                       frm_ival.stepwise.step.denominator, frm_ival.stepwise.step.numerator);
                break;
            case V4L2_FRMIVAL_TYPE_CONTINUOUS:
                DEBUGF(INDI::Logger::DBG_SESSION, "  Continuous: %u/%u - %u/%u",
                       frm_ival.stepwise.min.denominator, frm_ival.stepwise.min.numerator,
                       frm_ival.stepwise.max.denominator, frm_ival.stepwise.max.numerator);
                break;
        }
    }
    return true;
}

// Set V4L2 pixel format
bool indi_qhy_v4l2::setV4L2Format(uint32_t pixelformat)
{
    if (v4l2_fd < 0 || v4l2_streaming)
        return false;

    struct v4l2_format new_fmt = v4l2_fmt;
    if (v4l2_is_mplane)
        new_fmt.fmt.pix_mp.pixelformat = pixelformat;
    else
        new_fmt.fmt.pix.pixelformat = pixelformat;

    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &new_fmt) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to set V4L2 format: %s", strerror(errno));
        return false;
    }

    v4l2_fmt = new_fmt;
    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 format set to 0x%08x", pixelformat);
    return true;
}

// Set V4L2 frame size
bool indi_qhy_v4l2::setV4L2Size(unsigned int width, unsigned int height)
{
    if (v4l2_fd < 0 || v4l2_streaming)
        return false;

    struct v4l2_format new_fmt = v4l2_fmt;
    if (v4l2_is_mplane)
    {
        new_fmt.fmt.pix_mp.width = width;
        new_fmt.fmt.pix_mp.height = height;
        new_fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
        new_fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 0;
    }
    else
    {
        new_fmt.fmt.pix.width = width;
        new_fmt.fmt.pix.height = height;
    }

    if (ioctl(v4l2_fd, VIDIOC_S_FMT, &new_fmt) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to set V4L2 size: %s", strerror(errno));
        return false;
    }

    v4l2_fmt = new_fmt;
    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 size set to %ux%u", width, height);
    return true;
}

// Set V4L2 frame rate
bool indi_qhy_v4l2::setV4L2FrameRate(unsigned int numerator, unsigned int denominator)
{
    if (v4l2_fd < 0)
        return false;

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(v4l2_fd, VIDIOC_G_PARM, &parm) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_G_PARM failed: %s", strerror(errno));
        return false;
    }

    if (v4l2_is_mplane)
    {
        parm.parm.capture.timeperframe.numerator = numerator;
        parm.parm.capture.timeperframe.denominator = denominator;
    }
    else
    {
        parm.parm.capture.timeperframe.numerator = numerator;
        parm.parm.capture.timeperframe.denominator = denominator;
    }

    if (ioctl(v4l2_fd, VIDIOC_S_PARM, &parm) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to set V4L2 frame rate: %s", strerror(errno));
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 frame rate set to %u/%u", numerator, denominator);
    return true;
}

bool indi_qhy_v4l2::setV4L2Exposure(double value)
{
#ifdef __linux__
    v4l2_subdev_exposure = value;
    if (!openV4L2Subdevice())
        return false;

    ensureManualExposureMode();

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ctrl.value = static_cast<int32_t>(std::llround(value));

    if (ioctl(v4l2_subdev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to set sub-device exposure on %s: %s",
               v4l2_subdev_path.c_str(), strerror(errno));
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,
           "V4L2 absolute exposure set to %d (100us units)", ctrl.value);
    return true;
#else
    return false;
#endif
}

// Query V4L2 control
bool indi_qhy_v4l2::queryV4L2Control(unsigned int ctrl_id, struct v4l2_queryctrl *queryctrl)
{
    if (v4l2_fd < 0 || !queryctrl)
        return false;

    memset(queryctrl, 0, sizeof(*queryctrl));
    queryctrl->id = ctrl_id;

    if (ioctl(v4l2_fd, VIDIOC_QUERYCTRL, queryctrl) < 0)
    {
        if (errno != EINVAL)
            DEBUGF(INDI::Logger::DBG_SESSION, "VIDIOC_QUERYCTRL failed for 0x%08x: %s", ctrl_id, strerror(errno));
        return false;
    }

    return true;
}

// Set V4L2 control value
bool indi_qhy_v4l2::setV4L2Control(unsigned int ctrl_id, int32_t value)
{
    if (v4l2_fd < 0)
        return false;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = ctrl_id;
    ctrl.value = value;

    if (ioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to set V4L2 control 0x%08x: %s", ctrl_id, strerror(errno));
        return false;
    }

    return true;
}

// Get V4L2 control value
bool indi_qhy_v4l2::getV4L2Control(unsigned int ctrl_id, int32_t *value)
{
    if (v4l2_fd < 0 || !value)
        return false;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = ctrl_id;

    if (ioctl(v4l2_fd, VIDIOC_G_CTRL, &ctrl) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to get V4L2 control 0x%08x: %s", ctrl_id, strerror(errno));
        return false;
    }

    *value = ctrl.value;
    return true;
}

bool indi_qhy_v4l2::ensureManualExposureMode()
{
#ifdef __linux__
    if (!openV4L2Subdevice())
        return false;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_EXPOSURE_AUTO;
    ctrl.value = V4L2_EXPOSURE_MANUAL;

    if (ioctl(v4l2_subdev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        if (errno != EINVAL)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Failed to force manual exposure mode on %s: %s",
                   v4l2_subdev_path.c_str(), strerror(errno));
            return false;
        }
        // Control not supported: not fatal
    }
    return true;
#else
    return false;
#endif
}

bool indi_qhy_v4l2::openV4L2Subdevice()
{
#ifdef __linux__
    if (v4l2_subdev_path.empty())
        return false;
    if (v4l2_subdev_fd >= 0)
        return true;

    v4l2_subdev_fd = open(v4l2_subdev_path.c_str(), O_RDWR);
    if (v4l2_subdev_fd < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to open V4L2 sub-device %s: %s",
               v4l2_subdev_path.c_str(), strerror(errno));
        return false;
    }

    updateV4L2SubdevExposureRange();
    return true;
#else
    return false;
#endif
}

void indi_qhy_v4l2::closeV4L2Subdevice()
{
#ifdef __linux__
    if (v4l2_subdev_fd >= 0)
    {
        close(v4l2_subdev_fd);
        v4l2_subdev_fd = -1;
    }
#endif
}

void indi_qhy_v4l2::updateV4L2SubdevExposureRange()
{
#ifdef __linux__
    if (v4l2_subdev_fd < 0)
        return;

    struct v4l2_queryctrl query = {};
    query.id = V4L2_CID_EXPOSURE_ABSOLUTE;

    if (ioctl(v4l2_subdev_fd, VIDIOC_QUERYCTRL, &query) == 0)
    {
        v4l2_subdev_exposure_min = query.minimum;
        v4l2_subdev_exposure_max = query.maximum;

        const double minSeconds = query.minimum / 10000.0;
        const double maxSeconds = query.maximum / 10000.0;
        const double stepSeconds = std::max(0.0001, query.step / 10000.0);
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE",
                                 minSeconds, maxSeconds, stepSeconds, false);

        if (v4l2_subdev_exposure < v4l2_subdev_exposure_min)
            v4l2_subdev_exposure = v4l2_subdev_exposure_min;
        if (v4l2_subdev_exposure > v4l2_subdev_exposure_max)
            v4l2_subdev_exposure = v4l2_subdev_exposure_max;

        DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 absolute exposure range updated: %.0f - %.0f (100us units) (current: %.0f)",
               v4l2_subdev_exposure_min, v4l2_subdev_exposure_max, v4l2_subdev_exposure);
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Failed to query V4L2 subdev exposure range: %s", strerror(errno));
    }
#endif
}

void indi_qhy_v4l2::syncV4L2ExposureFromDuration(double duration)
{
#ifdef __linux__
    if (!use_v4l2_direct)
        return;

    if (duration <= 0)
        duration = std::max(0.0001, v4l2_subdev_exposure_min / 10000.0);

    // The exposure range is queried from the subdevice by updateV4L2SubdevExposureRange().
    double minValue = v4l2_subdev_exposure_min;
    double maxValue = v4l2_subdev_exposure_max;

    // V4L2_CID_EXPOSURE_ABSOLUTE uses 100 microsecond units.
    double exposure100us = duration * 10000.0;
    int32_t target = static_cast<int32_t>(std::round(exposure100us));

    if (target < static_cast<int32_t>(minValue))
        target = static_cast<int32_t>(minValue);
    if (target > static_cast<int32_t>(maxValue))
        target = static_cast<int32_t>(maxValue);

    if (!setV4L2Exposure(target))
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to auto-sync V4L2 exposure for duration %.3f s", duration);
    }
    else
    {
        v4l2_subdev_exposure = target;
        DEBUGF(INDI::Logger::DBG_SESSION,
               "Auto-synced V4L2 absolute exposure to %d (100us units) for duration %.3f s (range: %.0f-%.0f)",
               target, duration, minValue, maxValue);
    }
#else
    (void)duration;
#endif
}

// Set V4L2 crop rectangle
bool indi_qhy_v4l2::setV4L2Crop(int x, int y, int w, int h)
{
    if (v4l2_fd < 0 || !v4l2_can_crop)
        return false;

    v4l2_crop.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_crop.c.left = x;
    v4l2_crop.c.top = y;
    v4l2_crop.c.width = w;
    v4l2_crop.c.height = h;

    if (ioctl(v4l2_fd, VIDIOC_S_CROP, &v4l2_crop) < 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to set V4L2 crop: %s", strerror(errno));
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 crop set to (%d,%d) %dx%d", x, y, w, h);
    return true;
}

// Get V4L2 crop rectangle
struct v4l2_rect indi_qhy_v4l2::getV4L2Crop()
{
    struct v4l2_rect rect = {0, 0, 0, 0};

    if (v4l2_fd < 0 || !v4l2_can_crop)
        return rect;

    v4l2_crop.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2_fd, VIDIOC_G_CROP, &v4l2_crop) == 0)
        rect = v4l2_crop.c;

    return rect;
}
#endif

bool indi_qhy_v4l2::requeueAllV4L2Buffers()
{
#ifdef __linux__
    if (v4l2_fd < 0 || !v4l2_buffers || v4l2_buffer_count == 0)
        return false;

    unsigned int success = 0, busy = 0;
    for (unsigned int i = 0; i < v4l2_buffer_count; i++)
    {
        struct v4l2_buffer buf = {};
        buf.type = v4l2_is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        struct v4l2_plane planes[1];
        memset(planes, 0, sizeof(planes));
        if (v4l2_is_mplane)
        {
            buf.length = 1;
            buf.m.planes = planes;
        }
        if (ioctl(v4l2_fd, VIDIOC_QBUF, &buf) == 0)
        {
            success++;
            continue;
        }
        if (errno == EBUSY)
        {
            // The buffer is already queued.
            busy++;
            continue;
        }
        DEBUGF(INDI::Logger::DBG_DEBUG, "VIDIOC_QBUF (requeue) failed for idx=%u: %s", i, strerror(errno));
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "Re-queued V4L2 buffers: success=%u, busy=%u, total=%u",
           success, busy, v4l2_buffer_count);
    // Non-fatal dequeue errors do not invalidate the stream.
    return true;
#else
    return true;
#endif
}

bool indi_qhy_v4l2::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &CaptureDeviceSelection);
    IUSaveConfigSwitch(fp, &RapidStackingSelection);
    IUSaveConfigSwitch(fp, &OutputFormatSelection);
    IUSaveConfigNumber(fp, &PixelSizeTP);
    IUSaveConfigText(fp, &InputOptionsTP);
    IUSaveConfigNumber(fp, &TimeoutOptionsTP);
#ifdef __linux__
    IUSaveConfigText(fp, &V4L2SubdevPathTP);
#endif

    return true;
}

/**************************************************************************************
** V4L2 gain and offset controls
***************************************************************************************/

void indi_qhy_v4l2::updateV4L2GainRange()
{
#ifdef __linux__
    if (v4l2_subdev_fd < 0)
        return;

    struct v4l2_queryctrl query = {};
    query.id = V4L2_CID_ANALOGUE_GAIN;

    if (ioctl(v4l2_subdev_fd, VIDIOC_QUERYCTRL, &query) == 0)
    {
        v4l2_subdev_gain_min = query.minimum;
        v4l2_subdev_gain_max = query.maximum;

        if (v4l2_subdev_gain < v4l2_subdev_gain_min)
            v4l2_subdev_gain = v4l2_subdev_gain_min;
        if (v4l2_subdev_gain > v4l2_subdev_gain_max)
            v4l2_subdev_gain = v4l2_subdev_gain_max;

        DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 subdev gain range updated: %d - %d (current: %d)",
               v4l2_subdev_gain_min, v4l2_subdev_gain_max, v4l2_subdev_gain);
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Failed to query V4L2 subdev gain range: %s", strerror(errno));
    }
#endif
}

bool indi_qhy_v4l2::setV4L2Gain(int32_t gain)
{
#ifdef __linux__
    if (v4l2_subdev_fd < 0)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "V4L2 subdevice not open");
        return false;
    }

    if (gain < v4l2_subdev_gain_min)
        gain = v4l2_subdev_gain_min;
    if (gain > v4l2_subdev_gain_max)
        gain = v4l2_subdev_gain_max;

    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_ANALOGUE_GAIN;
    ctrl.value = gain;

    if (ioctl(v4l2_subdev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Failed to set V4L2 gain to %d: %s", gain, strerror(errno));
        return false;
    }

    v4l2_subdev_gain = gain;
    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 gain set to %d", gain);
    return true;
#else
    (void)gain;
    return false;
#endif
}

bool indi_qhy_v4l2::getV4L2Gain(int32_t *gain)
{
#ifdef __linux__
    if (v4l2_subdev_fd < 0 || !gain)
        return false;

    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_ANALOGUE_GAIN;

    if (ioctl(v4l2_subdev_fd, VIDIOC_G_CTRL, &ctrl) < 0)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Failed to get V4L2 gain: %s", strerror(errno));
        return false;
    }

    *gain = ctrl.value;
    v4l2_subdev_gain = ctrl.value;
    return true;
#else
    (void)gain;
    return false;
#endif
}

void indi_qhy_v4l2::updateV4L2OffsetRange()
{
#ifdef __linux__
    v4l2_offset_supported = false;
    if (v4l2_subdev_fd < 0)
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "V4L2 subdevice not open; offset control unavailable");
        return;
    }

    struct v4l2_queryctrl query = {};
    query.id = V4L2_CID_BLACK_LEVEL;
    if (ioctl(v4l2_subdev_fd, VIDIOC_QUERYCTRL, &query) < 0 ||
        (query.flags & V4L2_CTRL_FLAG_DISABLED))
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "V4L2 black-level control unavailable: %s", strerror(errno));
        return;
    }

    v4l2_offset_supported = true;
    v4l2_subdev_offset_control_id = V4L2_CID_BLACK_LEVEL;
    v4l2_subdev_offset_min = query.minimum;
    v4l2_subdev_offset_max = query.maximum;
    v4l2_subdev_offset_step = query.step > 0 ? query.step : 1;

    struct v4l2_control ctrl = {};
    ctrl.id = v4l2_subdev_offset_control_id;
    if (ioctl(v4l2_subdev_fd, VIDIOC_G_CTRL, &ctrl) == 0)
        v4l2_subdev_offset = ctrl.value;

    DEBUGF(INDI::Logger::DBG_SESSION,
           "V4L2 black level range: min=%d max=%d step=%d current=%d",
           v4l2_subdev_offset_min, v4l2_subdev_offset_max,
           v4l2_subdev_offset_step, v4l2_subdev_offset);
#else
    v4l2_offset_supported = false;
#endif
}

// Set the offset through the standard V4L2 black-level control.
bool indi_qhy_v4l2::setV4L2Offset(int32_t offset)
{
#ifdef __linux__
    if (v4l2_subdev_fd < 0 || !v4l2_offset_supported)
        return false;

    if (offset < v4l2_subdev_offset_min)
        offset = v4l2_subdev_offset_min;
    if (offset > v4l2_subdev_offset_max)
        offset = v4l2_subdev_offset_max;

    struct v4l2_control ctrl = {};
    ctrl.id = v4l2_subdev_offset_control_id;
    ctrl.value = offset;
    if (ioctl(v4l2_subdev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Failed to set V4L2 black level to %d: %s",
               offset, strerror(errno));
        return false;
    }

    v4l2_subdev_offset = offset;
    DEBUGF(INDI::Logger::DBG_SESSION, "V4L2 black level set to %d", offset);
    return true;
#else
    (void)offset;
    return false;
#endif
}

bool indi_qhy_v4l2::getV4L2Offset(int32_t *offset)
{
#ifdef __linux__
    if (v4l2_subdev_fd < 0 || !v4l2_offset_supported || !offset)
        return false;

    struct v4l2_control ctrl = {};
    ctrl.id = v4l2_subdev_offset_control_id;
    if (ioctl(v4l2_subdev_fd, VIDIOC_G_CTRL, &ctrl) < 0)
        return false;

    *offset = ctrl.value;
    v4l2_subdev_offset = *offset;
    return true;
#else
    (void)offset;
    return false;
#endif
}
