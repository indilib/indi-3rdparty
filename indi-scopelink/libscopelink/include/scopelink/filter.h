/*
    ScopeLink INDI driver - smoothing of the sensor module's readings

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include <array>
#include <chrono>

namespace scopelink
{

/**
 * @brief Smooths the sensor module's humidity, dew point and acceleration.
 *
 * The orientation is worked out from the acceleration, and an accelerometer read twice a second reports
 * the shake of a tube somebody has just touched as well as which way is up: unsmoothed, the altitude and
 * roll tremble by a degree or more on a tube standing still. Humidity and the dew point worked out from it
 * flicker in their last digit for the same reason, and the distance of the mirror from the dew point is
 * drawn between the two.
 *
 * The temperatures are not smoothed here. The status frame carries them already filtered by the firmware
 * the way the fans see them - see the temperature.ambient_filter and temperature.mirror_filter parameters -
 * and this driver has always published them as they arrive.
 *
 * A port of the Windows driver's ReadingFilter for these three, with the same constants, so that both
 * drivers show the same numbers. A time constant rather than a weight: what is written below is a time to
 * settle, and the weight each sample is worth is worked out from the gap since the last one, so the same
 * signal comes out the same whatever the polling period is set to. A real step is not waited out; a jump
 * larger than the step size, seen on two samples in a row, is taken at once, and a lone one is ignored.
 *
 * One filter per connection, reset when the link is opened. Not thread safe: the driver reads the
 * controller from its own timer and nowhere else.
 */
class ReadingFilter
{
    public:
        using Clock = std::chrono::steady_clock;

        /**
         * How long the humidity and the dew point take to follow a change, in seconds. The same fifteen
         * seconds the Windows driver gives the temperatures, because it is the same air being measured by
         * the same module.
         */
        static constexpr double ClimateSettleSeconds = 15.0;

        /** How far the dew point must move in one sample to be believed rather than smoothed, in Kelvin. */
        static constexpr double DewPointStepKelvin = 2.0;

        /**
         * How far the relative humidity must move in one sample to be believed rather than smoothed, in
         * percent. Five percent in one sample is a door opened onto the night rather than anything the
         * sensor does on its own.
         */
        static constexpr double HumidityStepPercent = 5.0;

        /**
         * How long the acceleration takes to follow a change, in seconds. A tube is moved by a mount at a
         * few degrees a second at most, so a second of lag keeps up with a slew.
         */
        static constexpr double AccelerationSettleSeconds = 1.0;

        /**
         * How far an axis must move in one sample to be believed rather than smoothed, in thousandths of g.
         * A quarter of a g is about fifteen degrees of tilt in one poll, which no mount does. What does it
         * is a tube being knocked or a module being refitted, and the second of those is worth showing at
         * once.
         */
        static constexpr double AccelerationStepMilliG = 250.0;

        /** How many samples in a row must agree before a step is believed. */
        static constexpr int SamplesToBelieveAStep = 2;

        /**
         * Gap after which a track starts again rather than carrying on, in seconds. A reading unavailable
         * for this long cannot be continued from: the world has had time to move while nobody measured it.
         */
        static constexpr double LongestGapSeconds = 5.0;

        /** @brief Forgets everything, so that the next reading of each kind is taken as it stands. */
        void reset();

        /** @brief Smooths one relative humidity reading, in percent. */
        double humidity(Clock::time_point moment, double percent);

        /** @brief Smooths one dew point reading, in degrees Celsius. */
        double dewPoint(Clock::time_point moment, double celsius);

        /**
         * @brief Smooths one reading of all three acceleration axes, in thousandths of g.
         *
         * The axes together, because what is made of them is a direction: three axes taken a sample apart
         * from each other would be a vector that was never measured.
         */
        std::array<double, 3> acceleration(Clock::time_point moment, const std::array<double, 3> &axes);

    private:
        /** @brief One reading followed over time. */
        class Track
        {
            public:
                Track(double settleSeconds, double step) : m_settleSeconds(settleSeconds), m_step(step) {}

                void reset();
                double apply(Clock::time_point moment, double reading);

            private:
                double start(Clock::time_point moment, double reading);

                double m_settleSeconds;
                double m_step;

                bool m_holding{ false };
                double m_held{ 0.0 };
                Clock::time_point m_at;
                int m_stepping{ 0 };
                bool m_steppingUp{ false };
        };

        Track m_humidity{ ClimateSettleSeconds, HumidityStepPercent };
        Track m_dewPoint{ ClimateSettleSeconds, DewPointStepKelvin };
        std::array<Track, 3> m_acceleration{ Track(AccelerationSettleSeconds, AccelerationStepMilliG),
                                             Track(AccelerationSettleSeconds, AccelerationStepMilliG),
                                             Track(AccelerationSettleSeconds, AccelerationStepMilliG) };
};

} // namespace scopelink
