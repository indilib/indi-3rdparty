/*  Streaming functions for the TSL 2591 light sensor.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

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
  float    lux;
  int      gain;
  int      timing;
} tslData;

void configureSensorTSL(tsl2591Gain_t gainSetting, tsl2591IntegrationTime_t timeSetting)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  tsl.setGain(gainSetting);

  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  tsl.setTiming(timeSetting);
}


// calibrate TSL gain and integration time
void calibrateTSL() {
  if (tslData.full < 100) { //Increase GAIN (and INTEGRATIONTIME) if light level too low
    switch (tslData.gain)
    {
      case TSL2591_GAIN_LOW :
        configureSensorTSL(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MED :
        configureSensorTSL(TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_HIGH :
        configureSensorTSL(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MAX :
        if (tslData.full < 100) {
          switch (tslData.timing)
          {
            case TSL2591_INTEGRATIONTIME_200MS :
              configureSensorTSL(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_300MS);
              break;
            case TSL2591_INTEGRATIONTIME_300MS :
              configureSensorTSL(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_400MS);
              break;
            case TSL2591_INTEGRATIONTIME_400MS :
              configureSensorTSL(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_500MS);
              break;
            case TSL2591_INTEGRATIONTIME_500MS :
              configureSensorTSL(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS);
              break;
            default:
              configureSensorTSL(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS);
              break;
          }
        }
        break;
      default:
        configureSensorTSL(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
    }
  }

  if (tslData.full > 30000) { //Decrease GAIN (and INTEGRATIONTIME) if light level too high
    switch (tslData.gain)
    {
      case TSL2591_GAIN_LOW :
        break;
      case TSL2591_GAIN_MED :
        configureSensorTSL(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_HIGH :
        configureSensorTSL(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MAX :
        configureSensorTSL(TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS);
        break;
      default:
        configureSensorTSL(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
    }
  }

}

void updateTSL() {
  if (tslData.status || (tslData.status = tsl.begin())) {
    // Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
    tslData.full    = tsl.getFullLuminosity();
    tslData.ir      = tslData.full >> 16;
    tslData.visible = tslData.full & 0xFFFF;
    tslData.lux     = tsl.calculateLux(tslData.full, tslData.ir);
    tslData.gain    = tsl.getGain();
    tslData.timing  = tsl.getTiming();

    calibrateTSL();
  }
  else {
    tslData.status = false;
    Serial.println("TSL sensor initialization FAILED!");
  }
}

void serializeTSL(JsonDocument &doc) {

  JsonObject data   = doc.createNestedObject("TSL2591");
  data["Lux"]       = tslData.lux;
}
