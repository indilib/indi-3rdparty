#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "config.h"
#include "eqmodbase.h"


using ::testing::_;
using ::testing::StrEq;

class TestEQMod : public EQMod
{
public:
    TestEQMod()
    {
    zeroRAEncoder = 1000000;
    totalRAEncoder = 360000;
    zeroDEEncoder = 2000000;
    totalDEEncoder = 360000;
    initProperties();
    }

    bool TestEncoders() {

        uint32_t destep = totalDEEncoder / 360;
        // do not test at 90 degree edges because the result pier side is not stable because of float comparison
        uint32_t demin = zeroDEEncoder - totalDEEncoder / 4 + 1;
        uint32_t demax = zeroDEEncoder + totalDEEncoder * 3 / 4  - 1;

        uint32_t rastep = totalRAEncoder / 360;
        uint32_t ramin = zeroRAEncoder - totalRAEncoder / 2 + 1;
        uint32_t ramax = zeroRAEncoder + totalRAEncoder / 2 - 1;


        for (currentDEEncoder = demin; currentDEEncoder <= demax; currentDEEncoder += destep)
        {
            double de = EncoderToDegrees(currentDEEncoder, zeroDEEncoder, totalDEEncoder, Hemisphere);
            uint32_t currentDEEncoder_res = EncoderFromDegree(de, zeroDEEncoder, totalDEEncoder, Hemisphere);
            EXPECT_EQ(currentDEEncoder, currentDEEncoder_res);
        }

        for (currentRAEncoder = ramin; currentRAEncoder <= ramax; currentRAEncoder += rastep)
        {
            double ha = EncoderToHours(currentRAEncoder, zeroRAEncoder, totalRAEncoder, Hemisphere);
            uint32_t currentRAEncoder_res = EncoderFromHour(ha, zeroRAEncoder, totalRAEncoder, Hemisphere);
            EXPECT_EQ(currentRAEncoder, currentRAEncoder_res);
        }

        double lst;

        for (currentDEEncoder = demin; currentDEEncoder <= demax; currentDEEncoder += destep)
        {
            for (currentRAEncoder = ramin; currentRAEncoder <= ramax; currentRAEncoder += rastep)
            {
                for (lst = 0.0; lst < 24.0; lst++)
                {
                    EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &currentRA, &currentDEC, &currentHA, &currentPierSide);
                    uint32_t currentRAEncoder_res = EncoderFromRA(currentRA, currentPierSide, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
                    uint32_t currentDEEncoder_res = EncoderFromDec(currentDEC, currentPierSide, zeroDEEncoder, totalDEEncoder, Hemisphere);
                    EXPECT_EQ(currentDEEncoder, currentDEEncoder_res);
                    EXPECT_EQ(currentRAEncoder, currentRAEncoder_res);
                }
            }
        }
        return true;
    }

    bool TestEncoderTarget() {
        double lst;
        double ra, de;
        // do not test at 90 degree edges because the result pier side is not stable because of float comparison
        for (ra = 0.5; ra < 24.0; ra += 1.0)
        {
            for (de = -89.5; de <= 90.0; de += 1.0)
            {
                bzero(&gotoparams, sizeof(gotoparams));
                gotoparams.ratarget  = ra;
                gotoparams.detarget  = de;

                gotoparams.racurrentencoder = currentRAEncoder;
                gotoparams.decurrentencoder = currentDEEncoder;
                gotoparams.completed        = false;
                gotoparams.checklimits      = true;
                gotoparams.outsidelimits    = false;
                gotoparams.pier_side        = PIER_UNKNOWN; // Auto - keep counterweight down
                if (Hemisphere == NORTH)
                {
                    gotoparams.limiteast        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 13h
                    gotoparams.limitwest        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 23h
                }
                else
                {
                    gotoparams.limiteast        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 13h
                    gotoparams.limitwest        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 23h
                }

                juliandate = getJulianDate();
                lst        = getLst(juliandate, getLongitude());

                EncoderTarget(&gotoparams);
                EncodersToRADec(gotoparams.ratargetencoder, gotoparams.detargetencoder, lst, &currentRA, &currentDEC, &currentHA, nullptr);
                EXPECT_NEAR(ra, currentRA, 0.001) << "ra=" << ra << " dec=" << de << std::endl;
                EXPECT_NEAR(de, currentDEC, 0.001) << "ra=" << ra << " dec=" << de << std::endl;

                // With counterweight down it can't go outside limits
                EXPECT_FALSE(gotoparams.outsidelimits) << "limiteast=" << gotoparams.limiteast << " limitwest=" << gotoparams.limitwest << "  pier_side=" << gotoparams.pier_side << " ratargetencoder=" << gotoparams.ratargetencoder << std::endl ;
            }
        }

        return true;
    }

