#include "indi_origin.h"
#include "OriginBackendSimple.hpp"
#include <indicom.h>
#include <memory>
#include <cstring>
#include <cmath>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <tiffio.h>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

// INDI requires these for driver registration
std::unique_ptr<OriginTelescope> telescope(new OriginTelescope());
std::unique_ptr<OriginCamera> camera(new OriginCamera());
std::unique_ptr<OriginBackendSimple> backend(new OriginBackendSimple());

//=============================================================================
// TELESCOPE IMPLEMENTATION
//=============================================================================

OriginTelescope::OriginTelescope()
{
    setVersion(1, 0);
    
    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO | 
        TELESCOPE_CAN_SYNC | 
        TELESCOPE_CAN_ABORT |
        TELESCOPE_CAN_CONTROL_TRACK |
        TELESCOPE_CAN_PARK |
        TELESCOPE_CAN_HOME_SET |
        TELESCOPE_CAN_HOME_GO |
        TELESCOPE_HAS_TIME |
        TELESCOPE_HAS_LOCATION |
        TELESCOPE_HAS_TRACK_MODE,
        4
    );
}

OriginTelescope::~OriginTelescope()
{
}

const char *OriginTelescope::getDefaultName()
{
    return "Origin Telescope";
}

bool OriginTelescope::initProperties()
{
    INDI::Telescope::initProperties();
    
    qDebug() << ("initProperties() called");
    
    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO | 
        TELESCOPE_CAN_SYNC | 
        TELESCOPE_CAN_ABORT |
        TELESCOPE_CAN_PARK |
        TELESCOPE_HAS_TIME |
        TELESCOPE_HAS_LOCATION,
        4
    );
    
    // Connection address
    IUFillText(&AddressT[0], "HOST", "Host", "");
    IUFillText(&AddressT[1], "PORT", "Port", "80");
    IUFillTextVector(&AddressTP, AddressT, 2, getDeviceName(),
                     "DEVICE_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    
    // Set up discovery callback
    m_discovery.setDiscoveryCallback([this](const OriginDiscovery::TelescopeInfo& info) {
        this->onTelescopeDiscovered(info);
    });
    
    // Start discovery immediately
    if (m_discovery.startDiscovery())
    {
        qDebug() << "Auto-started telescope discovery on port 55555";
    }
    else
    {
        qDebug() << "Failed to start auto-discovery";
    }
    
    addDebugControl();

    // Start the INDI base class timer - it will automatically call ReadScopeStatus()
    SetTimer(getCurrentPollingPeriod());
    qDebug() << ("Timer set");
    
    qDebug() << ("initProperties() complete");
    
    return true;
}

bool OriginTelescope::updateProperties()
{
    INDI::Telescope::updateProperties();
    
    if (isConnected())
    {
        defineProperty(&AddressTP);
    }
    else
    {
        deleteProperty(AddressTP.name);
    }
    
    return true;
}

// Callback when telescope is discovered
void OriginTelescope::onTelescopeDiscovered(const OriginDiscovery::TelescopeInfo& info)
{
    if (m_telescopeDiscovered)
        return;  // Already found one
    
    LOGF_INFO("Discovered: %s - %s", info.ipAddress.c_str(), info.model.c_str());
    
    // Update the connection address
    IUSaveText(&AddressT[0], info.ipAddress.c_str());
    IUSaveText(&AddressT[1], "80");
    
    // Notify the client that the property has changed
    AddressTP.s = IPS_OK;
    IDSetText(&AddressTP, "Found Origin telescope at %s", info.ipAddress.c_str());
    
    m_telescopeDiscovered = true;
    
    // Stop discovery after finding first telescope
    m_discovery.stopDiscovery();
    
    LOGF_INFO("Set connection address to %s:80", info.ipAddress.c_str());
}

void OriginTelescope::TimerHit()
{
  if (false) qDebug() << "telescope timer hit";  
    // Poll discovery if active
    if (m_discovery.isDiscovering()) m_discovery.poll();
    
    // Poll backend for telescope updates
    if (isConnected()) ReadScopeStatus();

    if (false) qDebug() << "telescope timer reschedule";  
    SetTimer(getCurrentPollingPeriod());
}

