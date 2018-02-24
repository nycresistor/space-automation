/**
 * \file
 * Common functions for all things that work with the NYCR space automation.
 *
 * Devices that use this should call thing_setup() in their setup().
 * Afterwards call thing_subscribe() to subscribe to MQTT commands.
 * Inside of loop(), call thing_loop() to check for MQTT commands.
 */

#include <stdarg.h>
#include "config.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

WiFiClient wifi;

// MQTT libraries for reporting status
#include <PubSubClient.h>
PubSubClient mqtt(wifi);
char device_id[16];

int
wifi_connect()
{
	int connAttempts = 0;

	Serial.println(String(device_id) + " connecting to " + String(WIFI_SSID));
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.print(".");

	while (WiFi.status() != WL_CONNECTED ) {
		delay(500);
		Serial.print(".");
		if(connAttempts > 20)
			return -5;
		connAttempts++;
	}

	Serial.println("WiFi connected\r\n");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	return 1;
}

typedef void (*thing_callback_t)(const char * topic, const uint8_t * msg, size_t len);

#define MAX_TOPICS 16

static int topic_count;
static char topic_names[MAX_TOPICS][32];
static thing_callback_t topic_callbacks[MAX_TOPICS];

void
mqtt_connect()
{
	static long last_mqtt_connect;

	if (mqtt.connected() || millis() - last_mqtt_connect < 5000)
		return;

	last_mqtt_connect = millis();

	Serial.print("mqtt: Trying to connect ");
	Serial.print(MQTT_SERVER);

	mqtt.setServer(MQTT_SERVER, MQTT_PORT);

	if (!mqtt.connect(device_id, MQTT_USER, MQTT_PASSWORD))
	{
		Serial.println(" fail");
		return;
	}

	Serial.println(" success");
	for(int i = 0 ; i < topic_count ; i++)
		mqtt.subscribe(topic_names[i]);
}


void
__attribute__ ((format (printf, 2, 3)))
thing_subscribe(
	thing_callback_t callback,
	const char * fmt,
	...
)
{
	if (topic_count == MAX_TOPICS)
		return;

	char * buf = topic_names[topic_count];
	topic_callbacks[topic_count] = callback;

	// leading part is /0123456789ab/ with device mac address
	buf[0] = '/';
	memcpy(buf+1, device_id, 12);
	buf[13] = '/';

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf+14, 32 - 14, fmt, ap);
	va_end(ap);

	mqtt.subscribe(buf);

	Serial.print("SUBSCRIBE ");
	Serial.println(buf);
	topic_count++;
}


// MQTT library has a single callback; we look at our list
// to determine which one is appropriate and strip off the
// device id "/0123456789ab/" portion, 14 bytes
static void
mqtt_callback(
	char * topic,
	byte * payload,
	unsigned int len
)
{
	Serial.print("MQTT: ");
	Serial.print(topic);
	Serial.print("='");
	Serial.write(payload, len);
	Serial.println("'");

	for(int i = 0 ; i < topic_count ; i++)
	{
		if (strcmp(topic, topic_names[topic_count]) != 0)
			continue;

		topic_callbacks[topic_count](topic + 14, payload, len);
		return;
	}
}



void thing_setup()
{ 
	uint8_t mac_bytes[6];
	WiFi.macAddress(mac_bytes);
	snprintf(device_id, sizeof(device_id), "%02x%02x%02x%02x%02x%02x",
		mac_bytes[0],
		mac_bytes[1],
		mac_bytes[2],
		mac_bytes[3],
		mac_bytes[4],
		mac_bytes[5]
	);

	wifi_connect();
	configTime(1, 3600, "pool.ntp.org");

	topic_count = 0;
	mqtt.setCallback(mqtt_callback);
	mqtt_connect();
}


void
__attribute__ ((format (printf, 2, 3)))
thing_publish(
	const char * user_topic,
	const char * fmt,
	...
)
{
	char topic[32];
	char msg[32];

	snprintf(topic, sizeof(topic), "/%s/%s", device_id, user_topic);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	Serial.print(topic);
	Serial.print("=");
	Serial.println(msg);

	mqtt.publish(topic, msg);
}


void thing_loop()
{
	mqtt.loop();
	if(!mqtt.connected())
		mqtt_connect();
}
