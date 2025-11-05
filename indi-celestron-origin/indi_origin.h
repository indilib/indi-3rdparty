#pragma once

#include <inditelescope.h>
#include <indiccd.h>
#include <memory>
#include "TelescopeData.hpp"
#include "OriginBackendSimple.hpp"

class OriginTelescope : public INDI::Telescope
{
public:
    OriginTelescope();
    virtual ~OriginTelescope();

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    
protected:
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool ReadScopeStatus() override;
    virtual bool Goto(double ra, double dec) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool Abort() override;
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual void TimerHit() override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

private:
    OriginDiscovery m_discovery;
    bool m_telescopeDiscovered {false};
    
    ITextVectorProperty AddressTP;
    IText AddressT[2] {};
    
    double m_currentRA {0};
    double m_currentDec {0};
    bool m_connected {false};
    void onTelescopeDiscovered(const OriginDiscovery::TelescopeInfo& info);
};

class OriginCamera : public INDI::CCD
{
public:
    OriginCamera();
    virtual ~OriginCamera();

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    // Add this method so telescope can give us the backend later
    void setBackend(OriginBackendSimple *backend);

protected:
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    virtual bool UpdateCCDFrame(int, int, int, int) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;
    virtual void TimerHit() override;
    
    // Only need this for the preview/full mode switch
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, 
                            char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword) override;

private:
    double m_exposureStart {0};
    double m_exposureDuration {0};
    
    // Image callback support
    bool m_imageReady {false};
    QString m_pendingImagePath;
    QByteArray m_pendingImageData;
    double m_pendingImageRA {0};
    double m_pendingImageDec {0};
    
    // State flags
    bool m_waitingForImage {false};
    bool m_useNextImage {false};

    // Gain/ISO property - like the simulator
    INDI::PropertyNumber GainNP {1};
    enum { GAIN };    

    // Preview/Full mode property
    INDI::PropertySwitch StreamSP {2};
    enum { STREAM_PREVIEW, STREAM_FULL };
    bool m_isPreviewMode {false};
    
    // Methods
    void onImageReady(const QString& filePath, const QByteArray& imageData,
                     double ra, double dec);
    bool processAndUploadImage(const QByteArray& imageData);
    double currentTime();
};
