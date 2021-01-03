/*  Functions for rain sensor such as  
    https://create.arduino.cc/projecthub/MisterBotBreak/how-to-use-a-rain-sensor-bcecd9

    Resistance of sensor is reported from the analog pin and normalised as rainData.wetness
    0 is dryest 1 is wettest
*/

struct {
  bool status;
  float wetness;
} rainData = {false, 0.0};

void updaterain() {
  if (rainData.status == false) {
    pinMode(RAIN_PIN, INPUT);
    rainData.status = true;
  }

  rainData.wetness = 1.0 - float(analogRead(RAIN_PIN))/1023;
}

void serializerain(JsonDocument &doc) {
  JsonObject data = doc.createNestedObject("Rain");
  data["init"] = rainData.status;

  if (rainData.status) {
    data["wetness"] = rainData.wetness;
  }
}
