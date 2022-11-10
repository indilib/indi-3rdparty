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

#include "../indi-eqmod/eqmodbase.h"
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>

class AHPGT : public EQMod
{
    public:
        AHPGT();
        bool initProperties() override;
        bool updateProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool Handshake() override;
        bool Disconnect() override;

    private:
        enum gt_axis_params {
            GT_MOTOR_STEPS = 0,
            GT_MOTOR_TEETH,
            GT_WORM_TEETH,
            GT_CROWN_TEETH,
            GT_MAX_SPEED,
            GT_ACCELERATION,
            GT_AXIS_N_PARAMS
        };
        enum gt_params {
            GT_PWM_FREQ = 0,
            GT_N_PARAMS
        };
        enum gt_stepping_mode {
            GT_MIXED_MODE = 0,
            GT_MICROSTEPPING_MODE,
            GT_HALFSTEP_MODE,
            GT_N_STEPPING_MODE
        };
        enum gt_winding_mode {
            GT_AABB = 0,
            GT_ABAB,
            GT_ABBA,
            GT_N_WINDING_MODE
        };
        enum gt_axis_invert {
            GT_INVERTED = 0,
            GT_N_INVERSION
        };
        enum gt_gpio_config {
            GT_NONE = 0,
            GT_ST4,
            GT_ENCODER,
            GT_STEPDIR,
            GT_N_GPIO_CONFIG
        };
        enum gt_mount_config {
            GT_GEM = 0,
            GT_AZEQ,
            GT_FORK,
            GT_N_MOUNT_CONFIG
        };

        // Telescope & guider aperture and focal length
        INumber GTRAConfigurationN[GT_AXIS_N_PARAMS];
        INumberVectorProperty GTRAConfigurationNP;
        ISwitch GTRASteppingModeS[GT_N_STEPPING_MODE];
        ISwitchVectorProperty GTRASteppingModeSP;
        ISwitch GTRAWindingS[GT_N_WINDING_MODE];
        ISwitchVectorProperty GTRAWindingSP;
        ISwitch GTRAInvertAxisS[GT_N_INVERSION];
        ISwitchVectorProperty GTRAInvertAxisSP;
        ISwitch GTRAGPIOConfigS[GT_N_GPIO_CONFIG];
        ISwitchVectorProperty GTRAGPIOConfigSP;

        INumber GTDEConfigurationN[GT_AXIS_N_PARAMS];
        INumberVectorProperty GTDEConfigurationNP;
        ISwitch GTDESteppingModeS[GT_N_STEPPING_MODE];
        ISwitchVectorProperty GTDESteppingModeSP;
        ISwitch GTDEWindingS[GT_N_WINDING_MODE];
        ISwitchVectorProperty GTDEWindingSP;
        ISwitch GTDEInvertAxisS[GT_N_INVERSION];
        ISwitchVectorProperty GTDEInvertAxisSP;
        ISwitch GTDEGPIOConfigS[GT_N_GPIO_CONFIG];
        ISwitchVectorProperty GTDEGPIOConfigSP;

        ISwitch GTMountConfigS[3];
        ISwitchVectorProperty GTMountConfigSP;
        INumber GTConfigurationN[GT_N_PARAMS];
        INumberVectorProperty GTConfigurationNP;
        INumber GTProgressN[1];
        INumberVectorProperty GTProgressNP;

        int progress { 0 };
        int write_finished { 1 };
        const char *CONFIGURATION_TAB = "Firmware";
};
