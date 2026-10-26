/*
    ScopeLink INDI driver - which way the telescope tube is pointing

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
#include <optional>
#include <stdexcept>
#include <string>

namespace scopelink
{

/** @brief A reading the orientation reference cannot be taken from. */
class OrientationError : public std::runtime_error
{
    public:
        explicit OrientationError(const std::string &what) : std::runtime_error(what) {}
};

/**
 * @brief Which way the telescope tube is pointing, worked out from the sensor module's accelerometer.
 *
 * An accelerometer standing still measures one thing, which is which way is up. That gives two of the
 * three angles a tube can be turned through: how far the tube is raised above the horizon, and how far it
 * has been rolled about its own length. The third - which way round the compass it faces - turns the tube
 * about the very direction being measured, changes nothing the sensor can see, and is not reported.
 *
 * What the sensor's axes are to the tube is a matter of how the module was fitted. Until the user says
 * otherwise the module is taken to be fitted the way it is designed to be: Z along the optical axis,
 * pointing towards the sky end, and Y pointing at the ground when the tube is level. A reference replaces
 * that assumption with a measurement: the tube is pointed level, at the roll it is normally used at, and
 * what the accelerometer reads there is taken to be "level and not rolled" - see @ref reference. That
 * corrects a module fitted crooked or turned round the tube, which are the two ways a module fitted with
 * Z along the tube can still be fitted wrong.
 *
 * Worked out from the smoothed axes. A single sample of an accelerometer is the shake of the tube as well
 * as the pull of gravity; see ReadingFilter.
 *
 * A port of the Windows driver's TubeOrientation, constant for constant, so that the two drivers show the
 * same angles for the same reading and the same reference.
 */
class TubeOrientation
{
    public:
        /** Three axes, X first, in thousandths of g or as a unit vector. */
        using Vector = std::array<double, 3>;

        /**
         * The weakest reading taken as gravity at all, in thousandths of g. What reads less is a sensor in
         * free fall or one returning nonsense, and there is no direction in either.
         */
        static constexpr double LeastMilliG = 200.0;

        /** The weakest reading a reference is taken from, in thousandths of g. */
        static constexpr double ReferenceLeastMilliG = 850.0;

        /**
         * The strongest reading a reference is taken from, in thousandths of g. Fifteen percent either side
         * of one g: a tube being handled reads more or less than gravity, and a reference taken while it was
         * would be a level that is out by however hard it was being pushed - for every evening after it.
         */
        static constexpr double ReferenceMostMilliG = 1150.0;

        /**
         * How far from the horizontal the sensor's Z axis may lie while the tube is level, in degrees. A Z
         * axis standing near upright when the tube is level says the module is not fitted with Z along the
         * tube at all, and squaring it up would be inventing a direction rather than correcting one.
         */
        static constexpr double ReferenceMostZTilt = 30.0;

        /**
         * How close to straight up or straight down the tube may point before it has no roll, in degrees.
         * Rolling a tube that points at the zenith turns it about the direction the sensor is measuring, and
         * a roll worked out from what is left near there swings through whole turns while the tube stands
         * still.
         */
        static constexpr double RollLostAltitude = 85.0;

        /** @brief How far the tube is raised above the horizon, in degrees: 90 at the zenith, -90 straight down. */
        double altitude() const { return m_altitude; }

        /**
         * @brief How far the tube is rolled about its length, in degrees from -180 to 180.
         *
         * Positive turning clockwise about the tube as seen from behind it, looking out of the sky end.
         * Empty where the tube points too nearly straight up or down to have one.
         */
        std::optional<double> roll() const { return m_roll; }

        /** @brief True when this was worked out against a reference rather than the module's design axes. */
        bool isReferenced() const { return m_referenced; }

        /**
         * @brief Works out where the tube is pointing from one reading of the accelerometer.
         * @param acceleration The three axes, in thousandths of g
         * @param reference What the accelerometer read with the tube level, as a unit vector, or empty to
         *                  take the module's design axes. One that is not usable is treated as empty.
         * @return The orientation, or empty where the reading is too weak to have a direction
         */
        static std::optional<TubeOrientation> from(const Vector &acceleration, const std::optional<Vector> &reference);

        /**
         * @brief Takes one reading of the accelerometer as the tube standing level and not rolled.
         * @param acceleration The three axes, in thousandths of g
         * @return The reference: which way is up, as a unit vector in the sensor's axes
         * @throws OrientationError The reading cannot be a level tube standing still.
         */
        static Vector reference(const Vector &acceleration);

        /**
         * @brief Whether a stored reference can be used.
         * @return True when it is three finite numbers making a direction Z can be squared up against.
         *         All zeros - what the driver stores for "no reference" - is not.
         */
        static bool isUsable(const Vector &reference);

    private:
        TubeOrientation(double altitude, std::optional<double> roll, bool referenced)
            : m_altitude(altitude), m_roll(roll), m_referenced(referenced)
        {
        }

        double m_altitude{ 0.0 };
        std::optional<double> m_roll;
        bool m_referenced{ false };
};

} // namespace scopelink
