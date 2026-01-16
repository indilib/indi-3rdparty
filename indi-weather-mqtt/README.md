# indi-weather-mqtt
INDI driver for weather monitoring from an arbitrary MQTT source.
This driver is superior to any existing weather driver as it can be fed
with data from virtually any source through local or public MQTT broker.
If your weather station can publish to MQTT topics, you can use 
it right away by subscribing to the weather topics. Otherwise you can
parse ANY weather data source with a middleware (eg. node-red) and publish
it to a MQTT broker. Then you subscribe to the weather topics and enjoy!
