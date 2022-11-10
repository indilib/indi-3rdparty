/*
   INDI Developers Manual
   Tutorial #2

   "Simple Telescope Driver"

   We develop a simple telescope simulator.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file indi_ahp_gt.h
    \brief Driver for the GT1 Telescope motor controller.
    \author Ilia Platone

    AHP GT1 Telescope stepper motor GOTO controller.
*/

#pragma once

#include "inditelescope.h"
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>

class AHPGT : public INDI::Telescope
{
    public:
        AHPGT();

    protected:
        bool Handshake() override;
        bool Disconnect() override;

        const char *getDefaultName() override;
        bool initProperties() override;

        bool SetTrackEnabled(bool enabled) override;
        bool ReadScopeStatus() override;
        bool Goto(double, double) override;
        bool Sync(double ra, double dec) override;
        bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        bool SetSlewRate(int index) override;
        bool SetTrackRate(double raRate, double deRate) override;
        bool SetTrackMode(uint8_t mode) override;
        bool updateLocation(double latitude, double longitude, double elevation) override;
        bool Abort() override;

    private:
        int SlewRate;
        double TrackRate[2];
        int TrackMode {0};

        bool isTracking {false};
        bool oldTracking {false};

        double currentRA {0};
        double currentDEC {90};
        double targetRA {0};
        double targetDEC {0};

        // Debug channel to write mount logs to
        // Default INDI::Logger debugging/logging channel are Message, Warn, Error, and Debug
        // Since scope information can be _very_ verbose, we create another channel SCOPE specifically
        // for extra debug logs. This way the user can turn it on/off as desired.
        uint8_t DBG_SCOPE { INDI::Logger::DBG_IGNORE };
};
