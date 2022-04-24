/*
INDI Webcam CCD Driver

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

#include "indi_webcam.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "libavutil/dict.h"
#ifdef __cplusplus
}
#endif

#include "config.h"

static std::unique_ptr<indi_webcam> webcam(new indi_webcam());

//Note this is how we get information about AVFoundation Devices
//FFMpeg does not provide a way to programmatically get them, but there is a way to log them.
//So we capture the logging and parse it to get the list of devices.
//This is the function FFMpeg calls.
void logDevices(void *ptr, int level, const char *fmt, va_list vargs)
{
    int printPrefix = 1;
    int lineSize = 1024;
    char *lineBuffer = (char *)malloc(1024);
    av_log_format_line(ptr, level, fmt, vargs, lineBuffer, lineSize, &printPrefix);
    if(checkingDevices)
    {
        if(allDevicesFound)
        {
            if(lineBuffer)
                free(lineBuffer);
            return;
        }
        if(strstr(lineBuffer, "AVFoundation video devices:") != nullptr)
        {
            if(lineBuffer)
                free(lineBuffer);
            return;
        }
        if(strstr(lineBuffer, "AVFoundation audio devices:") != nullptr)
        {
            allDevicesFound = true;
            if(lineBuffer)
                free(lineBuffer);
            return;
        }
        std::string line = lineBuffer;

        // This will remove the avfoundation label and hashcode and just leave the number in brackets and device name
        std::string device = line.substr(line.find_first_of("]") + 2);
        listOfSources.push_back(device);
    }
    else
    {
        if(av_log_get_level() >= level)
            fprintf(stderr, "%s", lineBuffer);
    }
    if(lineBuffer)
        free(lineBuffer);
}

//This method finds AVFoundation Devices.  Please see description above.
void indi_webcam::findAVFoundationVideoSources()
{
    //Need to disconnect streaming if it is running
    bool was_streaming = false;
    if(is_streaming)
    {
        was_streaming = true;
        StopStreaming();
    }

    //Need to disconnect the source to probe the streams
    if(isConnected())
    {
        avcodec_close(pCodecCtx);
        avformat_close_input(&pFormatCtx);
    }
    else
    {
        //This appears to be needed for the list to get updated if a device was not connected
        //But it is fine to do without this the first time, so it isn't done in that case.
        if(!connectedOnce)
            connectedOnce = true;
        else
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Briefly connecting to avfoundation to update the source list");
            if(ConnectToSource("avfoundation", "default", frameRate, videoSize, inputPixelFormat, "Not using IP Camera"))
                DEBUG(INDI::Logger::DBG_SESSION, "Source List Updated");
            avcodec_close(pCodecCtx);
            avformat_close_input(&pFormatCtx);
        }
    }

    listOfSources.clear();
    allDevicesFound = false;
    checkingDevices = true;

    //This is how we tell FFMpeg to call our method to log the sources.
    av_log_set_callback(logDevices);

    //This set of commands should open the avfoundation device to list its sources
    AVDictionary* options = nullptr;
    av_dict_set(&options, "list_devices", "true", 0);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    AVInputFormat *iformat = av_find_input_format("avfoundation");
#else
    const AVInputFormat *iformat = av_find_input_format("avfoundation");
#endif
    avformat_open_input(&pFormatCtx, "", iformat, &options);
    avformat_close_input(&pFormatCtx);
    checkingDevices = false;

    //Need to hook back up the source if it should be connected
    if(isConnected())
        ConnectToSource(videoDevice, videoSource, frameRate, videoSize, inputPixelFormat, url);

    //Hook back up streaming if it should be running
    if(was_streaming)
        StartStreaming();
}

indi_webcam::indi_webcam()
{
    setVersion(WEBCAM_VERSION_MAJOR, WEBCAM_VERSION_MINOR);

    pFormatCtx = nullptr;
    pCodecCtx = nullptr;
    pCodec = nullptr;
    optionsDict = nullptr;
    pFrame = nullptr;
    pFrameOUT = nullptr;
    sws_ctx = nullptr;
    buffer = nullptr;

    // These calls are depreciated, but are required for some older FFMPEG distributions on Linux
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    //This registers all devices
    avdevice_register_all();
    //This registers all codecs
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    avcodec_register_all();
#endif

    //This call is required for the IP Camera functionality to work
    avformat_network_init();

    //setting default values

#ifdef __linux__
    videoDevice = "video4linux2,v4l2";
    videoSource = "/dev/video0";
    inputPixelFormat = "yuv420p";
#elif __APPLE__
    videoDevice = "avfoundation";
    videoSource = "0";
    inputPixelFormat = "uyvy422";
#else
    videoDevice = ""
    videoSource = "";
#endif

    frameRate = 30;
    videoSize = "640x480";
    webcamStacking = false;
    averaging = false;
    outputFormat = "8 bit RGB";

    protocol = "HTTP";
    IPAddress = "xxx.xxx.x.xxx";
    port = "xxxx";
    username = "iphone";
    password = "password";

    ffmpegTimeout = 1000000;
    bufferTimeout = 10000;
    pixelSize = 5.0;

    //Creating the format context.
    pFormatCtx = nullptr;
    pFormatCtx = avformat_alloc_context();
}

indi_webcam::~indi_webcam()
{
    if(pFormatCtx)
        free(pFormatCtx);
}


/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool indi_webcam::Connect()
{
    bool rc = false;

    ISwitchVectorProperty *connect = getSwitch("CONNECTION");
    if (connect)
        connect->s = IPS_BUSY;

    if(videoDevice == "IP Camera")
        DEBUGF(INDI::Logger::DBG_SESSION, "Trying to connect to IP Camera at: %s", url.c_str());
    else
        DEBUGF(INDI::Logger::DBG_SESSION, "Trying to connect to: %s, on device: %s with %s at %u frames per second",
               videoSource.c_str(), videoDevice.c_str(), videoSize.c_str(), frameRate);

    rc = ConnectToSource(videoDevice, videoSource, frameRate, videoSize, inputPixelFormat, url.c_str());
    return rc;
}

//This is the code that we use for FFMpeg to set up an input, connect to it, and set up the correct codecs.
bool indi_webcam::ConnectToSource(std::string device, std::string source, int framerate, std::string videosize, std::string inputpixelformat,
                                  std::string urlSource)
{
    char stringFrameRate[16];
    snprintf(stringFrameRate, 16, "%u", framerate);
    char stringffmpegTimeout[16];
    snprintf(stringffmpegTimeout, 16, "%.0f", ffmpegTimeout);
    if(isConnected())
    {
        avcodec_close(pCodecCtx);
        avformat_close_input(&pFormatCtx);
    }

    AVDictionary* options = nullptr;
    av_dict_set(&options, "timeout", stringffmpegTimeout, 0); //Timeout for open_input and for read_frame.  VERY important.

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    AVInputFormat *iformat = nullptr;
#else
    const AVInputFormat *iformat = nullptr;
#endif

    if(device != "IP Camera")
    {
        //These items are not used by an IP Camera
        av_dict_set(&options, "framerate", stringFrameRate, 0);
        av_dict_set(&options, "video_size", videosize.c_str(), 0);
        av_dict_set(&options, "pixel_format", inputpixelformat.c_str(), 0);
        iformat = av_find_input_format(device.c_str());
    }
    DEBUG(INDI::Logger::DBG_SESSION, "Attempting to connect");

    //This opens the input to get it ready for streaming.
    //Warning:  It is possible for the avformat_open_input command to hang if the camera is there and does not respond.
    //I have not yet solved this problem.  It does not happen often.
    int connect = -1;
    if(device == "IP Camera")
        connect = avformat_open_input(&pFormatCtx, urlSource.c_str(), nullptr, &options );
    else
        connect = avformat_open_input(&pFormatCtx, source.c_str(), iformat, &options );
    if (connect != 0)
    {
        char errbuff[200];
        av_make_error_string(errbuff, 200, connect);
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to open source. Check your settings: %s", errbuff);
        return false;
    }

    //pFormatCtx->max_analyze_duration = 10000000;

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        return false;
    }

    //This will attempt to find a video stream in the input.
    videoStream = -1;
    for(unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoStream = i;
    if(videoStream == -1)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Failed to get a video stream.");
        return false;
    }

    //Find an appropriate decoder and then
    // Allocate a pointer to the codec context for the video stream
    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);

    //If an appropriate codec was not found, abort the connection
    if(pCodec == nullptr)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Unsupported codec.");
        return false;
    }

    //Attempt to open the codec.  If that fails, abort the connection.
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Failed to open codec.");
        return false;
    }

    //Set the initial parameters for the CCD.
    SetCCDParams(pCodecCtx->width, pCodecCtx->height, 8, pixelSize, pixelSize);

    DEBUG(INDI::Logger::DBG_SESSION, "Connection Successful.");
    return true;

}

//This should be run if the source was somehow disconnected to try to reconnect it.
//It will make 10 attempts.
//It returns true if it was successful.
bool indi_webcam::reconnectSource()
{
    int attempt = 0;
    while(attempt < 10)
    {
        if(ConnectToSource(videoDevice, videoSource, frameRate, videoSize, inputPixelFormat, url))
            return true;
    }
    //All 10 attempts resulted in failure.
    return false;
}

//This is the method that should be called to change the streaming device, source, framerate, or video size
//If it was already connected, it will attempt a connection with the new settings and if it is not successful, it will revert to the old ones.
//It should be safe to use while streaming or between image captures because it will pause them and return them to normal afterwards.
bool indi_webcam::ChangeSource(std::string newDevice, std::string newSource, int newFramerate, std::string newInputPixelFormat, std::string newVideosize)
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
    if(ConnectToSource(newDevice, newSource, newFramerate, newVideosize, newInputPixelFormat, url) == false)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Connection was NOT successful");
        DEBUGF(INDI::Logger::DBG_SESSION, "Changing back to: %s, on device: %s with %s at %u frames per second",
               videoSource.c_str(), videoDevice.c_str(), videoSize.c_str(), frameRate);
        ConnectToSource(videoDevice, videoSource, frameRate, videoSize, inputPixelFormat, url);
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

//This is the method that should be called to change the streaming device, source, framerate, or video size
//If it was already connected, it will attempt a connection with the new settings and if it is not successful, it will revert to the old ones.
//It should be safe to use while streaming or between image captures because it will pause them and return them to normal afterwards.
bool indi_webcam::ChangeOnlineSource(std::string newProtocol, std::string newIPAddress, std::string newPort, std::string newUserName,
                                   std::string newPassword)
{
    std::string newURL;

    if(!strcmp(newProtocol.c_str(), "CUSTOM"))
        newURL = customURL;
    else if(!strcmp(newProtocol.c_str(), "HTTP"))
        newURL = "http://" + newUserName + ":" + newPassword + "@" + newIPAddress + ":" + newPort;
    //else if(!strcmp(newProtocol.c_str(), "RTSP"))
    //    newURL = "rstp://" + newIPAddress + ":" + newPort + "//user=" + newUserName + "_password=" + newPassword + "_channel=1_stream=0.sdp?real_stream";

    if(ChangeOnlineSource(newURL))
    {
        protocol = newProtocol;
        IPAddress = newIPAddress;
        port = newPort;
        username = newUserName;
        password = newPassword;
        return true;
    }
    return false;
}

bool indi_webcam::ChangeOnlineSource(std::string newURL)
{

    //This will pause the streaming while it attempts the new connection settings.
    bool was_streaming = false;
    if(is_streaming)
    {
        was_streaming = true;
        StopStreaming();
    }

    //This is the case if the source is not currently connected yet.
    if(isConnected() == false)
        DEBUG(INDI::Logger::DBG_SESSION, "Not connected now, accepting settings.  It will be tested on connection");
    if(isConnected() == false || loadingSettings)
    {
        url = newURL;
        IText *URLText = &URLPathT[0];
        IUSaveText(URLText, newURL.c_str());
        IDSetText(&URLPathTP, nullptr);
        return true;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to Connect: IP Camera at: %s", newURL.c_str());

    //This is an attempt to connect, if it is already connected.  If it is not successful, it goes back to the old settings.
    if(ConnectToSource(videoDevice, videoSource, frameRate, videoSize, inputPixelFormat, newURL) == false)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Connection was NOT successful");
        DEBUGF(INDI::Logger::DBG_SESSION, "Changing back to IP Camera at: %s", url.c_str());
        ConnectToSource(videoDevice, videoSource, frameRate, videoSize, inputPixelFormat, url);
        if(was_streaming)
            StartStreaming();
        return false;
    }

    //This is what happens if the connection was successful, it saves the settings and continues.
    DEBUG(INDI::Logger::DBG_SESSION, "Due to success, saving settings.");
    url = newURL;
    IText *URLText = &URLPathT[0];
    IUSaveText(URLText, newURL.c_str());
    IDSetText(&URLPathTP, nullptr);

    //If it was streaming, we need to reinitialize that.
    if(was_streaming)
        StartStreaming();
    return true;
}


/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool indi_webcam::Disconnect()
{
    if (isConnected())
    {
        // Close the codecs
        avcodec_close(pCodecCtx);

        // Close the video file
        avformat_close_input(&pFormatCtx);

        DEBUG(INDI::Logger::DBG_SESSION, "INDI Webcam disconnected successfully!");
    }
    return true;
}
/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * indi_webcam::getDefaultName()
{
    return "INDI Webcam";
}
/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool indi_webcam::initProperties()
{
    loadingSettings = true;
    // Must init parent properties first!
    INDI::CCD::initProperties();

    setDefaultPollingPeriod(10);

    DEBUG(INDI::Logger::DBG_SESSION, "Webcam Driver initialized");

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

    IUFillNumber(&TimeoutOptionsT[0], "FFMPEG_TIMEOUT", "FFMPEG", "%.0f", 0 , 100000000, 1, ffmpegTimeout);
    IUFillNumber(&TimeoutOptionsT[1], "BUFFER_TIMEOUT", "Buffer", "%.0f", 0 , 10000000, 1, bufferTimeout);
    IUFillNumberVector(&TimeoutOptionsTP, TimeoutOptionsT, NARRAY(TimeoutOptionsT), getDeviceName(), "TIMEOUT_OPTIONS",
                     "Timeouts (us)", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    defineProperty(&TimeoutOptionsTP);

    IUFillNumber(&PixelSizeT[0], "PIXEL_SIZE_um", "Pixel Size (µm)", "%.3f", 0 , 50, 0.1, pixelSize);
    IUFillNumberVector(&PixelSizeTP, PixelSizeT, NARRAY(PixelSizeT), getDeviceName(), "PIXEL_SIZE",
                     "Pixel Size", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    defineProperty(&PixelSizeTP);

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

    IUFillText(&OnlineInputOptions[0], "CAPTURE_IP_ADDRESS", "IP Address", IPAddress.c_str());
    IUFillText(&OnlineInputOptions[1], "CAPTURE_PORT_NUMBER", "Port", port.c_str());
    IUFillText(&OnlineInputOptions[2], "CAPTURE_USERNAME", "User Name", username.c_str());
    IUFillText(&OnlineInputOptions[3], "CAPTURE_PASSWORD", "Password", password.c_str());
    IUFillTextVector(&OnlineInputOptionsP, OnlineInputOptions, 4, getDeviceName(), "ONLINE_INPUT_OPTIONS", "IP Camera",
                     CONNECTION_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&OnlineInputOptionsP);

    OnlineProtocols = new ISwitch[3];
    IUFillSwitch(&OnlineProtocols[0], "CUSTOM", "CUSTOM", ISS_OFF);
    IUFillSwitch(&OnlineProtocols[1], "HTTP", "HTTP", ISS_ON);
    //IUFillSwitch(&OnlineProtocols[2], "RTSP", "RTSP", ISS_OFF);

    IUFillSwitchVector(&OnlineProtocolSelection, OnlineProtocols, 2, getDeviceName(), "ONLINE_PROTOCOL", "Online Protocol",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineProperty(&OnlineProtocolSelection);

    IUFillText(&URLPathT[0], "URL_PATH", "URL", url.c_str());
    IUFillTextVector(&URLPathTP, URLPathT, NARRAY(URLPathT), getDeviceName(), "ONLINE_PATH",
                     "Online Path", CONNECTION_TAB, IP_RW, 0, IPS_IDLE);
    defineProperty(&URLPathTP);

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

    //Setting the log level
    av_log_set_level(AV_LOG_INFO);

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    uint32_t cap = 0;
    cap |= CCD_HAS_STREAMING;
    cap |= CCD_CAN_SUBFRAME;
    SetCCDCapability(cap);

    loadConfig(true, RapidStackingSelection.name);
    loadConfig(true, OutputFormatSelection.name);
    loadConfig(true, PixelSizeTP.name);
    loadConfig(true, InputOptionsTP.name);
    loadConfig(true, TimeoutOptionsTP.name);
    loadConfig(true, OnlineInputOptionsP.name);
    loadConfig(true, URLPathTP.name);
    loadConfig(true, OnlineProtocolSelection.name);

    refreshInputDevices();
    loadConfig(true, CaptureDeviceSelection.name);
    refreshInputSources();

    loadingSettings = false;
    return true;
}

//This refreshes the input device list
bool indi_webcam::refreshInputDevices()
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    AVInputFormat * d = nullptr;
#else
    const AVInputFormat * d = nullptr;
#endif
    int i = 0;
    int numDevices = getNumOfInputDevices();
    CaptureDevices = new ISwitch[numDevices + 1];
    while ((d = av_input_video_device_next(d)))
    {
        if(!strcmp(d->name, videoDevice.c_str()))
            IUFillSwitch(&CaptureDevices[i], d->name, d->name, ISS_ON);
        else
            IUFillSwitch(&CaptureDevices[i], d->name, d->name, ISS_OFF);
        i++;
    }
    IUFillSwitch(&CaptureDevices[numDevices], "IP Camera", "IP Camera", ISS_OFF);
    IUFillSwitchVector(&CaptureDeviceSelection, CaptureDevices, numDevices + 1, getDeviceName(), "CAPTURE_DEVICE",
                       "Capture Devices",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    defineProperty(&CaptureDeviceSelection);

    return true;
}

//This finds the number of devices available from FFMpeg
//I had to write it because there doesn't seem to be a method available in the FFMpeg Libraries
int indi_webcam::getNumOfInputDevices()
{
    int i = 0;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    AVInputFormat * d = nullptr;
#else
    const AVInputFormat * d = nullptr;
#endif
    while ((d = av_input_video_device_next(d)))
    {
        i++;
    }
    return i;
}

//This finds all the sources available from the selected device.
//FFMpeg provides a way to get the sources using the list input sources method
//But on OS X, AV Foundation is not supported in this list, so I wrote a method to get them.
//There are also other cases of devices that are not supported, for them, I just load numbers for devices.
bool indi_webcam::refreshInputSources()
{
    if (CaptureSources)
    {
        delete[] CaptureSources;
        CaptureSources = nullptr;
        deleteProperty(CaptureSourceSelection.name);
    }

    int sourceNum = 0;
    if(videoDevice == "avfoundation")
    {
        findAVFoundationVideoSources();
        sourceNum = listOfSources.size();
        CaptureSources = new ISwitch[sourceNum];
        for(int x = 0; x < sourceNum; x++)
        {
            char num[16];
            snprintf(num, 16, "%u", x);
            if(x == 0)
                IUFillSwitch(&CaptureSources[x], num, listOfSources.at(x).c_str(), ISS_ON);
            else
                IUFillSwitch(&CaptureSources[x], num, listOfSources.at(x).c_str(), ISS_OFF);
        }
    }
    else if(videoDevice == "IP Camera")
    {
        //No Source Buttons for IP Camera
    }
    else
    {
        int nbdev = 0;
        struct AVDeviceInfoList *devlist = nullptr;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 0, 100)
        AVInputFormat *iformat = av_find_input_format(videoDevice.c_str());
#else
        const AVInputFormat *iformat = av_find_input_format(videoDevice.c_str());
#endif
        nbdev = avdevice_list_input_sources(iformat, nullptr, nullptr, &devlist);

        //For this case the source list function is not implemented, we have to just list them by number
        if ((nbdev < 0 ) || (devlist->nb_devices == 0))
        {
            avdevice_free_list_devices(&devlist);
            sourceNum = 5;
            CaptureSources = new ISwitch[sourceNum];
            IUFillSwitch(&CaptureSources[0], "0", "0", ISS_ON);
            for(int x = 1; x < sourceNum; x++)
            {
                char num[16];
                snprintf(num, 16, "%u", x);
                IUFillSwitch(&CaptureSources[x], num, num, ISS_OFF);
            }
        }
        else
        {
            //For this case, we can use the names of the autodetected devices from FFMPEG
            sourceNum = devlist->nb_devices;
            CaptureSources = new ISwitch[sourceNum];
            for(int x = 0; x < sourceNum; x++)
            {
                if(!strcmp(devlist->devices[x]->device_name, videoSource.c_str()))
                    IUFillSwitch(&CaptureSources[x], devlist->devices[x]->device_name, devlist->devices[x]->device_name, ISS_ON);
                else
                    IUFillSwitch(&CaptureSources[x], devlist->devices[x]->device_name, devlist->devices[x]->device_name, ISS_OFF);
            }
            avdevice_free_list_devices(&devlist);
        }
    }

    IUFillSwitchVector(&CaptureSourceSelection, CaptureSources, sourceNum, getDeviceName(), "CAPTURE_SOURCE", "Capture Sources",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    //This controls whether the Input Options controls or the IP Camera Options controls are visible

    if (videoDevice == "IP Camera")
    {
        if(protocol != "CUSTOM")
            defineProperty(&OnlineInputOptionsP);
        defineProperty(&OnlineProtocolSelection);
        defineProperty(&URLPathTP);

        deleteProperty(CaptureSourceSelection.name);
        deleteProperty(VideoSizeSelection.name);
        deleteProperty(FrameRateSelection.name);
        deleteProperty(PixelFormatSelection.name);
        deleteProperty(InputOptionsTP.name);
    }
    else
    {
        defineProperty(&InputOptionsTP);
        defineProperty(&CaptureSourceSelection);
        defineProperty(&VideoSizeSelection);
        defineProperty(&FrameRateSelection);
        defineProperty(&PixelFormatSelection);

        if(protocol != "CUSTOM")
            deleteProperty(OnlineInputOptionsP.name);
        deleteProperty(OnlineProtocolSelection.name);
        deleteProperty(URLPathTP.name);
    }

    return true;
}


void indi_webcam::ISGetProperties(const char *dev)
{
    loadingSettings = true;
    INDI::CCD::ISGetProperties(dev);
    loadingSettings = false;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool indi_webcam::updateProperties()
{
    loadingSettings = true;

    // Call parent update properties first
    INDI::CCD::updateProperties();

    loadingSettings = false;
    return true;
}

bool indi_webcam::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
        return true;

    DEBUGF(INDI::Logger::DBG_SESSION, "Setting number %s", name);

    if (!strcmp(name, VideoAdjustmentsTP.name) )
    {
        IUUpdateNumber(&VideoAdjustmentsTP, values, names, n);

        brightness = IUFindNumber( &VideoAdjustmentsTP, "BRIGHTNESS" )->value;
        contrast = IUFindNumber( &VideoAdjustmentsTP, "CONTRAST" )->value;
        saturation = IUFindNumber( &VideoAdjustmentsTP, "SATURATION" )->value;

        DEBUGF(INDI::Logger::DBG_SESSION, "New Video Adjustments: brightness: %.3f, contrast: %.3f, saturation: %.3f", brightness,
               contrast, saturation);

        IDSetNumber(&VideoAdjustmentsTP, nullptr);
        VideoAdjustmentsTP.s = IPS_OK;

        updateVideoAdjustments();
        return true;
    }

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
        ffmpegTimeout = IUFindNumber( &TimeoutOptionsTP, "FFMPEG_TIMEOUT" )->value;
        bufferTimeout = IUFindNumber( &TimeoutOptionsTP, "BUFFER_TIMEOUT" )->value;
        DEBUGF(INDI::Logger::DBG_SESSION, "New Timeouts: ffmpeg: %.0f, buffer: %.0f", ffmpegTimeout, bufferTimeout);
        IDSetNumber (&TimeoutOptionsTP, nullptr);
        TimeoutOptionsTP.s = IPS_OK;
        return true;
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool indi_webcam::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
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

    if (!strcmp(svp->name, OnlineProtocolSelection.name))
    {
        IUUpdateSwitch(&OnlineProtocolSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&OnlineProtocolSelection);
        if (sp)
        {
            if (!strcmp(sp->name, "CUSTOM"))
            {
                deleteProperty(OnlineInputOptionsP.name);
                protocol = "CUSTOM";
                if(customURL.length() == 0)
                {
                    OnlineProtocolSelection.s = IPS_OK;
                    IDSetSwitch(&OnlineProtocolSelection, nullptr);
                    return false;
                }
                if(ChangeOnlineSource(customURL))
                {
                    OnlineProtocolSelection.s = IPS_OK;
                    IDSetSwitch(&OnlineProtocolSelection, nullptr);
                    return true;
                }
            }
            else
            {
                defineProperty(&OnlineInputOptionsP);
                if(ChangeOnlineSource(sp->name, IPAddress, port, username, password))
                {
                    OnlineProtocolSelection.s = IPS_OK;
                    IDSetSwitch(&OnlineProtocolSelection, nullptr);
                    return true;
                }
            }
        }
        return false;
    }

    if (!strcmp(name, RefreshSP.name))
    {
        if(videoDevice != "IP Camera")
        {
            bool a = refreshInputDevices();
            bool b = refreshInputSources();
            RefreshSP.s = (a && b) ? IPS_OK : IPS_ALERT;
        }
        IDSetSwitch(&RefreshSP, nullptr);
        RefreshS[0].s = ISS_OFF;
        return true;
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool indi_webcam::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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

    if (!strcmp(name, OnlineInputOptionsP.name) )
    {
        OnlineInputOptionsP.s = IPS_OK;

        IText *IPAddressText = IUFindText( &OnlineInputOptionsP, names[0] );
        IText *portText = IUFindText( &OnlineInputOptionsP, names[1] );
        IText *usernameText = IUFindText( &OnlineInputOptionsP, names[2] );
        IText *passwordText = IUFindText( &OnlineInputOptionsP, names[3] );

        if (!IPAddressText || !portText || !usernameText || !passwordText)
            return false;

        if(ChangeOnlineSource(protocol, texts[0], texts[1], texts[2], texts[3]))
        {
            IUSaveText(IPAddressText, texts[0]);
            IUSaveText(portText, texts[1]);
            IUSaveText(usernameText, texts[2]);
            IUSaveText(passwordText, texts[3]);
            IDSetText (&OnlineInputOptionsP, nullptr);
            return true;
        }
    }

    if (!strcmp(name, URLPathTP.name) )
    {
        URLPathTP.s = IPS_OK;

        IText *URLText = IUFindText( &URLPathTP, names[0] );

        customURL = texts[0];
        url = texts[0];

        if (!URLText || customURL.length() == 0)
            return false;

        if(ChangeOnlineSource(customURL))
        {
            IUSaveText(URLText, customURL.c_str());
            IDSetText (&URLPathTP, nullptr);
            IUFindSwitch(&OnlineProtocolSelection, "CUSTOM")->s = ISS_ON;
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

//This sets up a single exposure, or if rapid stacking is requested,
//It can also set up a series of exposures till the time runs out.
bool indi_webcam::StartExposure(float duration)
{
    //If the webcam is currently streaming, it cannot capture single exposures
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

    //This sets up the output format for the exposure
    if(outputFormat == "16 bit RGB")
    {
        out_pix_fmt = AV_PIX_FMT_RGB48LE;
        PrimaryCCD.setBPP(16);
        PrimaryCCD.setNAxis(3);
    }
    else if(outputFormat == "8 bit RGB")
    {
        out_pix_fmt = AV_PIX_FMT_RGB24;
        PrimaryCCD.setBPP(8);
        PrimaryCCD.setNAxis(3);
    }
    else if(outputFormat == "16 bit Grayscale")
    {
        out_pix_fmt = AV_PIX_FMT_GRAY16LE;
        PrimaryCCD.setBPP(16);
        PrimaryCCD.setNAxis(2);
    }
    else
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Invalid output format.");
        return false;
    }

    //Set up the stream, if there is an error, return
    if(!setupStreaming())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Error Setting up streaming from camera\n");
        return false;
    }

    //This will ensure that we get the current frame, not some old frame still in the buffer
    if(!flush_frame_buffer())
        DEBUG(INDI::Logger::DBG_SESSION, "FFMPEG Issue in flushing buffer");

    /*
    int ret = avformat_flush(pFormatCtx);
    if(ret != 0 )
    {
        char errbuff[200];
        av_make_error_string(errbuff, 200, ret);
        DEBUGF(INDI::Logger::DBG_SESSION, "FFMPEG Issue in flushing buffer: %d, %s.", ret, errbuff);
    }
    */

    //This sets up the exposure time settings
    ExposureRequest = duration;
    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&ExpStart, nullptr);
    timerID = SetTimer(getCurrentPollingPeriod());
    gotAnImageAlready = false;
    InExposure = true;
    return true;
}

