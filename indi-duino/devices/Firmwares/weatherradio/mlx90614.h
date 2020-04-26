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
} mlxData {false, 0.0, 0.0};

/**
   mlx.begin() always returns true, hence we need to check the I2C adress
*/
bool isMLX90614Present() {
  Wire.beginTransmission(MLX90614_I2CADDR);
  byte error = Wire.endTransmission();

  return (error == 0);
}

void updateMLX() {
  if (mlxData.status || (mlxData.status = isMLX90614Present())) {
    mlx.begin();
    mlxData.ambient_t = mlx.readAmbientTempC();
    mlxData.object_t  = mlx.readObjectTempC();
  }
}

void serializeMLX(JsonDocument & doc) {

  JsonObject data = doc.createNestedObject("MLX90614");
  data["init"] = mlxData.status;

  if (mlxData.status) {
    data["T amb"] = mlxData.ambient_t;
    data["T obj"] = mlxData.object_t;
  }
}
