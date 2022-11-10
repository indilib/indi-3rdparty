/** \file indi_ahp_gt.cpp
    \brief Driver for the GT1 GOTO telescope mount controller.
    \author Ilia Platone

    \example indi_ahp_gt.cpp
    Driver for the GT1 GOTO telescope mount controller.
    See https://www.iliaplatone.com/gt1 for more information
*/

#include "indi_ahp_gt.h"
#include <ahp_gt.h>

#include "indicom.h"

#include <cmath>
#include <memory>

static std::unique_ptr<AHPGT> ahp_gt(new AHPGT());

AHPGT::AHPGT()
{
}

bool AHPGT::Disconnect()
{
    return EQMod::Disconnect();
}

bool AHPGT::Handshake()
{
    if(EQMod::Handshake()) {
        ahp_gt_select_device(0);
        if(!ahp_gt_connect_fd(PortFD)) {
            if(!ahp_gt_detect_device()) {
                ahp_gt_set_motor_steps(0, 200);
                ahp_gt_set_motor_teeth(0, 1);
                ahp_gt_read_values(0);
                ahp_gt_set_motor_steps(1, 200);
                ahp_gt_set_motor_teeth(1, 1);
                ahp_gt_read_values(1);
                return true;
            }
        }
    }
    Disconnect();
    return false;
}

