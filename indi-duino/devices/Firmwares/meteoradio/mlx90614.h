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
  data["T amb"] = mlxData.ambient_t;
  data["T obj"] = mlxData.object_t;
}