bool indi_webcam::AbortExposure()
{
    if(stackBuffer)
        free(stackBuffer);
    InExposure = false;
    return true;
}

//This calculates the time left in the exposure.
float indi_webcam::CalcTimeLeft()
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
//If webcam stacking is happening, it repeatedly calls for images
//It also updates the reported exposure time left
//Finally when time is up, it copies any stack to the primary buffer
//And calls finish exposure to do any subframing necessary.

void indi_webcam::TimerHit()
{
    float timeleft;

    if (InExposure)
    {
        if (!isConnected())
            return; //  No need to reset timer if we are not connected anymore

        timeleft = CalcTimeLeft();
        PrimaryCCD.setExposureLeft(timeleft);
        if(webcamStacking || !gotAnImageAlready)
            grabImage(); //Note that this both starts and ends the exposure

        // The time left in the "exposure" is less than the time it takes to make an actual exposure
        // or the time left is less than the polling period, so get it now.
        if (timeleft < (1 / frameRate) || timeleft < getCurrentPollingPeriod()/1000.0)
        {
            if(webcamStacking)
                copyFinalStackToPrimaryFrameBuffer();
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            LOG_INFO("Download complete.");
            finishExposure();
            freeMemory();
            return;
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

// Downloads the image from the Webcam.
//If the image is an RGB, it converts it to Fits RGB
//If rapid stacking is happening, it adds the image to the stack.

bool indi_webcam::grabImage()
{
    if(getStreamFrame())
    {
        if(PrimaryCCD.getNAxis() == 3)
            convertINDI_RGBtoFITS_RGB(pFrameOUT->data[0], PrimaryCCD.getFrameBuffer());
        else
            memcpy(PrimaryCCD.getFrameBuffer(), pFrameOUT->data[0], numBytes);
        if(webcamStacking)
            addToStack();
        gotAnImageAlready = true;
    }
    else
    {
        freeMemory();
        return false;
    }

    return true;
}

//This adds each image to the running stack
bool indi_webcam::addToStack()
{
    if(!stackBuffer)
    {
        stackBuffer = (float *)malloc(numBytes * sizeof(float_t));
        numberOfFramesInStack = 0;
    }

    int w = pCodecCtx->width  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    int h = pCodecCtx->height;

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
float indi_webcam::getImageDataFloatValue(int x, int y)
{
    int w = pCodecCtx->width  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
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
void indi_webcam::setImageDataValueFromFloat(int x, int y, float value, bool roundAnswer)
{
    uint8_t *primaryBuffer = PrimaryCCD.getFrameBuffer();
    int w = pCodecCtx->width * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
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
void indi_webcam::copyFinalStackToPrimaryFrameBuffer()
{
    int w = pCodecCtx->width  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    int h = pCodecCtx->height;
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
void indi_webcam::finishExposure()
{
    uint8_t *memptr = PrimaryCCD.getFrameBuffer();
    int w = pCodecCtx->width;
    int h = pCodecCtx->height;
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

        ExposureComplete(&PrimaryCCD);

        // Restore old pointer and release memory
        PrimaryCCD.setFrameBuffer(memptr);
        PrimaryCCD.setFrameBufferSize(numBytes, false);
    }
    else
        ExposureComplete(&PrimaryCCD);
}

bool indi_webcam::UpdateCCDFrame(int x, int y, int w, int h)
{

    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

void indi_webcam::debugTriggered(bool enabled)
{
    if(enabled)
    {
        av_log_set_level(AV_LOG_DEBUG);
        DEBUG(INDI::Logger::DBG_SESSION, "Setting FFMPEG Logging to Verbose\n");
    }
    else
    {
        av_log_set_level(AV_LOG_INFO);
        DEBUG(INDI::Logger::DBG_SESSION, "Setting FFMPEG Logging to Info\n");
    }
}

//These next several methods handle streaming starting and stopping.

void indi_webcam::start_capturing()
{
    if (is_capturing) return;
    is_capturing = true;
    capture_thread = std::thread(RunCaptureThread, this);
}

void indi_webcam::stop_capturing()
{
    if (!is_capturing) return;
    is_capturing = false;
    if (std::this_thread::get_id() != capture_thread.get_id())
        capture_thread.join();
}

bool indi_webcam::StartStreaming()
{
    if (is_streaming) return true;
    if (!is_capturing) start_capturing();
    is_streaming = true;
    return true;
}

bool indi_webcam::StopStreaming()
{
    if (!is_streaming) return true;
    stop_capturing();
    is_streaming = false;
    return true;
}

void indi_webcam::RunCaptureThread(indi_webcam *webcam)
{
    webcam->run_capture();
}

//This is the loop that runs during streaming
//Note that it ONLY supports RGB24 aka INDI_RGB format.
void indi_webcam::run_capture()
{

    //This sets up the output format for the exposures
    if(outputFormat == "16 bit RGB")
    {
        LOG_INFO("Note, RGB 16 bit not supported in video stream using 8 Bit RGB instead.");
        out_pix_fmt = AV_PIX_FMT_RGB24;
        PrimaryCCD.setBPP(8);
        PrimaryCCD.setNAxis(3);
        Streamer->setPixelFormat(INDI_RGB);
    }
    else if(outputFormat == "8 bit RGB")
    {
        out_pix_fmt = AV_PIX_FMT_RGB24;
        PrimaryCCD.setBPP(8);
        PrimaryCCD.setNAxis(3);
        Streamer->setPixelFormat(INDI_RGB);
    }
    else if(outputFormat == "16 bit Grayscale")
    {
        LOG_INFO("Note, 16 bit Grayscale not supported in video stream using 8 Bit Grayscale instead.");
        out_pix_fmt = AV_PIX_FMT_GRAY8;
        PrimaryCCD.setBPP(8);
        PrimaryCCD.setNAxis(2);
        Streamer->setPixelFormat(INDI_MONO);
    }
    else
        return;

    if(!setupStreaming())
        return;

    int w = pCodecCtx->width;
    int h = pCodecCtx->height;
    Streamer->setSize(w, h);
    PrimaryCCD.setFrame(0, 0, w, h);

    //This will clear the frame button before streaming is started so that the frames are all current.
    if(!flush_frame_buffer())
        DEBUG(INDI::Logger::DBG_SESSION, "FFMPEG Issue in flushing buffer");

    /*
    int ret = avformat_flush(pFormatCtx);
    if(ret != 0 )
    {
        char errbuff[200];
        av_make_error_string(errbuff, 200, ret);
        DEBUGF(INDI::Logger::DBG_SESSION, "FFMPEG Issue in flushing buffer: %d, %s.", ret, errbuff);
    }
    */

    while (is_capturing && is_streaming)
    {

        if(getStreamFrame())
            Streamer->newFrame(pFrameOUT->data[0], numBytes);
        else
        {
            is_capturing = false;
            is_streaming = false;
        }
    }

    freeMemory();

    DEBUG(INDI::Logger::DBG_SESSION, "Capture thread releasing device.");
}

//This converts an image from INDI_RGB to FITS_RGB so the FITSViewer can read it.
bool indi_webcam::convertINDI_RGBtoFITS_RGB(uint8_t *originalImage, uint8_t *convertedImage)
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

//This sets up the webcam to get images
//It is used for both the streaming and exposing algorithms
bool indi_webcam::setupStreaming()
{
    // Determine required buffer size and allocate buffer for pframeRGB
    numBytes = av_image_get_buffer_size(out_pix_fmt, pCodecCtx->width, pCodecCtx->height, 1);

    // Allocate video frame
    pFrame = av_frame_alloc();
    if(pFrame == nullptr)
        return false;
    // Allocate an AVFrame structure
    pFrameOUT = av_frame_alloc();
    if(pFrameOUT == nullptr)
        return false;

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    if(buffer == nullptr)
        return false;

    av_image_fill_arrays (pFrameOUT->data, pFrameOUT->linesize, buffer, out_pix_fmt,
                          pCodecCtx->width, pCodecCtx->height, 1);

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext( pCodecCtx->width, pCodecCtx->height,
                              pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                              out_pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr
                            );
    if(sws_ctx == nullptr)
        return false;

    updateVideoAdjustments();

    PrimaryCCD.setFrameBufferSize(numBytes);
    PrimaryCCD.setResolution(pCodecCtx->width, pCodecCtx->height);

    return true;
}

void indi_webcam::updateVideoAdjustments()
{
    if(sws_ctx == nullptr)
        return;

    int src_range = 1, dst_range = 1; //These are just flags 1 for Jpeg and 2 for Mpeg
    const int* coefs = sws_getCoefficients(SWS_CS_DEFAULT);
    //Note these last 3 values are reported in 16.16 fixed point format
    sws_setColorspaceDetails(sws_ctx, coefs, src_range, coefs, dst_range,
                             (int)(brightness * 65536), (int)(contrast * 65536), (int)(saturation * 65536));
}

//This gets one image from the camera.
//It is used for both the streaming and exposing algorithms
bool indi_webcam::getStreamFrame()
{
    AVPacket packet;
    //If at first you don't succees to get a frame, try again.
    int ret = -1;
    while(ret < 0)
    {
        int tries = 0;
        while(tries < 10) //Try a maximum of 10 times before trying to reconnect the source
        {
            ret = av_read_frame(pFormatCtx, &packet);
            if(ret == 0)
                break;
            else
            {
                if(ret != -35) // Don't display "Resource Temporarily Unavailable"
                {
                    char errbuff[200];
                    av_make_error_string(errbuff, 200, ret);
                    DEBUGF(INDI::Logger::DBG_SESSION, "FFMPEG Error: %d, %s.", ret, errbuff);
                }
                tries++;
                usleep(bufferTimeout); //give it a moment, if it is unavailable
            }
        }
        if(ret < 0) // If it still is not working after 10 tries, we should try reconnecting the source.
        {
            if(reconnectSource())
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Device successfully reconnected.");
                freeMemory();
                //Try to set up streaming again, if there is an error, return
                if(!setupStreaming())
                {
                    DEBUG(INDI::Logger::DBG_SESSION, "Error on Stream Setup.");
                    return false;
                }
            }
            else
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Device did not reconnect after 10 tries.");
                av_packet_unref(&packet);
                return false;
            }
        }
    }
    if(packet.stream_index == videoStream)
    {
        int ret;
        ret = avcodec_send_packet(pCodecCtx, &packet);
        char errbuff[200];
        av_make_error_string(errbuff, 200, ret);
        if (ret < 0)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Error sending a packet for decoding:%s", errbuff);
            av_packet_unref(&packet);
            return false;
        }
        while (ret >= 0)
        {
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                continue;
            else if (ret < 0)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Error during decoding");
                av_packet_unref(&packet);
                return false;
            }
            // We have a frame at that point
            // Convert the image from its native format to our output format
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                      pFrame->linesize, 0, pCodecCtx->height,
                      pFrameOUT->data, pFrameOUT->linesize);
            av_packet_unref(&packet);
            return true;
        }

    }
    return false;
}

//This will clear out the frame buffer of any unread frames.
//That way we are sure to get the latest frames when exposing
 bool indi_webcam::flush_frame_buffer()
 {
     int packetReceiveTime = -1;
     int num = 0;
     while(packetReceiveTime < bufferTimeout)
     {
         num++;
         struct timeval then;
         gettimeofday(&then, nullptr);
         AVPacket packet;
         int ret = av_read_frame(pFormatCtx, &packet);
         if(ret != 0) // Return value less than 0 means error
         {
             if(ret == -35) //Leave the loop since the device is not available to give frames.
                 break;
             char errbuff[200];
             av_make_error_string(errbuff, 200, ret);
             DEBUGF(INDI::Logger::DBG_SESSION, "FFMPEG Error while clearing buffer: %s.", errbuff);
         }
         struct timeval now;
         gettimeofday(&now, nullptr);
         packetReceiveTime = now.tv_usec - then.tv_usec;
         av_packet_unref(&packet);
     }
     DEBUGF(INDI::Logger::DBG_SESSION, "Buffer Cleared of %u stale frames.", num);
     return true;  //Buffer Cleared
 }

//This frees up the resources used for streaming/exposing
void indi_webcam::freeMemory()
{
    // Free the sws_context
    if(sws_ctx)
        sws_freeContext(sws_ctx);
    sws_ctx = nullptr;

    // Free the Buffer
    if(buffer)
        av_free(buffer);
    buffer = nullptr;

    // Free the RGB image
    if(pFrameOUT)
        av_free(pFrameOUT);
    pFrameOUT = nullptr;

    // Free the input frame
    if(pFrame)
        av_free(pFrame);
    pFrame = nullptr;

}

bool indi_webcam::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &CaptureDeviceSelection);
    IUSaveConfigSwitch(fp, &RapidStackingSelection);
    IUSaveConfigSwitch(fp, &OutputFormatSelection);
    IUSaveConfigSwitch(fp, &OnlineProtocolSelection);
    IUSaveConfigNumber(fp, &PixelSizeTP);
    IUSaveConfigText(fp, &InputOptionsTP);
    IUSaveConfigText(fp, &OnlineInputOptionsP);
    IUSaveConfigText(fp, &URLPathTP);
    IUSaveConfigNumber(fp, &TimeoutOptionsTP);

    return true;
}
