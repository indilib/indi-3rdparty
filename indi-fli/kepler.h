/*
    INDI Driver for Kepler sCMOS camera.
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

#pragma once

#include <libflipro.h>
#include <indiccd.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>
#include <indipropertylight.h>
#include <indipropertyblob.h>

#include <inditimer.h>
#include <indisinglethreadpool.h>

class Kepler : public INDI::CCD
{
    public:
        Kepler(const FPRODEVICEINFO &info, std::wstring name);
        virtual ~Kepler() override;

        const char *getDefaultName() override;

        bool initProperties() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;

        bool Connect() override;
        bool Disconnect() override;

        bool StartExposure(float duration) override;
        bool AbortExposure() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        virtual int SetTemperature(double temperature) override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;
        virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

        virtual void debugTriggered(bool enable) override;
        virtual bool saveConfigItems(FILE *fp) override;

        virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;
        virtual void UploadComplete(INDI::CCDChip *targetChip) override;

    private:

        //****************************************************************************************
        // INDI Properties
        //****************************************************************************************
        INDI::PropertySwitch CommunicationMethodSP {2};


        // Gain
        INDI::PropertySwitch LowGainSP {0};
        INDI::PropertySwitch HighGainSP {0};

        // Cooler & Fan
        INDI::PropertyNumber CoolerDutyNP {1};
        INDI::PropertySwitch FanSP {2};

        // Merging
        INDI::PropertySwitch MergePlanesSP {3};
        INDI::PropertySwitch RequestStatSP {2};
        INDI::PropertyText MergeCalibrationFilesTP {2};
        enum
        {
            CALIBRATION_DARK,
            CALIBRATION_FLAT
        };

        // Black Level Adjust
        INDI::PropertyNumber BlackLevelNP {1};

        // GPS State
        INDI::PropertyLight GPSStateLP {4};

#ifdef LEGACY_MODE
        //****************************************************************************************
        // Legacy INDI Properties
        //****************************************************************************************
        INDI::PropertyNumber ExpValuesNP {11};
        enum
        {
            ExpTime,
            ROIW,
            ROIH,
            OSW,
            OSH,
            BinW,
            BinH,
            ROIX,
            ROIY,
            Shutter,
            Type
        };
        INDI::PropertySwitch ExposureTriggerSP {1};
        INDI::PropertyNumber TemperatureSetNP {1};
        INDI::PropertyNumber TemperatureReadNP {2};
        double m_ExposureRequest {1};
#endif


        //****************************************************************************************
        // Communication Functions
        //****************************************************************************************
        bool setup();
        void prepareUnpacked();
        void readTemperature();
        void readGPS();

        //****************************************************************************************
        // Workers
        //****************************************************************************************
        void workerExposure(const std::atomic_bool &isAboutToQuit, float duration);

        //****************************************************************************************
        // Variables
        //****************************************************************************************
        FPRODEVICEINFO m_CameraInfo;
        int32_t m_CameraHandle;
        uint32_t m_CameraCapabilitiesList[static_cast<uint32_t>(FPROCAPS::FPROCAP_NUM)];;

        uint8_t m_ExposureRetry {0};
        INDI::SingleThreadPool m_Worker;
        uint32_t m_TotalFrameBufferSize {0};

        // Merging
        uint8_t *m_FrameBuffer {nullptr};
        FPROUNPACKEDIMAGES fproUnpacked;
        FPROUNPACKEDSTATS  fproStats;
        FPRO_HWMERGEENABLE mergeEnables;

        // Format
        uint32_t m_FormatsCount;
        FPRO_PIXEL_FORMAT *m_FormatList {nullptr};

        // GPS
        FPROGPSSTATE m_LastGPSState {FPROGPSSTATE::FPRO_GPS_NOT_DETECTED};

        // Temperature
        INDI::Timer m_TemperatureTimer;
        INDI::Timer m_GPSTimer;

        // Gain Tables
        FPROGAINVALUE *m_LowGainTable {nullptr};
        FPROGAINVALUE *m_HighGainTable {nullptr};

        static std::map<FPRODEVICETYPE, double> SensorPixelSize;
        static constexpr double TEMPERATURE_THRESHOLD {0.1};
        static constexpr double TEMPERATURE_FREQUENCY_BUSY {1000};
        static constexpr double TEMPERATURE_FREQUENCY_IDLE {5000};
        static constexpr uint32_t GPS_TIMER_PERIOD {5000};

        static constexpr const char *GPS_TAB {"GPS"};
        static constexpr const char *LEGACY_TAB {"Legacy"};
};
