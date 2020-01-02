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
  data["Temp"] = bmeData.temperature;
  data["Pres"] = bmeData.pressure;
  data["Hum"] = bmeData.humidity;
}