bool OriginTelescope::Connect()
{
    qDebug() << ("=== Connect() START ===");

    // Check if we have a telescope address
    if (AddressT[0].text == nullptr || strlen(AddressT[0].text) == 0)
    {
      qDebug() << ("No telescope address found. Discovery may still be in progress.");
      return false;
    }
    
    // Get connection settings
    QString host = QString::fromUtf8(AddressT[0].text);
    int port = atoi(AddressT[1].text);
    
    qDebug() << "Connecting to " << host.toUtf8().constData() << ":" << port;

    if (!backend->connectToTelescope(host, port))
    {
        qDebug() << ("Failed to connect to Origin Telescope");
        return false;
    }
    
    backend->setConnected(true);
    m_connected = true;
    
    // Set up status callback
    backend->setStatusCallback([this]() {
        // Status updated - will be read in ReadScopeStatus()
    });
    
    qDebug() << ("=== Connect() COMPLETE ===");

    return true;
}

bool OriginTelescope::Disconnect()
{
    qDebug() << ("Disconnecting from Origin Telescope");
    
    backend->disconnectFromTelescope();
    
    m_connected = false;
    
    return true;
}

bool OriginTelescope::ReadScopeStatus()
{
    if (!m_connected)
    {
        qDebug() << ("ReadScopeStatus called but not connected");
        return false;
    }
    
    // Poll the backend to get latest data
    backend->poll();
    
    auto status = backend->status();
    
    if (false) qDebug() << "Backend status: RA=" <<
      status.raPosition << " Dec=" << status.decPosition << "Tracking=" << status.isTracking << "Slewing=" << status.isSlewing;
    
    // Update coordinates
    m_currentRA = status.raPosition;
    m_currentDec = status.decPosition;
    
    if (false) qDebug() << "Setting INDI coords: RA=" << m_currentRA << " Dec=" << m_currentDec;
    
    // Update the internal INDI state
    NewRaDec(m_currentRA, m_currentDec);
    
    // CRITICAL: Actually send the coordinates to the INDI client
    // This is what makes the coordinates appear in Ekos
    EqNP.apply();
    
    if (false) qDebug() << "After EqNP.apply() - coordinates sent to client";
    
    // Update tracking state
    if (status.isSlewing)
    {
        TrackState = SCOPE_SLEWING;
        qDebug() << ("State: SLEWING");
    }
    else if (status.isTracking)
    {
        TrackState = SCOPE_TRACKING;
        qDebug() << ("State: TRACKING");
    }
    else if (status.isParked)
    {
        TrackState = SCOPE_PARKED;
        qDebug() << ("State: PARKED");
    }
    else
    {
        TrackState = SCOPE_IDLE;
    }
    
    return true;
}

bool OriginTelescope::Goto(double ra, double dec)
{
    if (!m_connected)
        return false;
    
    qDebug() << "Slewing to RA:" << ra << " Dec:" << dec;
    
    if (backend->gotoPosition(ra, dec))
    {
        TrackState = SCOPE_SLEWING;
        return true;
    }
    
    return false;
}

bool OriginTelescope::Sync(double ra, double dec)
{
    if (!m_connected)
        return false;
    
    qDebug() << "Syncing to RA:" << ra << " Dec:" << dec;
    
    return backend->syncPosition(ra, dec);
}

bool OriginTelescope::Abort()
{
    if (!m_connected)
        return false;
    
    qDebug() << ("Aborting slew");
    return backend->abortMotion();
}

bool OriginTelescope::Park()
{
    if (!m_connected)
        return false;
    
    qDebug() << ("Parking telescope");
    return backend->parkMount();
}

bool OriginTelescope::UnPark()
{
    if (!m_connected)
        return false;
    
    qDebug() << ("Unparking telescope");
    return backend->unparkMount();
}

bool OriginTelescope::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, AddressTP.name) == 0)
        {
            IUUpdateText(&AddressTP, texts, names, n);
            AddressTP.s = IPS_OK;
            IDSetText(&AddressTP, nullptr);
            return true;
        }
    }
    
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

