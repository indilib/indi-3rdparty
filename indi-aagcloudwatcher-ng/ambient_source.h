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

#pragma once

#include <cmath>

namespace AAG
{

/**
 * Plausible range of the RH sensor's temperature ("t!" command). Lunatico,
 * "RS232 Communication protocol for AAG_CloudWatcher" (part 4, 2019): a value
 * outside [-40..80] °C marks the RH sensor invalid, for temperature and RH alike.
 */
static constexpr float RH_SENSOR_TEMPERATURE_MIN = -40.0f;
static constexpr float RH_SENSOR_TEMPERATURE_MAX = 80.0f;

inline bool isValidRHSensorTemperature(float temperature)
{
    return std::isfinite(temperature) && temperature >= RH_SENSOR_TEMPERATURE_MIN &&
           temperature <= RH_SENSOR_TEMPERATURE_MAX;
}

struct AmbientChoice
{
    float temperature;
    bool fromRHSensor;
};

/**
 * Pick the ambient temperature the driver works with.
 * @param otherSourceTemp ambient from the external NTC probe if fitted, else the IR sensor's own temperature
 * @param rhSensorTemp temperature read by the RH sensor (-1000 / 1000 when missing or broken)
 * @param preferRHSensor the user selected the RH sensor as the ambient source
 * @return the RH sensor's temperature when it is selected and valid, otherwise otherSourceTemp
 */
inline AmbientChoice selectAmbientTemperature(float otherSourceTemp, float rhSensorTemp, bool preferRHSensor)
{
    if (preferRHSensor && isValidRHSensorTemperature(rhSensorTemp))
        return {rhSensorTemp, true};

    return {otherSourceTemp, false};
}

} // namespace AAG
