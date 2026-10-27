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

#include "scopelink/filter.h"

#include <cmath>

namespace scopelink
{

void ReadingFilter::reset()
{
    m_humidity.reset();
    m_dewPoint.reset();

    for (Track &axis : m_acceleration)
        axis.reset();
}

double ReadingFilter::humidity(Clock::time_point moment, double percent)
{
    return m_humidity.apply(moment, percent);
}

double ReadingFilter::dewPoint(Clock::time_point moment, double celsius)
{
    return m_dewPoint.apply(moment, celsius);
}

std::array<double, 3> ReadingFilter::acceleration(Clock::time_point moment, const std::array<double, 3> &axes)
{
    return { m_acceleration[0].apply(moment, axes[0]), m_acceleration[1].apply(moment, axes[1]),
             m_acceleration[2].apply(moment, axes[2]) };
}

void ReadingFilter::Track::reset()
{
    m_holding  = false;
    m_stepping = 0;
}

double ReadingFilter::Track::apply(Clock::time_point moment, double reading)
{
    if (!m_holding)
        return start(moment, reading);

    double gap = std::chrono::duration<double>(moment - m_at).count();

    if (gap >= LongestGapSeconds)
        return start(moment, reading);

    // A sample older than the last one is worth no weight - a negative interval through an exponential is a
    // weight above one, which would overshoot a reading rather than approach it - and the clock is not
    // wound back to it either.
    if (gap < 0.0)
        gap = 0.0;
    else
        m_at = moment;

    const bool up = reading > m_held;

    if (std::abs(reading - m_held) > m_step)
    {
        // A step believed on one sample is a filter that passes every spike straight through. Two samples
        // the same side of the line, and a lone one is ignored completely rather than averaged in.
        m_stepping   = ((m_stepping > 0) && (m_steppingUp == up)) ? (m_stepping + 1) : 1;
        m_steppingUp = up;

        if (m_stepping < SamplesToBelieveAStep)
            return m_held;

        m_stepping = 0;
        m_held     = reading;

        return m_held;
    }

    m_stepping = 0;

    // What one sample is worth, given how long it has been since the last one: nothing at no gap at all,
    // roughly two thirds at a gap of one time constant, and very nearly the whole answer after several.
    m_held += (reading - m_held) * (1.0 - std::exp(-gap / m_settleSeconds));

    return m_held;
}

double ReadingFilter::Track::start(Clock::time_point moment, double reading)
{
    m_holding  = true;
    m_held     = reading;
    m_at       = moment;
    m_stepping = 0;

    return m_held;
}

} // namespace scopelink
