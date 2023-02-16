/** \file indi_ahp_gt.cpp
    \brief Driver for the GT1 GOTO telescope mount controller.
    \author Ilia Platone

    \example indi_ahp_gt.cpp
    Driver for the GT1 GOTO telescope mount controller.
    See https://www.iliaplatone.com/gt1 for more information
*/

#include "ahpgtbase.h"
#include <ahp_gt.h>

#include "indicom.h"

#include <cmath>
#include <memory>

AHPGTBase::AHPGTBase() : EQMod()
{
}

bool AHPGTBase::Disconnect()
{
    return EQMod::Disconnect();
}

bool AHPGTBase::Handshake()
{
    if(EQMod::Handshake())
    {
        if(!ahp_gt_connect_fd(PortFD))
        {
            if(!ahp_gt_detect_device())
            {
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

bool AHPGTBase::initProperties()
{
    EQMod::initProperties();
    for (auto oneProperty : *getProperties())
    {
        oneProperty.setDeviceName(getDeviceName());
    }

    GTRAConfigurationNP[GT_MOTOR_STEPS].fill("GT_MOTOR_STEPS", "Motor steps", "%.0f", 1, 1000, 1, 200);
    GTRAConfigurationNP[GT_MOTOR_TEETH].fill("GT_MOTOR_TEETH", "Motor teeth", "%.0f", 1, 100000, 1, 1);
    GTRAConfigurationNP[GT_WORM_TEETH].fill("GT_WORM_TEETH", "Worm teeth", "%.0f", 1, 100000, 1, 4);
    GTRAConfigurationNP[GT_CROWN_TEETH].fill("GT_CROWN_TEETH", "Crown teeth", "%.0f", 1, 100000, 1, 180);
    GTRAConfigurationNP[GT_MAX_SPEED].fill("GT_MAX_SPEED", "Max speed", "%.0f", 1, 1000, 1, 800);
    GTRAConfigurationNP[GT_ACCELERATION].fill("GT_ACCELERATION", "Acceleration (deg)", "%.1f", 1, 20, 0.1, 1.0);
    GTRAConfigurationNP.fill(getDeviceName(), "GT_RA_PARAMS", "RA Parameters", CONFIGURATION_TAB,
                             IP_RW, 60, IPS_IDLE);
    GTRAInvertAxisSP[GT_INVERTED].fill("GT_INVERTED", "Invert RA Axis", ISS_OFF);
    GTRAInvertAxisSP.fill(getDeviceName(), "GT_RA_INVERT", "Invert RA Axis", CONFIGURATION_TAB,
                          IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    GTRASteppingModeSP[GT_MIXED_MODE].fill("GT_MIXED_MODE", "Mixed", ISS_ON);
    GTRASteppingModeSP[GT_MICROSTEPPING_MODE].fill("GT_MICROSTEPPING_MODE", "Microstepping", ISS_OFF);
    GTRASteppingModeSP[GT_HALFSTEP_MODE].fill("GT_HALFSTEP_MODE", "Half-step", ISS_OFF);
    GTRASteppingModeSP.fill(getDeviceName(), "GT_RA_STEPPING_MODE", "Invert RA Axis", CONFIGURATION_TAB,
                            IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    GTRAWindingSP[GT_AABB].fill("GT_AABB", "AABB", ISS_ON);
    GTRAWindingSP[GT_ABAB].fill("GT_ABAB", "ABAB", ISS_OFF);
    GTRAWindingSP[GT_ABBA].fill("GT_ABBA", "ABBA", ISS_OFF);
    GTRAWindingSP.fill(getDeviceName(), "GT_RA_WINDING", "RA motor windings", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    GTRAGPIOConfigSP[GT_NONE].fill("GT_NONE", "Nothing", ISS_ON);
    GTRAGPIOConfigSP[GT_ST4].fill("GT_ST4", "ST4", ISS_OFF);
    GTRAGPIOConfigSP[GT_ENCODER].fill("GT_ENCODER", "Encoder", ISS_OFF);
    GTRAGPIOConfigSP[GT_STEPDIR].fill("GT_STEPDIR", "Step/Dir", ISS_OFF);
    GTRAGPIOConfigSP.fill(getDeviceName(), "GT_RA_GPIO_CONFIG", "RA GPIO port", CONFIGURATION_TAB,
                          IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    GTDEConfigurationNP[GT_MOTOR_STEPS].fill("GT_MOTOR_STEPS", "Motor steps", "%.0f", 1, 1000, 1, 200);
    GTDEConfigurationNP[GT_MOTOR_TEETH].fill("GT_MOTOR_TEETH", "Motor teeth", "%.0f", 1, 100000, 1, 1);
    GTDEConfigurationNP[GT_WORM_TEETH].fill("GT_WORM_TEETH", "Worm teeth", "%.0f", 1, 100000, 1, 4);
    GTDEConfigurationNP[GT_CROWN_TEETH].fill("GT_CROWN_TEETH", "Crown teeth", "%.0f", 1, 100000, 1, 180);
    GTDEConfigurationNP[GT_MAX_SPEED].fill("GT_MAX_SPEED", "Max speed", "%.0f", 1, 1000, 1, 800);
    GTDEConfigurationNP[GT_ACCELERATION].fill("GT_ACCELERATION", "Acceleration (deg)", "%.1f", 1, 20, 0.1, 1.0);
    GTDEConfigurationNP.fill(getDeviceName(), "GT_DE_PARAMS", "DEC Parameters", CONFIGURATION_TAB,
                             IP_RW, 60, IPS_IDLE);
    GTDEInvertAxisSP[GT_INVERTED].fill("GT_INVERTED", "Invert DE Axis", ISS_OFF);
    GTDEInvertAxisSP.fill(getDeviceName(), "GT_DE_INVERT", "Invert DE Axis", CONFIGURATION_TAB,
                          IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    GTDESteppingModeSP[GT_MIXED_MODE].fill("GT_MIXED_MODE", "Mixed", ISS_ON);
    GTDESteppingModeSP[GT_MICROSTEPPING_MODE].fill("GT_MICROSTEPPING_MODE", "Microstepping", ISS_OFF);
    GTDESteppingModeSP[GT_HALFSTEP_MODE].fill("GT_HALFSTEP_MODE", "Half-step", ISS_OFF);
    GTDESteppingModeSP.fill(getDeviceName(), "GT_DE_STEPPING_MODE", "Invert DE Axis", CONFIGURATION_TAB,
                            IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    GTDEWindingSP[GT_AABB].fill("GT_AABB", "AABB", ISS_ON);
    GTDEWindingSP[GT_ABAB].fill("GT_ABAB", "ABAB", ISS_OFF);
    GTDEWindingSP[GT_ABBA].fill("GT_ABBA", "ABBA", ISS_OFF);
    GTDEWindingSP.fill(getDeviceName(), "GT_DE_WINDING", "DE motor windings", CONFIGURATION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    GTDEGPIOConfigSP[GT_NONE].fill("GT_NONE", "Nothing", ISS_ON);
    GTDEGPIOConfigSP[GT_ST4].fill("GT_ST4", "ST4", ISS_OFF);
    GTDEGPIOConfigSP[GT_ENCODER].fill("GT_ENCODER", "Encoder", ISS_OFF);
    GTDEGPIOConfigSP[GT_STEPDIR].fill("GT_STEPDIR", "Step/Dir", ISS_OFF);
    GTDEGPIOConfigSP.fill(getDeviceName(), "GT_DE_GPIO_CONFIG", "DE GPIO port", CONFIGURATION_TAB,
                          IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    GTConfigurationNP[GT_PWM_FREQ].fill("GT_PWM_FREQ", "PWM Freq (Hz)", "%.0f", 1500, 8200, 700, 6400);
    GTConfigurationNP.fill(getDeviceName(), "GT_PARAMS", "Advanced", CONFIGURATION_TAB,
                           IP_RW, 60, IPS_IDLE);
    GTMountConfigSP[GT_GEM].fill("GT_GEM", "German mount", ISS_ON);
    GTMountConfigSP[GT_AZEQ].fill("GT_AZEQ", "AZ/EQ mount", ISS_OFF);
    GTMountConfigSP[GT_FORK].fill("GT_FORK", "Fork mount", ISS_OFF);
    GTMountConfigSP.fill(getDeviceName(), "GT_MOUNT_CONFIG", "Mount configuration", CONFIGURATION_TAB,
                         IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    return true;
}

void AHPGTBase::ISGetProperties(const char *dev)
{
    EQMod::ISGetProperties(dev);
    if (isConnected())
    {
        defineProperty(GTRAConfigurationNP);
        defineProperty(GTRAInvertAxisSP);
        defineProperty(GTRASteppingModeSP);
        defineProperty(GTRAWindingSP);
        defineProperty(GTRAGPIOConfigSP);
        defineProperty(GTDEConfigurationNP);
        defineProperty(GTDEInvertAxisSP);
        defineProperty(GTDESteppingModeSP);
        defineProperty(GTDEWindingSP);
        defineProperty(GTDEGPIOConfigSP);
        defineProperty(GTMountConfigSP);
        defineProperty(GTConfigurationNP);
    }
}

bool AHPGTBase::updateProperties()
{
    EQMod::updateProperties();

    if (isConnected())
    {
        defineProperty(GTRAConfigurationNP);
        defineProperty(GTRAInvertAxisSP);
        defineProperty(GTRASteppingModeSP);
        defineProperty(GTRAWindingSP);
        defineProperty(GTRAGPIOConfigSP);
        defineProperty(GTDEConfigurationNP);
        defineProperty(GTDEInvertAxisSP);
        defineProperty(GTDESteppingModeSP);
        defineProperty(GTDEWindingSP);
        defineProperty(GTDEGPIOConfigSP);
        defineProperty(GTMountConfigSP);
        defineProperty(GTConfigurationNP);
        GTRAInvertAxisSP[GT_INVERTED].setState(ahp_gt_get_direction_invert(0) ? ISS_ON : ISS_OFF);
        GTRAInvertAxisSP.apply();
        for(int x = 0; x < GT_N_STEPPING_MODE; x++)
            GTRASteppingModeSP[x].setState(ISS_OFF);
        GTRASteppingModeSP[ahp_gt_get_stepping_mode(0)].setState(ISS_ON);
        GTRASteppingModeSP.apply();
        for(int x = 0; x < GT_N_WINDING_MODE; x++)
            GTRAWindingSP[x].setState(ISS_OFF);
        GTRAWindingSP[ahp_gt_get_stepping_conf(0)].setState(ISS_ON);
        GTRAWindingSP.apply();
        for(int x = 0; x < GT_N_GPIO_CONFIG; x++)
            GTRAGPIOConfigSP[x].setState(ISS_OFF);
        GTRAGPIOConfigSP[ahp_gt_get_feature(0)].setState(ISS_ON);
        GTRAGPIOConfigSP.apply();
        GTRAConfigurationNP[GT_MOTOR_STEPS].setValue(ahp_gt_get_motor_steps(0));
        GTRAConfigurationNP[GT_MOTOR_TEETH].setValue(ahp_gt_get_motor_teeth(0));
        GTRAConfigurationNP[GT_WORM_TEETH].setValue(ahp_gt_get_worm_teeth(0));
        GTRAConfigurationNP[GT_CROWN_TEETH].setValue(ahp_gt_get_crown_teeth(0));
        GTRAConfigurationNP[GT_MAX_SPEED].setValue(ahp_gt_get_max_speed(0));
        GTRAConfigurationNP[GT_ACCELERATION].setValue(ahp_gt_get_acceleration_angle(0) * 180.0 / M_PI);
        GTRAConfigurationNP.apply();
        GTDEInvertAxisSP[GT_INVERTED].setState(ahp_gt_get_direction_invert(1) ? ISS_ON : ISS_OFF);
        GTDEInvertAxisSP.apply();
        for(int x = 0; x < GT_N_STEPPING_MODE; x++)
            GTDESteppingModeSP[0].setState(ISS_OFF);
        GTDESteppingModeSP[ahp_gt_get_stepping_mode(1)].setState(ISS_ON);
        GTDESteppingModeSP.apply();
        for(int x = 0; x < GT_N_WINDING_MODE; x++)
            GTDEWindingSP[x].setState(ISS_OFF);
        GTDEWindingSP[ahp_gt_get_stepping_conf(1)].setState(ISS_ON);
        GTDEWindingSP.apply();
        for(int x = 0; x < GT_N_GPIO_CONFIG; x++)
            GTDEGPIOConfigSP[x].setState(ISS_OFF);
        GTDEGPIOConfigSP[ahp_gt_get_feature(1)].setState(ISS_ON);
        GTDEGPIOConfigSP.apply();
        GTDEConfigurationNP[GT_MOTOR_STEPS].setValue(ahp_gt_get_motor_steps(1));
        GTDEConfigurationNP[GT_MOTOR_TEETH].setValue(ahp_gt_get_motor_teeth(1));
        GTDEConfigurationNP[GT_WORM_TEETH].setValue(ahp_gt_get_worm_teeth(1));
        GTDEConfigurationNP[GT_CROWN_TEETH].setValue(ahp_gt_get_crown_teeth(1));
        GTDEConfigurationNP[GT_MAX_SPEED].setValue(ahp_gt_get_max_speed(1));
        GTDEConfigurationNP[GT_ACCELERATION].setValue(ahp_gt_get_acceleration_angle(1) * 180.0 / M_PI);
        GTDEConfigurationNP.apply();
        for(int x = 0; x < GT_N_MOUNT_CONFIG; x++)
            GTMountConfigSP[x].setState(ISS_OFF);
        int fork = (ahp_gt_get_mount_flags() & isForkMount) ? 1 : 0;
        int azeq = 0;
        azeq |= ((ahp_gt_get_features(0) & isAZEQ) != 0) ? 1 : 0;
        azeq |= ((ahp_gt_get_features(1) & isAZEQ) != 0) ? 1 : 0;
        if(azeq)
            GTMountConfigSP[GT_AZEQ].setState(ISS_ON);
        else if (fork)
            GTMountConfigSP[GT_FORK].setState(ISS_ON);
        else
            GTMountConfigSP[GT_GEM].setState(ISS_ON);
        GTMountConfigSP.apply();
        GTConfigurationNP[GT_PWM_FREQ].setValue(ahp_gt_get_pwm_frequency() * 700 + 1500);
        GTConfigurationNP.apply();
    }
    else
    {
        deleteProperty(GTRAConfigurationNP.getName());
        deleteProperty(GTRAInvertAxisSP.getName());
        deleteProperty(GTRASteppingModeSP.getName());
        deleteProperty(GTRAWindingSP.getName());
        deleteProperty(GTRAGPIOConfigSP.getName());
        deleteProperty(GTDEConfigurationNP.getName());
        deleteProperty(GTDEInvertAxisSP.getName());
        deleteProperty(GTDESteppingModeSP.getName());
        deleteProperty(GTDEWindingSP.getName());
        deleteProperty(GTDEGPIOConfigSP.getName());
        deleteProperty(GTMountConfigSP.getName());
        deleteProperty(GTConfigurationNP.getName());
    }
    return true;
}

bool AHPGTBase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        if(!strcmp(GTRAConfigurationNP.getName(), name))
        {
            ahp_gt_set_motor_steps(0, GTRAConfigurationNP[GT_MOTOR_STEPS].getValue());
            ahp_gt_set_motor_teeth(0, GTRAConfigurationNP[GT_MOTOR_TEETH].getValue());
            ahp_gt_set_worm_teeth(0, GTRAConfigurationNP[GT_WORM_TEETH].getValue());
            ahp_gt_set_crown_teeth(0, GTRAConfigurationNP[GT_CROWN_TEETH].getValue());
            ahp_gt_set_max_speed(0, GTRAConfigurationNP[GT_MAX_SPEED].getValue());
            ahp_gt_set_acceleration_angle(0, GTRAConfigurationNP[GT_ACCELERATION].getValue() * M_PI / 180.0);
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEConfigurationNP.getName(), name))
        {
            ahp_gt_set_motor_steps(1, GTDEConfigurationNP[GT_MOTOR_STEPS].getValue());
            ahp_gt_set_motor_teeth(1, GTDEConfigurationNP[GT_MOTOR_TEETH].getValue());
            ahp_gt_set_worm_teeth(1, GTDEConfigurationNP[GT_WORM_TEETH].getValue());
            ahp_gt_set_crown_teeth(1, GTDEConfigurationNP[GT_CROWN_TEETH].getValue());
            ahp_gt_set_max_speed(1, GTDEConfigurationNP[GT_MAX_SPEED].getValue());
            ahp_gt_set_acceleration_angle(1, GTDEConfigurationNP[GT_ACCELERATION].getValue() * M_PI / 180.0);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTConfigurationNP.getName(), name))
        {
            ahp_gt_set_pwm_frequency((GTConfigurationNP[GT_PWM_FREQ].getValue() - 1500) / 700);
            ahp_gt_write_values(0, &progress, &write_finished);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
    }
    return EQMod::ISNewNumber(dev, name, values, names, n);
}

bool AHPGTBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        if(!strcmp(GTMountConfigSP.getName(), name))
        {
            int mount_type = GTMountConfigSP.findOnSwitchIndex();
            switch(mount_type)
            {
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
                default:
                    break;
            }
            ahp_gt_write_values(0, &progress, &write_finished);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRAInvertAxisSP.getName(), name))
        {
            ahp_gt_set_direction_invert(0, GTRAInvertAxisSP[GT_INVERTED].s == ISS_ON);
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRASteppingModeSP.getName(), name))
        {
            ahp_gt_set_stepping_mode(0, static_cast<GT1SteppingMode>(GTRASteppingModeSP.findOnSwitchIndex()));
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRAWindingSP.getName(), name))
        {
            ahp_gt_set_stepping_conf(0, static_cast<GT1SteppingConfiguration>(GTRAWindingSP.findOnSwitchIndex()));
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTRAGPIOConfigSP.getName(), name))
        {
            ahp_gt_set_feature(0, static_cast<GT1Feature>(GTRAGPIOConfigSP.findOnSwitchIndex()));
            ahp_gt_write_values(0, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEInvertAxisSP.getName(), name))
        {
            ahp_gt_set_direction_invert(1, GTDEInvertAxisSP[GT_INVERTED].s == ISS_ON);
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDESteppingModeSP.getName(), name))
        {
            ahp_gt_set_stepping_mode(1, static_cast<GT1SteppingMode>(GTDESteppingModeSP.findOnSwitchIndex()));
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEWindingSP.getName(), name))
        {
            ahp_gt_set_stepping_conf(1, static_cast<GT1SteppingConfiguration>(GTDEWindingSP.findOnSwitchIndex()));
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
        if(!strcmp(GTDEGPIOConfigSP.getName(), name))
        {
            ahp_gt_set_feature(1, static_cast<GT1Feature>(GTDEGPIOConfigSP.findOnSwitchIndex()));
            ahp_gt_write_values(1, &progress, &write_finished);
            updateProperties();
        }
    }
    return EQMod::ISNewSwitch(dev, name, states, names, n);
}

const char *AHPGTBase::getDefaultName()
{
    return "AHP GT Mount";
}
