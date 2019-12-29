/*  Firmware for a weather sensor device streaming the data as JSON documents.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include <ArduinoJson.h>
// #include <MemoryFree.h> // only necessary for debugging


#define USE_BME_SENSOR            //USE BME280 ENVIRONMENT SENSOR. Comment if not.
#define USE_MLX_SENSOR            //USE MELEXIS IR SENSOR. Comment if not.
#define USE_TSL_SENSOR            //USE TSL2591 SENSOR. Comment if not.

#ifdef USE_TSL_SENSOR
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
  data["Lum_ir"]    = tslData.ir;
  data["Lum_vis"]   = tslData.visible;
  data["Lux"]       = tslData.lux;
  data["Gain"]      = tslData.gain;
  data["Timing"]    = tslData.timing;
}
#endif //USE_TSL_SENSOR

#ifdef USE_BME_SENSOR
#include "Adafruit_BME280.h"
Adafruit_BME280 bme;
struct {
  bool status;
  float temperature;
  float pressure;
  float humidity;
} bmeData;


void updateBME() {
  if (bmeData.status || (bmeData.status = bme.begin())) {
    bmeData.temperature = bme.readTemperature();
    bmeData.pressure    = bme.readPressure() / 100.0;
    bmeData.humidity    = bme.readHumidity();
  }
  else {
    bmeData.status = false;
    Serial.println("BME sensor initialization FAILED!");
  }
}

void serializeBME(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("BME280");
  data["T"] = bmeData.temperature;
  data["P"] = bmeData.pressure;
  data["H"] = bmeData.humidity;
}
#endif //USE_BME_SENSOR

#ifdef USE_MLX_SENSOR
#include <Adafruit_MLX90614.h>
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
struct {
  bool status;
  float ambient_t;
  float object_t;
} mlxData;

void updateMLX() {
  if (mlxData.status || (mlxData.status = mlx.begin())) {
    mlxData.ambient_t = mlx.readAmbientTempC() / 100.0;
    mlxData.object_t  = mlx.readObjectTempC() / 100.0;
  }
  else {
    mlxData.status = false;
    Serial.println("MLX sensor initialization FAILED!");
  }
}

void serializeMLX(JsonDocument & doc) {

  JsonObject data = doc.createNestedObject("MLX90614");
  data["Ta"] = mlxData.ambient_t;
  data["To"] = mlxData.object_t;
}
#endif //USE_MLX_SENSOR

void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

}

void loop() {

  StaticJsonDocument < JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) > weatherDoc;

#ifdef USE_BME_SENSOR
  updateBME();
  serializeBME(weatherDoc);
#endif //USE_BME_SENSOR

#ifdef USE_MLX_SENSOR
  updateMLX();
  serializeMLX(weatherDoc);
#endif //USE_MLX_SENSOR

#ifdef USE_TSL_SENSOR
  updateTSL();
  serializeTSL(weatherDoc);
#endif //USE_TSL_SENSOR

  serializeJson(weatherDoc, Serial);
  Serial.println();

  delay(1000);
  //Serial.print("free Memory="); Serial.println(freeMemory());
}
