/**
   This file is part of the AAG Cloud Watcher INDI Driver.

   Copyright (C) 2026 Vilen Sharifov (vilen.sharifov@gmail.com)

   AAG Cloud Watcher INDI Driver is free software : you can redistribute it
   and / or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   AAG Cloud Watcher INDI Driver is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with AAG Cloud Watcher INDI Driver.  If not, see
   < http : //www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>

#include <cmath>

#include "ambient_source.h"

// Values from a CloudWatcher (FW 5.89) at night: the IR sensor's own temperature
// read 10.65 °C while the RH sensor ("t!") read 11.16 °C.
static constexpr float IR_SENSOR_TEMP = 10.65f;
static constexpr float RH_SENSOR_TEMP = 11.16f;

TEST(AmbientSource, IRSensorSelectedKeepsTheIRValue)
{
    auto choice = AAG::selectAmbientTemperature(IR_SENSOR_TEMP, RH_SENSOR_TEMP, false);
    EXPECT_FLOAT_EQ(choice.temperature, IR_SENSOR_TEMP);
    EXPECT_FALSE(choice.fromRHSensor);
}

TEST(AmbientSource, RHSensorSelectedUsesTheRHValue)
{
    auto choice = AAG::selectAmbientTemperature(IR_SENSOR_TEMP, RH_SENSOR_TEMP, true);
    EXPECT_FLOAT_EQ(choice.temperature, RH_SENSOR_TEMP);
    EXPECT_TRUE(choice.fromRHSensor);
}

// CloudWatcherController::getTemperature() reports -1000 when the firmware is
// older than 5.6 (no RH sensor) or the reading is below -70 °C, and 1000 above 70 °C.
TEST(AmbientSource, BrokenOrMissingRHSensorFallsBackToIR)
{
    const float sentinels[] = {-1000.0f, 1000.0f};
    for (float sentinel : sentinels)
    {
        auto choice = AAG::selectAmbientTemperature(IR_SENSOR_TEMP, sentinel, true);
        EXPECT_FLOAT_EQ(choice.temperature, IR_SENSOR_TEMP) << sentinel;
        EXPECT_FALSE(choice.fromRHSensor) << sentinel;
    }
}

TEST(AmbientSource, NotANumberFallsBackToIR)
{
    auto choice = AAG::selectAmbientTemperature(IR_SENSOR_TEMP, std::nanf(""), true);
    EXPECT_FLOAT_EQ(choice.temperature, IR_SENSOR_TEMP);
    EXPECT_FALSE(choice.fromRHSensor);
}

// Lunatico, "RS232 Communication protocol for AAG_CloudWatcher" (part 4, 2019):
// a RH-sensor temperature outside [-40..80] °C marks the RH sensor invalid.
TEST(AmbientSource, ValidRangeFollowsTheProtocolDocument)
{
    EXPECT_TRUE(AAG::isValidRHSensorTemperature(-40.0f));
    EXPECT_TRUE(AAG::isValidRHSensorTemperature(80.0f));
    EXPECT_FALSE(AAG::isValidRHSensorTemperature(-40.1f));
    EXPECT_FALSE(AAG::isValidRHSensorTemperature(80.1f));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