    bool TestHemisphereSymmetry() {

        uint32_t destep = totalDEEncoder / 36;
        // do not test at 90 degree edges because the result pier side is not stable because of float comparison
        uint32_t demin = zeroDEEncoder - totalDEEncoder / 4 + 1;
        uint32_t demax = zeroDEEncoder + totalDEEncoder * 3 / 4  - 1;

        uint32_t rastep = totalRAEncoder / 36;
        uint32_t ramin = zeroRAEncoder - totalRAEncoder / 2 + 1;
        uint32_t ramax = zeroRAEncoder + totalRAEncoder / 2 - 1;


        // test with lst = 0.0 and longitude = 0.0 so we can assume RA = HA
        double lst = 0.0;

        for (currentDEEncoder = demin; currentDEEncoder <= demax; currentDEEncoder += destep)
        {
            for (currentRAEncoder = ramin; currentRAEncoder <= ramax; currentRAEncoder += rastep)
            {
                updateLocation(50.0, 0.0, 0);
                double RA_North, DEC_North, HA_North;
                TelescopePierSide PierSide_North;
                EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &RA_North, &DEC_North, &HA_North, &PierSide_North);

                updateLocation(-50.0, 0.0, 0);
                double RA_South, DEC_South, HA_South;
                TelescopePierSide PierSide_South;
                EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &RA_South, &DEC_South, &HA_South, &PierSide_South);

                EXPECT_NEAR(RA_North, 24.0 - RA_South, 0.001);
                EXPECT_NEAR(DEC_North, -DEC_South, 0.001);
                EXPECT_NE(PierSide_North, PierSide_South);
            }
        }
        return true;
    }

};


TEST(EqmodTest, hemisphere_symmetry)
{
    TestEQMod eqmod;
    eqmod.TestHemisphereSymmetry();
}

TEST(EqmodTest, encoders_north)
{
    TestEQMod eqmod;
    eqmod.updateLocation(50.0, 15.0, 0);
    eqmod.TestEncoders();
}

TEST(EqmodTest, encoders_south)
{
    TestEQMod eqmod;
    eqmod.updateLocation(-50.0, 15.0, 0);
    eqmod.TestEncoders();
}


TEST(EqmodTest, encoder_target_north)
{
    TestEQMod eqmod;
    eqmod.updateLocation(50.0, 15.0, 0);
    eqmod.TestEncoderTarget();
}

TEST(EqmodTest, encoder_target_south)
{
    TestEQMod eqmod;
    eqmod.updateLocation(-50.0, 15.0, 0);
    eqmod.TestEncoderTarget();
}

#ifdef WITH_SCOPE_LIMITS
TEST(EqmodTest, scope_limits_properties)
{
    TestEQMod eqmod;

    {
        auto p = eqmod.getText("HORIZONLIMITSDATAFILE");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSFILENAME"), nullptr);
        }
    }
    {
        auto p = eqmod.getNumber("HORIZONLIMITSPOINT");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITS_POINT_AZ"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITS_POINT_ALT"), nullptr);
        }
    }
    {
        auto p = eqmod.getSwitch("HORIZONLIMITSTRAVERSE");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTFIRST"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTPREV"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTNEXT"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTLAST"), nullptr);
        }
    }
    {
        auto p = eqmod.getSwitch("HORIZONLIMITSMANAGE");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTADDCURRENT"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTDELETE"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLISTCLEAR"), nullptr);
        }
    }
    {
        auto p = eqmod.getSwitch("HORIZONLIMITSFILEOPERATION");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSWRITEFILE"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLOADFILE"), nullptr);
        }
    }
    {
        auto p = eqmod.getSwitch("HORIZONLIMITSONLIMIT");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSONLIMITTRACK"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSONLIMITSLEW"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSONLIMITGOTO"), nullptr);
        }
    }
    {
        auto p = eqmod.getSwitch("HORIZONLIMITSLIMITGOTO");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLIMITGOTODISABLE"), nullptr);
            EXPECT_NE(p.findWidgetByName("HORIZONLIMITSLIMITGOTOENABLE"), nullptr);
        }
    }
    {
        auto p = eqmod.getBLOB("HORIZONLIMITSDATAFITS");
        EXPECT_TRUE(p.isValid());
        if (p)
        {
            EXPECT_NE(p.findWidgetByName("HORIZONPOINTS"), nullptr);
        }
    }
}

