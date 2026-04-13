/*******************************************************************************
 Copyright(c) 2021 Radek Kaczorek  <rkaczorek AT gmail DOT com>

 INDI MQTT Weather Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indi-weather-mqtt.h"

#include <memory>
#include <cstring>
#include <unistd.h>
#include <functional>
#include "config.h"

#define MQTT_POLL (200) // 0.2 sec

std::unique_ptr<WeatherMQTT> weatherMQTT(new WeatherMQTT());

WeatherMQTT::WeatherMQTT()
{
    setVersion(VERSION_MAJOR,VERSION_MINOR);
    setWeatherConnection(CONNECTION_NONE);

    mosquitto_lib_init();
    snprintf(mqtt_clientid, 31, "indi-weather-mqtt-%d", getpid());
    mosq = mosquitto_new(mqtt_clientid, true, this);
    mosquitto_message_callback_set(mosq, mqttMsgCallback);
}

WeatherMQTT::~WeatherMQTT()
{
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
}

const char *WeatherMQTT::getDefaultName()
{
    return (const char *)"MQTT Weather";
}

bool WeatherMQTT::Connect()
{
	// connect to mqtt broker
	if (mosq)
	{
		DEBUGF(INDI::Logger::DBG_DEBUG, "Connecting to MQTT broker (mqtt_host=%s, mqtt_port=%s, mqtt_user=%s, mqtt_pass=%s)", MqttServerT[0].text, MqttServerT[1].text, MqttServerT[2].text, MqttServerT[3].text);

		// set mqtt username and password
		mosquitto_username_pw_set(mosq, MqttServerT[2].text, MqttServerT[3].text);

		// connect to broker
		int rc = mosquitto_connect(mosq, MqttServerT[0].text, atoi(MqttServerT[1].text), 60);
		DEBUGF(INDI::Logger::DBG_DEBUG, "MQTT broker connection status: %d", rc);

		if ( rc == 0)
		{
			DEBUG(INDI::Logger::DBG_SESSION, "MQTT Weather connected successfully.");
			// subscribe topics
			mqttSubscribe();
			// set mqtt loop timer
			MqttLoopTimerID = IEAddTimer(MQTT_POLL, MqttLoopHelper, this);
			return true;
		}  else {
			DEBUG(INDI::Logger::DBG_SESSION, "Error connecting to MQTT broker. Check MQTT Server parameters in Options.");
			return false;
		}
	}
	return true;
}

bool WeatherMQTT::Disconnect()
{
	// disconnect from mqtt broker
	mosquitto_disconnect(mosq);
    return true;
}

bool WeatherMQTT::initProperties()
{
    INDI::Weather::initProperties();

	// MQTT server
	IUFillText(&MqttServerT[0], "MQTT_HOST", "Host", "");
	IUFillText(&MqttServerT[1], "MQTT_PORT", "Port", "1883");
	IUFillText(&MqttServerT[2], "MQTT_USER", "User", "");
	IUFillText(&MqttServerT[3], "MQTT_PASS", "Pass", "");
	IUFillTextVector(&MqttServerTP, MqttServerT, 4, getDeviceName(), "MQTT_SERVER", "MQTT Server", OPTIONS_TAB,IP_RW, 0, IPS_IDLE);	

	// MQTT topics
	IUFillText(&MqttTopicsT[0], "MQTT_TEMPERATURE", "Temperature", "");
	IUFillText(&MqttTopicsT[1], "MQTT_HUMIDITY", "Humidity", "");
	IUFillText(&MqttTopicsT[2], "MQTT_PRESSURE", "Pressure", "");
	IUFillText(&MqttTopicsT[3], "MQTT_WIND", "Wind", "");
	IUFillText(&MqttTopicsT[4], "MQTT_GUST", "Gust", "");
	IUFillText(&MqttTopicsT[5], "MQTT_RAIN", "Rain", "");
	IUFillText(&MqttTopicsT[6], "MQTT_CLOUDS", "Clouds", "");
	IUFillText(&MqttTopicsT[7], "MQTT_LIGHT", "Light", "");
	IUFillTextVector(&MqttTopicsTP, MqttTopicsT, 8, getDeviceName(), "MQTT_TOPICS", "MQTT Topics", OPTIONS_TAB,IP_RW, 0, IPS_IDLE);

	// add weather parameters
    addParameter("WEATHER_FORECAST", "Weather", 0, 1, 15);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 100, 15);
    addParameter("WEATHER_PRESSURE", "Pressure (hPa)", 900, 1100, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind Speed (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_GUST", "Wind Gust (kph)", 0, 20, 15);
    addParameter("WEATHER_RAINFALL", "Rain (mm)", 0, 0, 15);
    addParameter("WEATHER_CLOUDS", "Clouds (%)", 0, 100, 15);
    addParameter("WEATHER_LIGHT", "Light (mag/arcsec^2)", 0, 22, 15);

	// set default values
    setParameterValue("WEATHER_FORECAST", 0);
    setParameterValue("WEATHER_TEMPERATURE", 0);
    setParameterValue("WEATHER_HUMIDITY", 0);
    setParameterValue("WEATHER_PRESSURE", 960);
    setParameterValue("WEATHER_WIND_SPEED", 0);
    setParameterValue("WEATHER_WIND_GUST", 0);
    setParameterValue("WEATHER_RAINFALL", 0);
    setParameterValue("WEATHER_CLOUDS", 0);
    setParameterValue("WEATHER_LIGHT", 18);

	// set critical weather parameters
    setCriticalParameter("WEATHER_FORECAST");
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_HUMIDITY");
    //setCriticalParameter("WEATHER_PRESSURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    //setCriticalParameter("WEATHER_WIND_GUST");
    setCriticalParameter("WEATHER_RAINFALL");
    setCriticalParameter("WEATHER_CLOUDS");
    setCriticalParameter("WEATHER_LIGHT");

	// we need this before connecting to mqtt broker
	defineProperty(&MqttServerTP);
	defineProperty(&MqttTopicsTP);

	// load saved config
	loadConfig(false, "MQTT_SERVER");
	loadConfig(false, "MQTT_TOPICS");

    //addDebugControl();

    return true;
}

bool WeatherMQTT::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
		// we don't need these properties
		deleteProperty(RefreshSP);
		deleteProperty(UpdatePeriodNP);
    } else {
		// nop
	}

    return true;
}

IPState WeatherMQTT::updateWeather()
{
	return IPS_OK;
}

bool WeatherMQTT::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// first we check if it's for our device
	if (!strcmp(dev, getDeviceName()))
	{
		// handle mqtt server
		if (!strcmp(name, MqttServerTP.name))
		{
			IUUpdateText(&MqttServerTP,texts,names,n);
			MqttServerTP.s=IPS_OK;
			IDSetText(&MqttServerTP, nullptr);
			DEBUG(INDI::Logger::DBG_SESSION, "MQTT broker parameters set.");
			return true;
		}

		// handle mqtt topics
		if (!strcmp(name, MqttTopicsTP.name))
		{
			// unsubscribe old topics
			MqttTopicsTP.s=IPS_BUSY;
			IDSetText(&MqttTopicsTP, nullptr);
			mqttUnSubscribe();

			IUUpdateText(&MqttTopicsTP,texts,names,n);
			MqttTopicsTP.s=IPS_OK;
			IDSetText(&MqttTopicsTP, nullptr);
			DEBUG(INDI::Logger::DBG_SESSION, "MQTT weather topics set.");

			// subscribe new topics
			mqttSubscribe();
			return true;
		}
	}

	return INDI::Weather::ISNewText(dev,name,texts,names,n);
}

bool WeatherMQTT::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

	IUSaveConfigText(fp, &MqttServerTP);
	IUSaveConfigText(fp, &MqttTopicsTP);

    return true;
}

void WeatherMQTT::MqttLoopHelper(void *context)
{
	static_cast<WeatherMQTT*>(context)->MqttLoop();
}

void WeatherMQTT::MqttLoop()
{
	// do nothing if not connected
	if (!isConnected())
		return;

	// mqtt ping
	DEBUG(INDI::Logger::DBG_DEBUG, "MQTT ping.");

	// keep alive and reconnect if neccessary
	if (mosquitto_loop(mosq, -1, 1))
	{
		DEBUG(INDI::Logger::DBG_SESSION, "Connection to MQTT broker lost. Reconnecting.");
		mosquitto_reconnect(mosq);
		if (mosq)
		{
			DEBUG(INDI::Logger::DBG_SESSION, "Successfully connected to MQTT broker.");
			mqttSubscribe();
		}
	}

	// restart timer
	MqttLoopTimerID = IEAddTimer(MQTT_POLL, MqttLoopHelper, this);
}

void WeatherMQTT::mqttSubscribe()
{
	DEBUG(INDI::Logger::DBG_DEBUG, "Subscribing to MQTT topics.");

	// calculate number of topics
	int topics = *(&MqttTopicsT + 1) - MqttTopicsT;

	// subscribe to each topic
	for (int i=0; i < topics; i++)
	{
		if (MqttTopicsT[i].text != NULL && MqttTopicsT[i].text[0] != '\0')
		{
			if (!mosquitto_subscribe(mosq, NULL, MqttTopicsT[i].text, 0))
			{
				DEBUGF(INDI::Logger::DBG_DEBUG, "Subscribed to %s", MqttTopicsT[i].text);
			} else {
				DEBUGF(INDI::Logger::DBG_DEBUG, "Error subscribing to %s", MqttTopicsT[i].text);
			}
		}
	}
}

void WeatherMQTT::mqttUnSubscribe()
{
	DEBUG(INDI::Logger::DBG_DEBUG, "Unsubscribing MQTT topics.");

	// calculate number of topics
	int topics = *(&MqttTopicsT + 1) - MqttTopicsT;

	// unsubscribe each topic
	for (int i=0; i < topics; i++)
	{
		if (MqttTopicsT[i].text != NULL && MqttTopicsT[i].text[0] != '\0')
		{
			if (!mosquitto_unsubscribe(mosq, NULL, MqttTopicsT[i].text))
			{
				DEBUGF(INDI::Logger::DBG_DEBUG, "Unsubscribed %s", MqttTopicsT[i].text);
			} else {
				DEBUGF(INDI::Logger::DBG_DEBUG, "Error unsubscribing %s", MqttTopicsT[i].text);
			}
		}
	}
}

void WeatherMQTT::mqttMsgCallback(struct mosquitto *, void *obj, const struct mosquitto_message *message)
{
	static_cast<WeatherMQTT*>(obj)->mqttMsg((char*) message->topic, (char*) message->payload);
}

void WeatherMQTT::mqttMsg(char* topic, char* msg)
{
	DEBUGF(INDI::Logger::DBG_DEBUG, "Received MQTT message '%s for topic %s", topic, msg);

	if (MqttTopicsT[0].text != NULL && !strcmp(MqttTopicsT[0].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Temperature received.");
		setParameterValue("WEATHER_TEMPERATURE", atof(msg));
	}

	if (MqttTopicsT[1].text != NULL && !strcmp(MqttTopicsT[1].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Humidity received.");
		setParameterValue("WEATHER_HUMIDITY", atof(msg));
	}

	if (MqttTopicsT[2].text != NULL && !strcmp(MqttTopicsT[2].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Pressure received.");
		setParameterValue("WEATHER_PRESSURE", atof(msg));
	}

	if (MqttTopicsT[3].text != NULL && !strcmp(MqttTopicsT[3].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Wind received.");
		setParameterValue("WEATHER_WIND_SPEED", atof(msg));
	}

	if (MqttTopicsT[4].text != NULL && !strcmp(MqttTopicsT[4].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Wind gust received.");
		setParameterValue("WEATHER_WIND_GUST", atof(msg));
	}

	if (MqttTopicsT[5].text != NULL && !strcmp(MqttTopicsT[5].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Rain received.");
		setParameterValue("WEATHER_RAINFALL", atof(msg));
	}

	if (MqttTopicsT[6].text != NULL && !strcmp(MqttTopicsT[6].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Clouds received.");
		setParameterValue("WEATHER_CLOUDS", atof(msg));
	}

	if (MqttTopicsT[7].text != NULL && !strcmp(MqttTopicsT[7].text, topic)) {
		DEBUG(INDI::Logger::DBG_DEBUG, "Light received.");
		setParameterValue("WEATHER_LIGHT", atof(msg));
	}

	// clear
	if (checkParameterState("WEATHER_TEMPERATURE") == IPS_OK && checkParameterState("WEATHER_HUMIDITY") == IPS_OK && checkParameterState("WEATHER_WIND_SPEED") == IPS_OK && checkParameterState("WEATHER_RAINFALL") == IPS_OK && checkParameterState("WEATHER_CLOUDS") == IPS_OK && checkParameterState("WEATHER_LIGHT") == IPS_OK)
	{
		setParameterValue("WEATHER_FORECAST", 0);
	}

	// warning zone
	if (checkParameterState("WEATHER_TEMPERATURE") == IPS_BUSY || checkParameterState("WEATHER_HUMIDITY") == IPS_BUSY || checkParameterState("WEATHER_WIND_SPEED") == IPS_BUSY || checkParameterState("WEATHER_RAINFALL") == IPS_BUSY || checkParameterState("WEATHER_CLOUDS") == IPS_BUSY || checkParameterState("WEATHER_LIGHT") == IPS_BUSY)
	{
		setParameterValue("WEATHER_FORECAST", 1);
	}

	// danger zone
	if (checkParameterState("WEATHER_TEMPERATURE") == IPS_ALERT || checkParameterState("WEATHER_HUMIDITY") == IPS_ALERT || checkParameterState("WEATHER_WIND_SPEED") == IPS_ALERT || checkParameterState("WEATHER_RAINFALL") == IPS_ALERT || checkParameterState("WEATHER_CLOUDS") == IPS_ALERT || checkParameterState("WEATHER_LIGHT") == IPS_ALERT)
	{
		setParameterValue("WEATHER_FORECAST", 2);
	}

	TimerHit();
}