//=============================================================================
// CAMERA IMPLEMENTATION - Using callback data directly
//=============================================================================

OriginCamera::OriginCamera()
{
    setVersion(1, 0);
}

OriginCamera::~OriginCamera()
{
    // Cleanup if needed
}

const char *OriginCamera::getDefaultName()
{
    return "Origin Camera";
}

bool OriginCamera::updateProperties()
{
    INDI::CCD::updateProperties();
    
    if (isConnected())
    {
        defineProperty(GainNP);
        defineProperty(StreamSP);
    }
    else
    {
        deleteProperty(GainNP);
        deleteProperty(StreamSP);
    }
    
    return true;
}

bool OriginCamera::Connect()
{
    qDebug() << ("Origin Camera connected");
    initProperties();
    ISGetProperties(nullptr);
    
    backend->setImageCallback([this](const QString& path, const QByteArray& data, 
				       double ra, double dec, double /*exposure*/) {
      this->onImageReady(path, data, ra, dec);
    });
    // START THE CAMERA'S TIMER!
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool OriginCamera::Disconnect()
{
    qDebug() << ("Origin Camera disconnected");
    return true;
}


void OriginCamera::onImageReady(const QString& filePath, const QByteArray& imageData,
                                 double ra, double dec)
{
    qDebug() << "Image ready callback received:" << filePath 
             << "Size:" << imageData.size() << "bytes";
    
    // Check if this is a preview or full capture based on filename
    bool isPreview = filePath.contains("jpg", Qt::CaseInsensitive);
    
    qDebug() << "Image type:" << (isPreview ? "Preview" : "Full capture");
    
    // If we're not in exposure or waiting for an image, ignore it
    if (!InExposure && !m_useNextImage)
    {
        qDebug() << "Ignoring unsolicited image (not in exposure)";
        return;
    }
    
    // If we're in preview mode, accept previews
    // If we're in full mode, only accept full captures
    if (m_isPreviewMode && !isPreview)
    {
        qDebug() << "Ignoring full capture (preview mode active)";
        return;
    }
    
    if (!m_isPreviewMode && isPreview)
    {
        qDebug() << "Ignoring preview (full mode active, waiting for full capture)";
        return;
    }
    
    // This is the image we want!
    m_pendingImagePath = filePath;
    m_pendingImageData = imageData;
    m_pendingImageRA = ra;
    m_pendingImageDec = dec;
    m_imageReady = true;
    
    qDebug() << "Image accepted for processing";
}

bool OriginCamera::StartExposure(float duration)
{
    qDebug() << "Starting exposure:" << duration << "seconds";
    
    // Clear previous state
    m_imageReady = false;
    m_pendingImagePath.clear();
    m_pendingImageData.clear();
    m_waitingForImage = true;
    m_useNextImage = true;
    
    // Get the ISO value from INDI's built-in Gain property
    int iso = GainNP[0].getValue();
    
    qDebug() << "Using ISO:" << iso << "Mode:" << (m_isPreviewMode ? "Preview" : "Full");
    
    bool success;
    
    if (m_isPreviewMode)
    {
        // Preview mode - wait for next preview image Origin sends automatically
        qDebug() << "Preview mode: waiting for next preview image...";
        success = true;
    }
    else
    {
        // Full resolution capture
        success = backend->takeSnapshot(duration, iso);
        qDebug() << "Full mode: triggered snapshot capture";
    }
    
    if (!success)
    {
        qDebug() << "Failed to send capture command";
        m_waitingForImage = false;
        m_useNextImage = false;
        return false;
    }
    
    m_exposureDuration = duration;
    m_exposureStart = currentTime();
    
    PrimaryCCD.setExposureDuration(duration);
    PrimaryCCD.setExposureLeft(duration);
    InExposure = true;
    
    return true;
}

bool OriginCamera::AbortExposure()
{
    qDebug() << "Aborting exposure";
    
    InExposure = false;
    m_imageReady = false;
    m_waitingForImage = false;
    m_useNextImage = false;
    m_pendingImageData.clear();
    
    return true;
}

bool OriginCamera::UpdateCCDFrame(int, int, int, int)
{
    return true;
}

bool OriginCamera::UpdateCCDBin(int binx, int biny)
{
    qDebug() << "Setting binning to" << binx << "x" << biny;
    return true;
}

void OriginCamera::TimerHit()
{  
    if (!isConnected())
        return;
    
    if (InExposure)
    {
        // In preview mode, accept the image as soon as it arrives
        // In full mode, wait for both time elapsed AND image ready
        bool canComplete = false;
        
        if (m_isPreviewMode)
        {
            // Preview mode: complete as soon as we have an image
            canComplete = m_imageReady && !m_pendingImageData.isEmpty();
        }
        else
        {
            // Full mode: wait for exposure time AND image
            double elapsed = currentTime() - m_exposureStart;
            double remaining = m_exposureDuration - elapsed;
            
            if (remaining > 0)
            {
                // Still exposing
                PrimaryCCD.setExposureLeft(remaining);
            }
            else
            {
                // Exposure time complete, check for image
                PrimaryCCD.setExposureLeft(0);
                canComplete = m_imageReady && !m_pendingImageData.isEmpty();
                
                if (!canComplete && false)
                {
                    qDebug() << "Exposure time complete, waiting for image...";
                }
            }
        }
        
        if (canComplete)
        {
            qDebug() << "Exposure complete and image data ready, processing...";
            
            if (processAndUploadImage(m_pendingImageData))
            {
                qDebug() << "Image processed and sent to client";
                InExposure = false;
                m_imageReady = false;
                m_waitingForImage = false;
                m_useNextImage = false;
                m_pendingImageData.clear();
            }
            else
            {
                qDebug() << "Failed to process image";
                PrimaryCCD.setExposureFailed();
                InExposure = false;
                m_imageReady = false;
                m_waitingForImage = false;
                m_useNextImage = false;
                m_pendingImageData.clear();
            }
        }
    }
    
    SetTimer(getCurrentPollingPeriod());
}

// Helper function to get current time in seconds
double OriginCamera::currentTime()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

bool OriginCamera::initProperties()
{
    INDI::CCD::initProperties();
    
    SetCCDCapability(CCD_CAN_ABORT);
    
    // Origin camera dimensions
    SetCCDParams(3056, 2048, 16, 3.76, 3.76);
    
    // Set exposure range with 1 microsecond resolution
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.000001, 3600, 0.000001, false);
    
    // Gain/ISO property - Origin supports 0-1600
    // Match the simulator pattern exactly
    GainNP[GAIN].fill("GAIN", "value", "%.f", 0, 1600, 1, 100);
    //                                          ^  ^     ^  ^
    //                                        min max  step default
    
    // Preview/Full mode property
    StreamSP[STREAM_PREVIEW].fill("PREVIEW", "Preview (fast)", ISS_OFF);
    StreamSP[STREAM_FULL].fill("FULL", "Full Resolution", ISS_ON);
    StreamSP.fill(getDeviceName(), "STREAM_MODE", "Capture Mode", IMAGE_SETTINGS_TAB, 
                  IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    m_isPreviewMode = false;
    
    addDebugControl();
    
    return true;
}

bool OriginCamera::processAndUploadImage(const QByteArray& imageData)
{
    qDebug() << "Processing 16-bit RGB TIFF with libtiff:" << imageData.size() << "bytes";
    
    // Use QStandardPaths to get a persistent temp directory
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    
    // Create the directory if it doesn't exist
    QDir dir(tempDir);
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qDebug() << "Failed to create cache directory:" << tempDir;
            return false;
        }
    }
    
    // Create temp file in persistent location
    QString tempPath = QString("%1/origin_temp_%2.tiff")
                           .arg(tempDir)
                           .arg(QDateTime::currentMSecsSinceEpoch());
    
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly))
    {
        qDebug() << "Failed to create temp file";
        return false;
    }
    tempFile.write(imageData);
    tempFile.close();
    
    // Open TIFF with libtiff
    TIFF* tif = TIFFOpen(tempPath.toUtf8().constData(), "r");
    if (!tif)
    {
        qDebug() << "Failed to open TIFF with libtiff";
        QFile::remove(tempPath);
        return false;
    }
    
    // Read TIFF properties
    uint32_t width, height;
    uint16_t samplesperpixel, bitspersample, photometric, config;
    
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);
    
    qDebug() << "TIFF properties: width=" << width << "height=" << height
             << "samples=" << samplesperpixel << "bits=" << bitspersample
             << "photometric=" << photometric << "planar=" << config;
    
    if (samplesperpixel != 3 || bitspersample != 16)
    {
        qDebug() << "Unexpected TIFF format";
        TIFFClose(tif);
        QFile::remove(tempPath);
        return false;
    }
    
    // Set up for 3-axis FITS (RGB cube)
    PrimaryCCD.setFrame(0, 0, width, height);
    PrimaryCCD.setExposureDuration(m_exposureDuration);
    PrimaryCCD.setNAxis(3);
    
    // Allocate buffer
    int planeSize = width * height;
    PrimaryCCD.setFrameBufferSize(planeSize * 3 * sizeof(uint16_t));
    uint16_t *image = (uint16_t *)PrimaryCCD.getFrameBuffer();
    
    // Allocate scanline buffer
    uint16_t *scanline = (uint16_t *)_TIFFmalloc(TIFFScanlineSize(tif));
    
    if (!scanline)
    {
        qDebug() << "Failed to allocate scanline buffer";
        TIFFClose(tif);
        QFile::remove(tempPath);
        return false;
    }
    
    // Read TIFF data line by line and convert to planar RGB
    qDebug() << "Reading TIFF data...";
    
    for (uint32_t row = 0; row < height; row++)
    {
        if (TIFFReadScanline(tif, scanline, row) < 0)
        {
            qDebug() << "Error reading scanline" << row;
            break;
        }
        
        // Convert interleaved RGB to planar RGB
        // scanline format: R0 G0 B0 R1 G1 B1 R2 G2 B2 ...
        // output format: R0 R1 R2 ... G0 G1 G2 ... B0 B1 B2 ...
        for (uint32_t col = 0; col < width; col++)
        {
            int idx = row * width + col;
            int scanIdx = col * 3;
            
            image[idx] = scanline[scanIdx];                    // R plane
            image[planeSize + idx] = scanline[scanIdx + 1];    // G plane
            image[planeSize * 2 + idx] = scanline[scanIdx + 2]; // B plane
        }
    }
    
    // Cleanup
    _TIFFfree(scanline);
    TIFFClose(tif);
    //    QFile::remove(tempPath);
    qDebug() << "TIFF file left in: " << tempPath;
    
    // Sample center pixel to verify
    int centerIdx = (height / 2) * width + (width / 2);
    qDebug() << "Center pixel values: R=" << image[centerIdx] 
             << "G=" << image[planeSize + centerIdx]
             << "B=" << image[planeSize*2 + centerIdx];
    
    qDebug() << "3-axis RGB FITS ready, sending to Ekos";
    
    // Send to Ekos
    ExposureComplete(&PrimaryCCD);
    
    return true;
}

bool OriginCamera::ISNewSwitch(const char *dev, const char *name, ISState *states, 
                                char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Handle Stream Mode changes
        if (StreamSP.isNameMatch(name))
        {
            StreamSP.update(states, names, n);
            
            m_isPreviewMode = (StreamSP[STREAM_PREVIEW].getState() == ISS_ON);
            
            qDebug() << "Capture mode changed to:" << (m_isPreviewMode ? "PREVIEW" : "FULL");
            
            StreamSP.setState(IPS_OK);
            StreamSP.apply();
            
            return true;
        }
    }
    
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool OriginCamera::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    // Gain
    GainNP.save(fp);

    // Save stream mode
    StreamSP.save(fp);
    
    return true;
}

void OriginCamera::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeyword);

    fitsKeyword.push_back({"GAIN", GainNP[0].getValue(), 3, "ISO"});
}

