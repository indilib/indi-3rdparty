/*  Streaming functions for the Melexis IR sensor MLX90614.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <Adafruit_MLX90614.h>
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
struct {
  bool status;
  float ambient_t;
  float object_t;
} mlxData;

void updateMLX() {
  if (mlxData.status || (mlxData.status = mlx.begin())) {
    mlxData.ambient_t = mlx.readAmbientTempC();
    mlxData.object_t  = mlx.readObjectTempC();
  }
  else {
    mlxData.status = false;
    Serial.println("MLX sensor initialization FAILED!");
  }
}

void serializeMLX(JsonDocument & doc) {

  JsonObject data = doc.createNestedObject("MLX90614");
  data["T amb"] = mlxData.ambient_t;
  data["T obj"] = mlxData.object_t;
}
