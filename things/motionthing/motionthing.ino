/** \file
 * Home automation thing with MQTT.
 *
 * PIR motion sensor interface to MQTT with a Neopixel featherwing
 */

// User provided wifi and mqtt credentials
// these should be in the EEPROM or something
#include "config.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
//#include <WiFiClientSecure.h>

WiFiClient wifi;

// MQTT libraries for reporting status
#include <PubSubClient.h>
PubSubClient mqtt(wifi);


// Neopixel hat
#include <Adafruit_NeoPixel.h>
#define WIDTH		8
#define HEIGHT		4
#define NEOPIXEL_PIN	15
Adafruit_NeoPixel pixels(WIDTH*HEIGHT, NEOPIXEL_PIN, NEO_GRB);
static int lamp_state;

// Motion sensor goes high for motion
#define MOTION_PIN	14
unsigned last_motion;


static void lamp_on(int bright = 255)
{
	if (bright > 255)
		bright = 255;

	for(int i = 0 ; i < WIDTH*HEIGHT ; i++)
	{
		int b = bright > (i * 10 / (WIDTH*HEIGHT)) ? 10 : 0;
		pixels.setPixelColor(i, b, b, b);
	}
	pixels.show();
	lamp_state = 1;
}

static void lamp_off()
{
	Serial.println("off!");
	for(int i = 0 ; i < WIDTH*HEIGHT ; i++)
		pixels.setPixelColor(i, 0, 0, 0);
	pixels.show();
	lamp_state = 0;
}

int wifi_connect(char* ssid, char* password)
{
	int connAttempts = 0;

	Serial.println("\r\nConnecting to: "+String(ssid));
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
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


static void
mqtt_connect()
{
	static long last_mqtt_connect;

	if (mqtt.connected() || millis() - last_mqtt_connect < 5000)
		return;

	last_mqtt_connect = millis();

	Serial.print("mqtt: Trying to connect ");
	Serial.print(mqtt_server);
	mqtt.setServer(mqtt_server, mqtt_port);
	if (!mqtt.connect(mqtt_id, MQTT_USER, mqtt_password))
	{
		Serial.println(" fail");
		return;
	}

	Serial.println(" success");
	mqtt.subscribe("/motion1/status");
}


static void
mqtt_callback(
	char * topic,
	byte * payload,
	unsigned int len
)
{
	Serial.print("MQTT: ");
	Serial.print(topic);
	Serial.print("'");
	Serial.write(payload, len);
	Serial.println("'");

	const char * msg = (const char *) payload;

	// we can't turn it on or off, we can only toggle it.
	// so we use the measured lamps state to 
#if 0
	if (strncmp(msg, "TOGGLE", len) == 0)
	{
		lamp_toggle();
	} else
#endif
	if (strncmp(msg, "ON", len) == 0)
	{
		lamp_on();
	} else
	if (strncmp(msg, "OFF", len) == 0)
	{
		lamp_off();
	} else {
		Serial.println("Unknown command");
	}
}
 

void setup()
{ 
	Serial.begin(115200);

	pinMode(MOTION_PIN, INPUT);

	wifi_connect(ssid,password);
	configTime(1, 3600, "pool.ntp.org");

	mqtt.setCallback(mqtt_callback);
	mqtt_connect();

	pixels.begin();
}


void notify(int state)
{
	// we've had a transition
	mqtt.publish("/motion1/state", state ? "ON" : "OFF");
}

 
void loop()
{
	// if we've lost connection, reconnect to our mqtt server
	mqtt.loop();
	if(!mqtt.connected())
		mqtt_connect();

	int motion = digitalRead(MOTION_PIN);
	static int bright = 0;
	static unsigned long last_motion, first_motion;
	unsigned long now = millis();

	if (motion)
	{
		if (first_motion == 0)
		{
			notify(1);
			first_motion = now;
		}

		last_motion = now;

		int delta = now - first_motion;
		bright = delta > 1000 ? 250 : delta/4;
		lamp_on(bright);
	} else {
		if (bright > 0)
			lamp_on(--bright);

		if (last_motion != 0 && now - last_motion > 500)
		{
			// ok, we're done
			notify(0);
			lamp_off();
			first_motion = last_motion = 0;
			bright = 0;
		}
	}

	int c = Serial.read();
	if (c == '\n')
	{
		if (lamp_state)
			lamp_off();
		else
			lamp_on();
	}

}