TEST(EqmodTest, scope_limits_empty)
{
    TestEQMod eqmod;
    eqmod.updateLocation(50.0, 15.0, 0);

    auto onlimit = eqmod.getSwitch("HORIZONLIMITSONLIMIT");
    ASSERT_TRUE(onlimit.isValid());
    auto limittrack = onlimit.findWidgetByName("HORIZONLIMITSONLIMITTRACK");
    ASSERT_NE(limittrack, nullptr);
    auto limitslew = onlimit.findWidgetByName("HORIZONLIMITSONLIMITSLEW");
    ASSERT_NE(limitslew, nullptr);
    auto limitgoto = onlimit.findWidgetByName("HORIZONLIMITSONLIMITGOTO");
    ASSERT_NE(limitgoto, nullptr);

    HorizonLimits * const hl = eqmod.horizon;
    ASSERT_NE(hl, nullptr);

    // Because there are no horizon limits set, any altitude under the horizon will trigger the limit check
    // So use that to test switches appropriately
    for (int ftrack = ISS_OFF; ftrack <= ISS_ON; ftrack++)
    {
        limittrack->s = (ISState) ftrack;
        for (int fslew = ISS_OFF; fslew <= ISS_ON; fslew++)
        {
            limitslew->s = (ISState) fslew;
            for (int fgoto = ISS_OFF; fgoto <= ISS_ON; fgoto++)
            {
                limitgoto->s = (ISState) fgoto;

                onlimit.apply();

                // Over the horizon (0 <= alt), so always outside limits
                for (double alt = 0; alt < +90; alt += 0.7)
                {
                    for (double az = -365; az < 365; az += 0.7)
                    {
                        // Inside limits, no aborts
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_IDLE, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_SLEWING, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_SLEWING, true), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_TRACKING, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_TRACKING, true), false);

                        // Remaining tests are improbable, and won't abort anything
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_IDLE, true), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKING, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKING, true), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKED, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKED, true), false);
                    }
                }

                // On or under horizon (alt < 0 strictly), so always outside limits
                // Those tests output warnings thus are slower, so use larger verification strides
                for (double alt = -0.001; -90 < alt; alt -= 10.1)
                {
                    for (double az = -365; az < 365; az += 10.1)
                    {
                        // When idle, limits are not tested
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_IDLE, false), false);

                        // When slewing, limits may abort move without goto, and gotos
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_SLEWING, false), fslew == ISS_ON);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_SLEWING, true), fgoto == ISS_ON);

                        // When tracking, limits may abort move, also in the edge case of tracking during goto
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_TRACKING, false), ftrack == ISS_ON);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_TRACKING, true), ftrack == ISS_ON);

                        // Remaining tests are improbable, and won't abort anything
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_IDLE, true), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKING, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKING, true), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKED, false), false);
                        ASSERT_EQ(hl->checkLimits(az, alt, INDI::Telescope::SCOPE_PARKED, true), false);
                    }
                }
            }
        }
    }
}