bool AHPGT::initProperties()
{
    EQMod::initProperties();
    for (auto oneProperty : *getProperties())
    {
        oneProperty->setDeviceName(getDeviceName());
    }

    IUFillNumber(&GTRAConfigurationN[GT_MOTOR_STEPS], "GT_MOTOR_STEPS", "Motor steps", "%.0f", 1, 1000, 1, 200);
    IUFillNumber(&GTRAConfigurationN[GT_MOTOR_TEETH], "GT_MOTOR_TEETH", "Motor teeth", "%.3f", 1, 100000, 1, 1);
    IUFillNumber(&GTRAConfigurationN[GT_WORM_TEETH], "GT_WORM_TEETH", "Worm teeth", "%.3f", 1, 100000, 1, 4);
    IUFillNumber(&GTRAConfigurationN[GT_CROWN_TEETH], "GT_CROWN_TEETH", "Crown teeth", "%.0f", 1, 100000, 1, 180);
    IUFillNumber(&GTRAConfigurationN[GT_MAX_SPEED], "GT_MAX_SPEED", "Max speed", "%.0f", 1, 1000, 1, 800);
    IUFillNumber(&GTRAConfigurationN[GT_ACCELERATION], "GT_ACCELERATION", "Acceleration (deg)", "%.1f", 1, 20, 0.1, 1.0);
    IUFillNumberVector(&GTRAConfigurationNP, GTRAConfigurationN, GT_AXIS_N_PARAMS, getDeviceName(), "GT_RA_PARAMS", "RA Parameters", CONFIGURATION_TAB,
                       IP_RW, 60, IPS_IDLE);
    IUFillSwitch(&GTRAInvertAxisS[GT_INVERTED], "GT_INVERTED", "Invert RA Axis", ISS_OFF);
    IUFillSwitchVector(&GTRAInvertAxisSP, GTRAInvertAxisS, GT_N_INVERSION, getDeviceName(), "GT_RA_INVERT", "Invert RA Axis", CONFIGURATION_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillSwitch(&GTRASteppingModeS[GT_MIXED_MODE], "GT_MIXED_MODE", "Mixed", ISS_ON);
    IUFillSwitch(&GTRASteppingModeS[GT_MICROSTEPPING_MODE], "GT_MICROSTEPPING_MODE", "Microstepping", ISS_OFF);
    IUFillSwitch(&GTRASteppingModeS[GT_HALFSTEP_MODE], "GT_HALFSTEP_MODE", "Half-step", ISS_OFF);
    IUFillSwitchVector(&GTRASteppingModeSP, GTRASteppingModeS, GT_N_STEPPING_MODE, getDeviceName(), "GT_RA_STEPPING_MODE", "Invert RA Axis", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&GTRAWindingS[GT_AABB], "GT_AABB", "AABB", ISS_ON);
    IUFillSwitch(&GTRAWindingS[GT_ABAB], "GT_ABAB", "ABAB", ISS_OFF);
    IUFillSwitch(&GTRAWindingS[GT_ABBA], "GT_ABBA", "ABBA", ISS_OFF);
    IUFillSwitchVector(&GTRAWindingSP, GTRAWindingS, GT_N_WINDING_MODE, getDeviceName(), "GT_RA_WINDING", "RA motor windings", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&GTRAGPIOConfigS[GT_NONE], "GT_NONE", "Nothing", ISS_ON);
    IUFillSwitch(&GTRAGPIOConfigS[GT_ST4], "GT_ST4", "ST4", ISS_OFF);
    IUFillSwitch(&GTRAGPIOConfigS[GT_ENCODER], "GT_ENCODER", "Encoder", ISS_OFF);
    IUFillSwitch(&GTRAGPIOConfigS[GT_STEPDIR], "GT_STEPDIR", "Step/Dir", ISS_OFF);
    IUFillSwitchVector(&GTRAGPIOConfigSP, GTRAGPIOConfigS, GT_N_GPIO_CONFIG, getDeviceName(), "GT_RA_GPIO_CONFIG", "RA GPIO port", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&GTDEConfigurationN[GT_MOTOR_STEPS], "GT_MOTOR_STEPS", "Motor steps", "%.0f", 1, 1000, 1, 200);
    IUFillNumber(&GTDEConfigurationN[GT_MOTOR_TEETH], "GT_MOTOR_TEETH", "Motor teeth", "%.3f", 1, 100000, 1, 1);
    IUFillNumber(&GTDEConfigurationN[GT_WORM_TEETH], "GT_WORM_TEETH", "Worm teeth", "%.3f", 1, 100000, 1, 4);
    IUFillNumber(&GTDEConfigurationN[GT_CROWN_TEETH], "GT_CROWN_TEETH", "Crown teeth", "%.0f", 1, 100000, 1, 180);
    IUFillNumber(&GTDEConfigurationN[GT_MAX_SPEED], "GT_MAX_SPEED", "Max speed", "%.0f", 1, 1000, 1, 800);
    IUFillNumber(&GTDEConfigurationN[GT_ACCELERATION], "GT_ACCELERATION", "Acceleration (deg)", "%.1f", 1, 20, 0.1, 1.0);
    IUFillNumberVector(&GTDEConfigurationNP, GTDEConfigurationN, GT_AXIS_N_PARAMS, getDeviceName(), "GT_DE_PARAMS", "DEC Parameters", CONFIGURATION_TAB,
                       IP_RW, 60, IPS_IDLE);
    IUFillSwitch(&GTDEInvertAxisS[GT_INVERTED], "GT_INVERTED", "Invert DE Axis", ISS_OFF);
    IUFillSwitchVector(&GTDEInvertAxisSP, GTDEInvertAxisS, GT_N_INVERSION, getDeviceName(), "GT_DE_INVERT", "Invert DE Axis", CONFIGURATION_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillSwitch(&GTDESteppingModeS[GT_MIXED_MODE], "GT_MIXED_MODE", "Mixed", ISS_ON);
    IUFillSwitch(&GTDESteppingModeS[GT_MICROSTEPPING_MODE], "GT_MICROSTEPPING_MODE", "Microstepping", ISS_OFF);
    IUFillSwitch(&GTDESteppingModeS[GT_HALFSTEP_MODE], "GT_HALFSTEP_MODE", "Half-step", ISS_OFF);
    IUFillSwitchVector(&GTDESteppingModeSP, GTDESteppingModeS, GT_N_STEPPING_MODE, getDeviceName(), "GT_DE_STEPPING_MODE", "Invert DE Axis", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&GTDEWindingS[GT_AABB], "GT_AABB", "AABB", ISS_ON);
    IUFillSwitch(&GTDEWindingS[GT_ABAB], "GT_ABAB", "ABAB", ISS_OFF);
    IUFillSwitch(&GTDEWindingS[GT_ABBA], "GT_ABBA", "ABBA", ISS_OFF);
    IUFillSwitchVector(&GTDEWindingSP, GTDEWindingS, GT_N_WINDING_MODE, getDeviceName(), "GT_DE_WINDING", "DE motor windings", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&GTDEGPIOConfigS[GT_NONE], "GT_NONE", "Nothing", ISS_ON);
    IUFillSwitch(&GTDEGPIOConfigS[GT_ST4], "GT_ST4", "ST4", ISS_OFF);
    IUFillSwitch(&GTDEGPIOConfigS[GT_ENCODER], "GT_ENCODER", "Encoder", ISS_OFF);
    IUFillSwitch(&GTDEGPIOConfigS[GT_STEPDIR], "GT_STEPDIR", "Step/Dir", ISS_OFF);
    IUFillSwitchVector(&GTDEGPIOConfigSP, GTDEGPIOConfigS, GT_N_GPIO_CONFIG, getDeviceName(), "GT_DE_GPIO_CONFIG", "DE GPIO port", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillNumber(&GTConfigurationN[GT_PWM_FREQ], "GT_PWM_FREQ", "PWM Freq (Hz)", "%.0f", 1500, 8200, 700, 6400);
    IUFillNumberVector(&GTConfigurationNP, GTConfigurationN, GT_N_PARAMS, getDeviceName(), "GT_PARAMS", "Advanced", CONFIGURATION_TAB,
                       IP_RW, 60, IPS_IDLE);
    IUFillSwitch(&GTMountConfigS[GT_GEM], "GT_GEM", "German mount", ISS_ON);
    IUFillSwitch(&GTMountConfigS[GT_AZEQ], "GT_AZEQ", "AZ/EQ mount", ISS_OFF);
    IUFillSwitch(&GTMountConfigS[GT_FORK], "GT_FORK", "Fork mount", ISS_OFF);
    IUFillSwitchVector(&GTMountConfigSP, GTMountConfigS, GT_N_MOUNT_CONFIG, getDeviceName(), "GT_MOUNT_CONFIG", "Mount configuration", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    return true;
}

void AHPGT::ISGetProperties(const char *dev)
{
    EQMod::ISGetProperties(dev);
    if (isConnected())
    {
        defineProperty(&GTRAConfigurationNP);
        defineProperty(&GTRAInvertAxisSP);
        defineProperty(&GTRASteppingModeSP);
        defineProperty(&GTRAWindingSP);
        defineProperty(&GTRAGPIOConfigSP);
        defineProperty(&GTDEConfigurationNP);
        defineProperty(&GTDEInvertAxisSP);
        defineProperty(&GTDESteppingModeSP);
        defineProperty(&GTDEWindingSP);
        defineProperty(&GTDEGPIOConfigSP);
        defineProperty(&GTMountConfigSP);
        defineProperty(&GTConfigurationNP);
    }
}

bool AHPGT::updateProperties()
{
    EQMod::updateProperties();

    if (isConnected())
    {
        defineProperty(&GTRAConfigurationNP);
        defineProperty(&GTRAInvertAxisSP);
        defineProperty(&GTRASteppingModeSP);
        defineProperty(&GTRAWindingSP);
        defineProperty(&GTRAGPIOConfigSP);
        defineProperty(&GTDEConfigurationNP);
        defineProperty(&GTDEInvertAxisSP);
        defineProperty(&GTDESteppingModeSP);
        defineProperty(&GTDEWindingSP);
        defineProperty(&GTDEGPIOConfigSP);
        defineProperty(&GTMountConfigSP);
        defineProperty(&GTConfigurationNP);
        GTRAInvertAxisSP.sp[GT_INVERTED].s = ahp_gt_get_direction_invert(0) ? ISS_ON : ISS_OFF;
        IDSetSwitch(&GTRAInvertAxisSP, nullptr);
        for(int x = 0; x < GT_N_STEPPING_MODE; x++)
            GTRASteppingModeSP.sp[x].s = ISS_OFF;
        GTRASteppingModeSP.sp[ahp_gt_get_stepping_mode(0)].s = ISS_ON;
        IDSetSwitch(&GTRASteppingModeSP, nullptr);
        for(int x = 0; x < GT_N_WINDING_MODE; x++)
            GTRAWindingSP.sp[x].s = ISS_OFF;
        GTRAWindingSP.sp[ahp_gt_get_stepping_conf(0)].s = ISS_ON;
        IDSetSwitch(&GTRAWindingSP, nullptr);
        for(int x = 0; x < GT_N_GPIO_CONFIG; x++)
            GTRAGPIOConfigSP.sp[x].s = ISS_OFF;
        GTRAGPIOConfigSP.sp[ahp_gt_get_feature(0)].s = ISS_ON;
        IDSetSwitch(&GTRAGPIOConfigSP, nullptr);
        GTRAConfigurationNP.np[GT_MOTOR_STEPS].value = ahp_gt_get_motor_steps(0);
        GTRAConfigurationNP.np[GT_MOTOR_TEETH].value = ahp_gt_get_motor_teeth(0);
        GTRAConfigurationNP.np[GT_WORM_TEETH].value = ahp_gt_get_worm_teeth(0);
        GTRAConfigurationNP.np[GT_CROWN_TEETH].value = ahp_gt_get_crown_teeth(0);
        GTRAConfigurationNP.np[GT_MAX_SPEED].value = ahp_gt_get_max_speed(0);
        GTRAConfigurationNP.np[GT_ACCELERATION].value = ahp_gt_get_acceleration_angle(0)*180.0/M_PI;
        IDSetNumber(&GTRAConfigurationNP, nullptr);
        GTDEInvertAxisSP.sp[GT_INVERTED].s = ahp_gt_get_direction_invert(1) ? ISS_ON : ISS_OFF;
        IDSetSwitch(&GTDEInvertAxisSP, nullptr);
        for(int x = 0; x < GT_N_STEPPING_MODE; x++)
            GTDESteppingModeSP.sp[0].s = ISS_OFF;
        GTDESteppingModeSP.sp[ahp_gt_get_stepping_mode(1)].s = ISS_ON;
        IDSetSwitch(&GTDESteppingModeSP, nullptr);
        for(int x = 0; x < GT_N_WINDING_MODE; x++)
            GTDEWindingSP.sp[x].s = ISS_OFF;
        GTDEWindingSP.sp[ahp_gt_get_stepping_conf(1)].s = ISS_ON;
        IDSetSwitch(&GTDEWindingSP, nullptr);
        for(int x = 0; x < GT_N_GPIO_CONFIG; x++)
            GTDEGPIOConfigSP.sp[x].s = ISS_OFF;
        GTDEGPIOConfigSP.sp[ahp_gt_get_feature(1)].s = ISS_ON;
        IDSetSwitch(&GTDEGPIOConfigSP, nullptr);
        GTDEConfigurationNP.np[GT_MOTOR_STEPS].value = ahp_gt_get_motor_steps(1);
        GTDEConfigurationNP.np[GT_MOTOR_TEETH].value = ahp_gt_get_motor_teeth(1);
        GTDEConfigurationNP.np[GT_WORM_TEETH].value = ahp_gt_get_worm_teeth(1);
        GTDEConfigurationNP.np[GT_CROWN_TEETH].value = ahp_gt_get_crown_teeth(1);
        GTDEConfigurationNP.np[GT_MAX_SPEED].value = ahp_gt_get_max_speed(1);
        GTDEConfigurationNP.np[GT_ACCELERATION].value = ahp_gt_get_acceleration_angle(1)*180.0/M_PI;
        IDSetNumber(&GTDEConfigurationNP, nullptr);
        for(int x = 0; x < GT_N_MOUNT_CONFIG; x++)
            GTMountConfigSP.sp[x].s = ISS_OFF;
        int fork = (ahp_gt_get_mount_flags() & isForkMount) ? 1 : 0;
        int azeq = 0;
        azeq |= ((ahp_gt_get_features(0) & isAZEQ) != 0) ? 1 : 0;
        azeq |= ((ahp_gt_get_features(1) & isAZEQ) != 0) ? 1 : 0;
        if(azeq)
            GTMountConfigSP.sp[GT_AZEQ].s = ISS_ON;
        else if (fork)
            GTMountConfigSP.sp[GT_FORK].s = ISS_ON;
        else
            GTMountConfigSP.sp[GT_GEM].s = ISS_ON;
        IDSetSwitch(&GTMountConfigSP, nullptr);
        GTConfigurationNP.np[GT_PWM_FREQ].value = ahp_gt_get_pwm_frequency()*700+1500;
        IDSetNumber(&GTConfigurationNP, nullptr);
    } else {
        deleteProperty(GTRAConfigurationNP.name);
        deleteProperty(GTRAInvertAxisSP.name);
        deleteProperty(GTRASteppingModeSP.name);
        deleteProperty(GTRAWindingSP.name);
        deleteProperty(GTRAGPIOConfigSP.name);
        deleteProperty(GTDEConfigurationNP.name);
        deleteProperty(GTDEInvertAxisSP.name);
        deleteProperty(GTDESteppingModeSP.name);
        deleteProperty(GTDEWindingSP.name);
        deleteProperty(GTDEGPIOConfigSP.name);
        deleteProperty(GTMountConfigSP.name);
        deleteProperty(GTConfigurationNP.name);
    }
    return true;
}

bool AHPGT::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if(!strcmp(dev, getDeviceName())) {
        if(!strcmp(GTRAConfigurationNP.name, name)) {
            ahp_gt_set_motor_steps(0, GTRAConfigurationNP.np[GT_MOTOR_STEPS].value);
            ahp_gt_set_motor_teeth(0, GTRAConfigurationNP.np[GT_MOTOR_TEETH].value);
            ahp_gt_set_worm_teeth(0, GTRAConfigurationNP.np[GT_WORM_TEETH].value);
            ahp_gt_set_crown_teeth(0, GTRAConfigurationNP.np[GT_CROWN_TEETH].value);
            ahp_gt_set_max_speed(0, GTRAConfigurationNP.np[GT_MAX_SPEED].value);
            ahp_gt_set_acceleration_angle(0, GTRAConfigurationNP.np[GT_ACCELERATION].value * M_PI / 180.0);
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEConfigurationNP.name, name)) {
            ahp_gt_set_motor_steps(1, GTDEConfigurationNP.np[GT_MOTOR_STEPS].value);
            ahp_gt_set_motor_teeth(1, GTDEConfigurationNP.np[GT_MOTOR_TEETH].value);
            ahp_gt_set_worm_teeth(1, GTDEConfigurationNP.np[GT_WORM_TEETH].value);
            ahp_gt_set_crown_teeth(1, GTDEConfigurationNP.np[GT_CROWN_TEETH].value);
            ahp_gt_set_max_speed(1, GTDEConfigurationNP.np[GT_MAX_SPEED].value);
            ahp_gt_set_acceleration_angle(1, GTDEConfigurationNP.np[GT_ACCELERATION].value * M_PI / 180.0);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTConfigurationNP.name, name)) {
            ahp_gt_set_pwm_frequency((GTConfigurationNP.np[GT_PWM_FREQ].value-1500) / 700);
            ahp_gt_write_values(0, &progress, &write_finished);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
    }
    return EQMod::ISNewNumber(dev, name, values, names, n);
}

bool AHPGT::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName())) {
        if(!strcmp(GTMountConfigSP.name, name)) {
            int mount_type = IUFindOnSwitchIndex(&GTMountConfigSP);
            switch(mount_type) {
            case GT_GEM:
                ahp_gt_set_features(0, static_cast<SkywatcherFeature>(ahp_gt_get_features(0) & ~static_cast<int>(isAZEQ)));
                ahp_gt_set_features(1, static_cast<SkywatcherFeature>(ahp_gt_get_features(1) & ~static_cast<int>(isAZEQ)));
                ahp_gt_set_mount_flags(static_cast<GT1Flags>(0));
                break;
            case GT_AZEQ:
                ahp_gt_set_features(0, static_cast<SkywatcherFeature>(ahp_gt_get_features(0) | static_cast<int>(isAZEQ)));
                ahp_gt_set_features(1, static_cast<SkywatcherFeature>(ahp_gt_get_features(1) | static_cast<int>(isAZEQ)));
                ahp_gt_set_mount_flags(static_cast<GT1Flags>(0));
                break;
            case GT_FORK:
                ahp_gt_set_features(0, static_cast<SkywatcherFeature>(ahp_gt_get_features(0) & ~static_cast<int>(isAZEQ)));
                ahp_gt_set_features(1, static_cast<SkywatcherFeature>(ahp_gt_get_features(1) & ~static_cast<int>(isAZEQ)));
                ahp_gt_set_mount_flags(isForkMount);
                break;
            default:break;
            }
            ahp_gt_write_values(0, &progress, &write_finished);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRAInvertAxisSP.name, name)) {
            ahp_gt_set_direction_invert(0, GTRAInvertAxisSP.sp[GT_INVERTED].s == ISS_ON);
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRASteppingModeSP.name, name)) {
            ahp_gt_set_stepping_mode(0, static_cast<GT1SteppingMode>(IUFindOnSwitchIndex(&GTRASteppingModeSP)));
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRAWindingSP.name, name)) {
            ahp_gt_set_stepping_conf(0, static_cast<GT1SteppingConfiguration>(IUFindOnSwitchIndex(&GTRAWindingSP)));
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRAGPIOConfigSP.name, name)) {
            ahp_gt_set_feature(0, static_cast<GT1Feature>(IUFindOnSwitchIndex(&GTRAGPIOConfigSP)));
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEInvertAxisSP.name, name)) {
            ahp_gt_set_direction_invert(1, GTDEInvertAxisSP.sp[GT_INVERTED].s == ISS_ON);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDESteppingModeSP.name, name)) {
            ahp_gt_set_stepping_mode(1, static_cast<GT1SteppingMode>(IUFindOnSwitchIndex(&GTDESteppingModeSP)));
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEWindingSP.name, name)) {
            ahp_gt_set_stepping_conf(1, static_cast<GT1SteppingConfiguration>(IUFindOnSwitchIndex(&GTDEWindingSP)));
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEGPIOConfigSP.name, name)) {
            ahp_gt_set_feature(1, static_cast<GT1Feature>(IUFindOnSwitchIndex(&GTDEGPIOConfigSP)));
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
    }
    return EQMod::ISNewSwitch(dev, name, states, names, n);
}

const char *AHPGT::getDefaultName()
{
    return "AHP GT Mount";
}
