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

#include "scopelink/orientation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace scopelink
{

namespace
{

constexpr double Pi = 3.14159265358979323846;

using Vector = TubeOrientation::Vector;

double along(const Vector &a, const Vector &b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

double degrees(double radians)
{
    return radians * 180.0 / Pi;
}

/**
 * @brief The tube's own axes, written in the sensor's.
 * @param reference The reference, or empty
 * @param referenced Set to true where the reference was used
 * @return Across the tube, down at no roll, and along it towards the sky, in that order
 */
std::array<Vector, 3> axesOf(const std::optional<Vector> &reference, bool &referenced)
{
    referenced = reference.has_value() && TubeOrientation::isUsable(*reference);

    if (!referenced)
        return { Vector{ 1.0, 0.0, 0.0 }, Vector{ 0.0, 1.0, 0.0 }, Vector{ 0.0, 0.0, 1.0 } };

    const Vector &r     = *reference;
    const double length = std::sqrt(along(r, r));
    const Vector down   = { -r[0] / length, -r[1] / length, -r[2] / length };

    // Along the tube is the sensor's Z with whatever part of it points up or down taken out, which is what
    // a module fitted a few degrees crooked needs: its Z still runs along the tube, near enough, and the
    // reference says which part of it was the crookedness.
    const double lift  = down[2];
    Vector ahead       = { -lift * down[0], -lift * down[1], 1.0 - (lift * down[2]) };
    const double reach = std::sqrt(along(ahead, ahead));

    ahead = { ahead[0] / reach, ahead[1] / reach, ahead[2] / reach };

    const Vector across = { (down[1] * ahead[2]) - (down[2] * ahead[1]), (down[2] * ahead[0]) - (down[0] * ahead[2]),
                            (down[0] * ahead[1]) - (down[1] * ahead[0]) };

    return { across, down, ahead };
}

} // namespace

std::optional<TubeOrientation> TubeOrientation::from(const Vector &acceleration, const std::optional<Vector> &reference)
{
    const double length = std::sqrt(along(acceleration, acceleration));

    if (length < LeastMilliG)
        return std::nullopt;

    const Vector up = { acceleration[0] / length, acceleration[1] / length, acceleration[2] / length };

    bool referenced                  = false;
    const std::array<Vector, 3> axes = axesOf(reference, referenced);

    const double across = along(up, axes[0]);
    const double down   = along(up, axes[1]);
    const double ahead  = along(up, axes[2]);

    const double altitude = degrees(std::atan2(ahead, std::sqrt((across * across) + (down * down))));

    // Up seen down the tube. At no roll it is straight away from the module's Y, and a tube turned
    // clockwise carries its Y round with it, which leaves up to the anticlockwise side of where it was.
    std::optional<double> roll;

    if (std::abs(altitude) <= RollLostAltitude)
        roll = degrees(std::atan2(-across, -down));

    return TubeOrientation(altitude, roll, referenced);
}

Vector TubeOrientation::reference(const Vector &acceleration)
{
    const double length = std::sqrt(along(acceleration, acceleration));
    char message[320];

    if ((length < ReferenceLeastMilliG) || (length > ReferenceMostMilliG))
    {
        snprintf(message, sizeof(message),
                 "The accelerometer is reading %.2f g rather than the pull of gravity alone, so the telescope is "
                 "moving or being handled. Hold it still and set the reference again.",
                 length / 1000.0);

        throw OrientationError(message);
    }

    const Vector up   = { acceleration[0] / length, acceleration[1] / length, acceleration[2] / length };
    const double tilt = degrees(std::asin(std::min(1.0, std::abs(up[2]))));

    if (tilt > ReferenceMostZTilt)
    {
        snprintf(message, sizeof(message),
                 "With the telescope level the sensor's Z axis should lie along the tube, but it is %.0f degrees "
                 "from the horizontal. Point the telescope level and check that the sensor module is fitted with "
                 "its Z axis along the optical axis.",
                 tilt);

        throw OrientationError(message);
    }

    return up;
}

bool TubeOrientation::isUsable(const Vector &reference)
{
    for (double each : reference)
    {
        if (!std::isfinite(each))
            return false;
    }

    const double length = std::sqrt(along(reference, reference));

    return (length > 0.5) && (std::abs(reference[2] / length) < std::sin(ReferenceMostZTilt * Pi / 180.0) + 1e-9);
}

} // namespace scopelink