TEST(EqmodTest, scope_limits_altaz)
{
    TestEQMod eqmod;
    eqmod.updateLocation(50.0, 15.0, 0);

    auto onlimit = eqmod.getSwitch("HORIZONLIMITSONLIMIT");
    ASSERT_TRUE(onlimit.isValid());
    auto limittrack = onlimit.findWidgetByName("HORIZONLIMITSONLIMITTRACK");
    ASSERT_NE(limittrack, nullptr);
    auto limitslew = onlimit.findWidgetByName("HORIZONLIMITSONLIMITSLEW");
    ASSERT_NE(limitslew, nullptr);
    auto limitgoto = onlimit.findWidgetByName("HORIZONLIMITSONLIMITGOTO");
    ASSERT_NE(limitgoto, nullptr);

    HorizonLimits * const hl = eqmod.horizon;
    ASSERT_NE(hl, nullptr);

    // Use a configuration that aborts tracking out of limits
    limittrack->setState(ISS_ON);
    limitslew->setState(ISS_OFF);
    limitgoto->setState(ISS_OFF);
    onlimit.apply();

    // Retrieve points properties
    auto ppoint = eqmod.getNumber("HORIZONTAL_COORD");
    ASSERT_TRUE(ppoint.isValid());
    auto paz = ppoint.findWidgetByName("AZ");
    ASSERT_NE(paz, nullptr);
    auto palt = ppoint.findWidgetByName("ALT");
    ASSERT_NE(palt, nullptr);

    // Retrieve points management properties
    auto pmanage = eqmod.getSwitch("HORIZONLIMITSMANAGE");
    ASSERT_TRUE(pmanage.isValid());
    auto padd = pmanage.findWidgetByName("HORIZONLIMITSLISTADDCURRENT");
    ASSERT_NE(padd, nullptr);
    auto pclear = pmanage.findWidgetByName("HORIZONLIMITSLISTCLEAR");
    ASSERT_NE(pclear, nullptr);

    ISState iss_on[] = { ISS_ON };
    const char * manage_add[] = { "HORIZONLIMITSLISTADDCURRENT" };
    const char * manage_clear[] = { "HORIZONLIMITSLISTCLEAR" };

    // Add a single alt-az horizon point and test edge case points, then clear horizon points
    paz->value = 30;
    palt->value = 45;
    ppoint.apply();
    ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_add, 1));
    ASSERT_EQ(hl->checkLimits(30, 50, INDI::Telescope::SCOPE_TRACKING, false), false);
    ASSERT_EQ(hl->checkLimits(30, 40, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(20, 50, INDI::Telescope::SCOPE_TRACKING, false), false);
    ASSERT_EQ(hl->checkLimits(20, 40, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(40, 50, INDI::Telescope::SCOPE_TRACKING, false), false);
    ASSERT_EQ(hl->checkLimits(40, 40, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_clear, 1));

    // Try out some altitudes in horizontal circles, limit must trig as soon as we go lower than horizon altitude - no interpolation here
    for (palt->value = 0; palt->value <= 90; palt->value += 10)
    {
        for (paz->value = 0; paz->value < 360; paz->value += 60)
        {
            ppoint.apply();
            ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_add, 1));
        }

        for (double test_alt = 0; test_alt < 90; test_alt += 8.4)
            for (double test_az = -365; test_az < +365; test_az += 26.7)
                ASSERT_EQ(hl->checkLimits(test_az, test_alt, INDI::Telescope::SCOPE_TRACKING, false), test_alt < palt->value);

        ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_clear, 1));
    }

    // Try out increasing altitude to test interpolation
    paz->value = 0;
    palt->value = 10;
    ppoint.apply();
    ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_add, 1));

    paz->value = 180;
    palt->value = 20;
    ppoint.apply();
    ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_add, 1));

    // Test at horizon points
    ASSERT_EQ(hl->checkLimits(0, 9, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(0, 10, INDI::Telescope::SCOPE_TRACKING, false), false);
    ASSERT_EQ(hl->checkLimits(180, 19, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(180, 20, INDI::Telescope::SCOPE_TRACKING, false), false);

    // Test in middles of horizon segments
    ASSERT_EQ(hl->checkLimits(90, 14, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(90, 15, INDI::Telescope::SCOPE_TRACKING, false), false);
    ASSERT_EQ(hl->checkLimits(270, 14, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(270, 15, INDI::Telescope::SCOPE_TRACKING, false), false);

    // Test in quarters of horizon segments
    ASSERT_EQ(hl->checkLimits(45, 15, INDI::Telescope::SCOPE_TRACKING, false), false);
    ASSERT_EQ(hl->checkLimits(135, 15, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(225, 15, INDI::Telescope::SCOPE_TRACKING, false), true);
    ASSERT_EQ(hl->checkLimits(315, 15, INDI::Telescope::SCOPE_TRACKING, false), false);

    ASSERT_TRUE(hl->ISNewSwitch(eqmod.getDeviceName(), "HORIZONLIMITSMANAGE", iss_on, (char**) manage_clear, 1));
}
#endif

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    me = strdup("indi_eqmod_driver");

    return RUN_ALL_TESTS();
}
