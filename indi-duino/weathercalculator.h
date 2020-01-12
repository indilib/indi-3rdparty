/*
    Function definitions for weather stations.

    Cloud coverage calculation taken from the INDI driver for the AAG Cloud Watcher
    (Lunatico https://www.lunatico.es), implemented by Sergio Alonso (zerjioi@ugr.es)

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include <math.h>

class WeatherCalculator
{
public:
    //Clear sky corrected temperature (temp below means 0% clouds)
    #define CLOUD_TEMP_CLEAR  -8
    //Totally cover sky corrected temperature (temp above means 100% clouds)
    #define CLOUD_TEMP_OVERCAST  0

    /**
     * @brief Calculate the cloud coverage from the ambient and cloud temperature.
     *
     * Calculate the cloud coverage from the difference between ambient and sky temperature
     * The original formula stems from the AAG cloud watcher http://lunatico.es/aagcw/enhelp/
     * and the INDI driver indi_aagcloudwatcher.cpp.
     */
    static double cloudCoverage(double ambientTemperature, double skyTemperature)
    {
        double correctedTemperature = skyTemperatureCorr(ambientTemperature, skyTemperature);


        if (correctedTemperature < CLOUD_TEMP_CLEAR) correctedTemperature = CLOUD_TEMP_CLEAR;
        if (correctedTemperature > CLOUD_TEMP_OVERCAST) correctedTemperature = CLOUD_TEMP_OVERCAST;

        return (correctedTemperature - CLOUD_TEMP_CLEAR) * 100 / (CLOUD_TEMP_OVERCAST - CLOUD_TEMP_CLEAR);
    }


    /**
     * @brief Calculate the Sky quality SQM from the measured illuminance.
     */
    static double sqmValue(double lux) { return log10(lux/108000)/-0.4; }

    /**
     * @brief saturation vapour pressure based on the Magnus formula
     * ps(T) =  c * pow(10, (a*T)/(b+T))
     */
    static double saturationVapourPressure (double temperature) { return dp_c * pow(10, (dp_a*temperature)/(dp_b + temperature)); }

    /**
     * @brief vapour pressure: vapourPressure(r,T) = r/100 * saturationVapourPressure(T)
     * @param humidity 0 <= humidity <= 100
     * @param temperature in °C
     */
    static double vapourPressure (double humidity, double temperature) {return humidity * saturationVapourPressure(temperature) / 100;}

    /**
     * @brief Dew point
     * dewPoint(humidity, temperature) = b*v/(a-v) with v = log(vapourPressure(humidity, temperature)/c)
     *
     */
    static double dewPoint(double humidity, double temperature)
    {
        double v = log10(vapourPressure(humidity, temperature)/dp_c);
        return dp_b*v/(dp_a-v);
    }

    /**
     * @brief Normalize the sky temperature for the cloud coverage calculation.
     * The original formula stems from the AAG cloud watcher http://lunatico.es/aagcw/enhelp/
     * and the INDI driver indi_aagcloudwatcher.cpp.
     */
    static double skyTemperatureCorr(double ambientTemperature, double skyTemperature)
    {
        // FIXME: make the parameters k1 .. k5 configurable
        double k1 = 33.0, k2 = 0.0,  k3 = 4.0, k4 = 100.0, k5 = 100.0;

        double correctedTemperature =
            skyTemperature - ((k1 / 100.0) * (ambientTemperature - k2 / 10.0) +
                              (k3 / 100.0) * pow(exp(k4 / 1000. * ambientTemperature), (k5 / 100.0)));

        return correctedTemperature;
    }
private:
    WeatherCalculator() {}

    /**
     * @brief Magnus formula constants for values of -45°C < T < 60°C over water.
     */
    static constexpr double dp_a = 7.5, dp_b = 237.5, dp_c = 6.1078;
};
