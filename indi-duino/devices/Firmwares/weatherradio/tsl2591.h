/*  Streaming functions for the TSL 2591 light sensor.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <Adafruit_TSL2591.h>
Adafruit_TSL2591 tsl = Adafruit_TSL2591();
struct {
  bool status;
  uint32_t full;
  uint16_t ir;
  uint16_t visible;
  int      gain;
  int      timing;
  float    lux;
} tsl2591Data {false, 0, 0, 0, 0, 0, 0.0};

/**
   tsl.begin() always returns true, hence we need to check the I2C adress
*/
bool isTSL2591Present() {
  Wire.beginTransmission(TSL2591_ADDR);
  byte error = Wire.endTransmission();

  return (error == 0);
}


void configureSensorTSL2591(tsl2591Gain_t gainSetting, tsl2591IntegrationTime_t timeSetting)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  tsl.setGain(gainSetting);

  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  tsl.setTiming(timeSetting);
}


// calibrate TSL2591 gain and integration time
bool calibrateTSL2591() {
  if (tsl2591Data.visible < 100) { //Increase GAIN (and INTEGRATIONTIME) if light level too low
    switch (tsl2591Data.gain)
    {
      case TSL2591_GAIN_LOW :
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MED :
        configureSensorTSL2591(TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_HIGH :
        configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MAX :
        switch (tsl2591Data.timing)
        {
          case TSL2591_INTEGRATIONTIME_200MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_300MS);
            break;
          case TSL2591_INTEGRATIONTIME_300MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_400MS);
            break;
          case TSL2591_INTEGRATIONTIME_400MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_500MS);
            break;
          case TSL2591_INTEGRATIONTIME_500MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS);
            break;
          case TSL2591_INTEGRATIONTIME_600MS :
            // no higher sensitivity level available
            return false;
            break;
          default:
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS);
            break;
        }
        break;
      default:
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
    }

    // calibration changed
    return true;
  }

  if (tsl2591Data.visible > 30000) { //Decrease GAIN (and INTEGRATIONTIME) if light level too high
    switch (tsl2591Data.gain)
    {
      case TSL2591_GAIN_LOW :
        switch (tsl2591Data.timing)
        {
          case TSL2591_INTEGRATIONTIME_500MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_400MS);
            break;
          case TSL2591_INTEGRATIONTIME_400MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_300MS);
            break;
          case TSL2591_INTEGRATIONTIME_300MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_200MS);
            break;
          case TSL2591_INTEGRATIONTIME_200MS :
            // no higher sensitivity level available
            return false;
            break;

          default:
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_200MS);
            break;
        }
        break;
      case TSL2591_GAIN_MED :
        configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_HIGH :
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MAX :
        configureSensorTSL2591(TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS);
        break;
      default:
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
    }
    // calibraton changed
    return true;
  }

  // no calibration change necessary
  return false;

}

void updateTSL2591() {
  if (tsl2591Data.status || (tsl2591Data.status = isTSL2591Present())) {
    tsl.begin();
    // Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
    tsl2591Data.full    = tsl.getFullLuminosity();
    tsl2591Data.ir      = tsl2591Data.full >> 16;
    tsl2591Data.visible = tsl2591Data.full & 0xFFFF;
    tsl2591Data.lux     = tsl.calculateLux(tsl2591Data.visible, tsl2591Data.ir);
    tsl2591Data.gain    = tsl.getGain();
    tsl2591Data.timing  = tsl.getTiming();

    bool changed = calibrateTSL2591();
    if (changed) updateTSL2591();
  }
}

void serializeTSL2591(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("TSL2591");
  data["init"] = tsl2591Data.status;

  if (tsl2591Data.status) {
    data["Lux"]     = tsl2591Data.lux;
    data["Visible"] = tsl2591Data.visible;
    data["IR"]      = tsl2591Data.ir;
    data["Gain"]    = tsl2591Data.gain;
    data["Timing"]  = tsl2591Data.timing;
  }
}

String displayTSL2591Parameters() {
  //if (tsl2591Data.status == false) return "";
  
  String result = " Lux: " + String(tsl2591Data.lux, 1) + "\n Visible: " + String(tsl2591Data.visible, DEC);
  result += "\n IR: " + String(tsl2591Data.ir, DEC) + "\n" + "\n Gain: " + String(tsl2591Data.gain, DEC);
  result += "\n Timing: " + String(tsl2591Data.timing, DEC) + "\n";
  return result;
}
