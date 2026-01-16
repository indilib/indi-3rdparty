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

#pragma once

#include "indiweather.h"
#include <mosquitto.h>

class WeatherMQTT : public INDI::Weather
{
  public:
	WeatherMQTT();
	virtual ~WeatherMQTT();

	bool Connect() override;
	bool Disconnect() override;
	const char *getDefaultName() override;

	virtual bool initProperties() override;
	virtual bool updateProperties() override;
	virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) override;

  protected:
	virtual IPState updateWeather() override;
	virtual bool saveConfigItems(FILE *fp) override;
	char mqtt_clientid[32] = {};

  private:
	IText MqttServerT[4];
	ITextVectorProperty MqttServerTP;
	IText MqttTopicsT[8];
	ITextVectorProperty MqttTopicsTP;

	struct mosquitto *mosq = NULL;
	void mqttSubscribe();
	void mqttUnSubscribe();
	int MqttLoopTimerID { -1 };
	static void MqttLoopHelper(void *context);
	void MqttLoop();
	static void mqttMsgCallback(struct mosquitto *, void *, const struct mosquitto_message *message);
	void mqttMsg(char *topic, char *message);
};
