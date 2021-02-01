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

    WeatherCalculator() = default;
    ~WeatherCalculator() = default;

    /**
     * @brief Calculate the cloud coverage from the ambient and cloud temperature.
     *
     * Calculate the cloud coverage from the difference between ambient and sky temperature
     * The original formula stems from the AAG cloud watcher http://lunatico.es/aagcw/enhelp/
     * and the INDI driver indi_aagcloudwatcher.cpp.
     */
    double cloudCoverage(double ambientTemperature, double skyTemperature)
    {
        double correctedTemperature = skyTemperatureCorr(ambientTemperature, skyTemperature);

        if (correctedTemperature < skyTemperatureCoefficients.t_clear) correctedTemperature = skyTemperatureCoefficients.t_clear;
        if (correctedTemperature > skyTemperatureCoefficients.t_overcast) correctedTemperature = skyTemperatureCoefficients.t_overcast;

        return (correctedTemperature - skyTemperatureCoefficients.t_clear) * 100 / (skyTemperatureCoefficients.t_overcast - skyTemperatureCoefficients.t_clear);
    }



    /**
     * @brief Calculate the Sky quality SQM from the measured illuminance.
     */
    double sqmValue(double lux) { return log10(lux/108000)/-0.4; }

    /**
     * @brief saturation vapour pressure based on the Magnus formula
     * ps(T) =  c * pow(10, (a*T)/(b+T))
     */
    double saturationVapourPressure (double temperature) { return dp_c * pow(10, (dp_a*temperature)/(dp_b + temperature)); }

    /**
     * @brief vapour pressure: vapourPressure(r,T) = r/100 * saturationVapourPressure(T)
     * @param humidity 0 <= humidity <= 100
     * @param temperature in °C
     */
    double vapourPressure (double humidity, double temperature) {return humidity * saturationVapourPressure(temperature) / 100;}

    /**
     * @brief Dew point
     * dewPoint(humidity, temperature) = b*v/(a-v) with v = log(vapourPressure(humidity, temperature)/c)
     *
     */
     double dewPoint(double humidity, double temperature)
     {
         double v = log10(vapourPressure(humidity, temperature)/dp_c);
         return dp_b*v/(dp_a-v);
     }

    /**
     * @brief Normalize the sky temperature for the cloud coverage calculation.
     * The original formula stems from the AAG cloud watcher http://lunatico.es/aagcw/enhelp/
     * and the INDI driver indi_aagcloudwatcher.cpp.
     */
    double skyTemperatureCorr(double ambientTemperature, double skyTemperature)
    {
        double correctedTemperature =
                skyTemperature - ((skyTemperatureCoefficients.k1 / 100.0) * (ambientTemperature - skyTemperatureCoefficients.k2 / 10.0) +
                                  (skyTemperatureCoefficients.k3 / 100.0) * pow(exp(skyTemperatureCoefficients.k4 / 1000. * ambientTemperature),
                                                                                (skyTemperatureCoefficients.k5 / 100.0)));

        return correctedTemperature;
    }

    // Calibration coefficients for cloud coverage calculation
    struct {
        double k1 = 33.0, k2 = 0.0,  k3 = 4.0, k4 = 100.0, k5 = 100.0;
        //Clear sky corrected temperature (temp below means 0% clouds)
        //Totally cover sky corrected temperature (temp above means 100% clouds)
        double t_clear = -8, t_overcast = 0;
    } skyTemperatureCoefficients;


    /**
     * @brief Calculate the sealevel equivalent pressure, based on the barometric height formula.
     * @param absolutePressure measured air pressure
     * @param elevation elevation of the location
     * @param temp temperature when the air pressure has been measured
     */
    double sealevelPressure(double absolutePressure, double elevation, double temp)
    {
        return absolutePressure / pow(1-temp_gradient*elevation/(temp+elevation*temp_gradient+273.15), 0.03416/temp_gradient);
    }


    double windDirectionOffset = 0; // default value

    /**
     * @brief Correct the wind direction with a defined offset
     * @param direction
     * @return
     */
    double calibratedWindDirection(double direction)
    {
        double result = direction + windDirectionOffset;

        // ensure 0 <= result < 360
        if (result < 0)
            result += 360;
        if (result >= 360)
            result -= 360;

        return result;
    }

    struct linearCalibration {
        double factor = 1.0, shift = 0;
    } humidityCalibration, temperatureCalibration, sqmCalibration, wetnessCalibration;

    double calibrate (linearCalibration calibration, double value)
    {return calibration.factor * value + calibration.shift;}


private:

    /**
     * @brief Magnus formula constants for values of -45°C < T < 60°C over water.
     */
    static constexpr double dp_a = 7.5, dp_b = 237.5, dp_c = 6.1078;

    /**
     * @brief Temperature gradient for sealevel pressure calculation
     */
    const double temp_gradient = 0.0065;


};
