/*
 * Turn off the Mitsubishi A/C units at NYCR.
 *
 * The IR is 36 KHz, which is non-standard compared to the normal 38 KHz.
 *
 * Start pulse is modulated high for 3.5 ms
 * 1 bits are 1720us low, then modulated high for 430us
 * 0 bits are  860us low, then modulated high for 430us
 *
 * Each time the button is pressed it sends the command twice.
 *
 * The device listens for MQTT commands or serial port commands to
 * send the OFF command.

    3440 us               430       430    430
   +------------+         +--+      +--+   +--+
___|            |_________|  |______|  |___|  |___
                 430+1320    1 = 1320  0=430

 */
#include "config.h"

#define MODULATION_WIDTH	  12 // usec == 35.7 kHz
#define SYNC_WIDTH		3440
#define HIGH_WIDTH		 430
#define ZERO_WIDTH		 430
#define ONE_WIDTH		1320


#include <ESP8266WiFi.h>
#include <WiFiClient.h>

WiFiClient wifi;

// MQTT libraries for reporting status
#include <PubSubClient.h>
PubSubClient mqtt(wifi);
static char device_id[16];
#define IR_INPUT_PIN 14

static const char off_cmd[] =
"11100010" "01101001" "10110010" "01000000"
"00000000" "00000000" "00001000" "01011000"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01010011" "0";

// this is the on command for 85F, swing h+v, 3 fan
static const char heat_cmd[] = 
"11100010" "01101001" "10110010" "01000000"
"00000000" "00000010" "00001000" "01011000"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01010000" "1";

// this is the on command for 70F, swing h+v, 3 fan
static const char cool_cmd[] =
"11100010" "01101001" "10110010" "01000000"
"00000000" "00000010" "00001000" "01010100"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01011000" "1";

void setup()
{
	pinMode(IR_INPUT_PIN, INPUT);
	Serial.begin(115200);
	thing_setup();
}


unsigned long high_length()
{
	// wait for a rising edge on the signal or timeout
	while(digitalRead(IR_INPUT_PIN) == 0)
		yield();

	// reset our start counter for the rising edge clock
	unsigned long start = micros();
	unsigned long last_high = start;

	while(1)
	{
		const int bit = digitalRead(IR_INPUT_PIN);
		const unsigned long now = micros();

		if (bit)
		{
			last_high = now;
			yield();
			continue;
		}

		// filter out the high-frequency modulation
		if (now - last_high < 100)
		{
			yield();
			continue;
		}

		// ok, this is probably a new bit
		unsigned long delta = now - start;
		return delta;
	}
}

void send_high(unsigned long length)
{
	const unsigned long start = micros();
	unsigned long last_toggle = start;
	int state = 1;

	digitalWrite(IR_INPUT_PIN, state);

	while(1)
	{
		unsigned long now = micros();

		if (now - start > length)
			break;

		if (now - last_toggle > MODULATION_WIDTH)
		{
			state = !state;
			last_toggle = now;
			digitalWrite(IR_INPUT_PIN, state);
		}
	}

	digitalWrite(IR_INPUT_PIN, 0);
}

void send_ir(const char * cmd)
{
	// send the sync pulse
	pinMode(IR_INPUT_PIN, OUTPUT);
	send_high(SYNC_WIDTH);
	delayMicroseconds(ZERO_WIDTH);

	while(1)
	{
		char bit = *cmd++;
		if(bit == '\0')
			break;

		if (bit == '1')
			delayMicroseconds(ONE_WIDTH);
		else
			delayMicroseconds(ZERO_WIDTH);

		send_high(HIGH_WIDTH);
	}

	pinMode(IR_INPUT_PIN, INPUT);
}



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

	if (!mqtt.connect("gpiothing", MQTT_USER, MQTT_PASSWORD))
	{
		Serial.println(" fail");
		return;
	}

	Serial.println(" success");

	// subscribe to all of our output status bits
	char buf[32];
	snprintf(buf, sizeof(buf), "/%s/ac/status", device_id);
	mqtt.subscribe(buf);
	Serial.println(buf);
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

	if (strncmp(msg, "HEAT", len) == 0)
	{
		send_ir(heat_cmd);
		delay(100);
		send_ir(heat_cmd);
	} else
	if (strncmp(msg, "COOL", len) == 0)
	{
		send_ir(cool_cmd);
		delay(100);
		send_ir(cool_cmd);
	} else
	if (strncmp(msg, "OFF", len) == 0)
	{
		send_ir(off_cmd);
		delay(100);
		send_ir(off_cmd);
	} else {
		Serial.println("UNKNOWN COMMAND");
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

	mqtt.setCallback(mqtt_callback);
	mqtt_connect();
}


void loop()
{
	// wait for a sync pulse or a command from the serial port
	while(1)
	{
		mqtt.loop();
		if(!mqtt.connected())
			mqtt_connect();

		if (Serial.available())
		{
			if (Serial.read() == '\n')
			{
				send_ir(off_cmd);
				delay(10);
				send_ir(off_cmd);
			}
		}

		if(digitalRead(IR_INPUT_PIN) != 0)
		{
			unsigned long length = high_length();
			if (length > 3000)
				break;
			Serial.print("-");
			Serial.println(length);
		}
	}

	unsigned long start = micros();

	Serial.println("------");

	while(1)
	{
		unsigned long now = micros();
		if(digitalRead(IR_INPUT_PIN) == 0)
		{
			// if we timeout on getting a rising edge,
			// the packet is done and we return to waiting
			// for a sync pulse
			if (now - start > 10000)
				break;

			yield();
			continue;
		}

		// we've started another bit
		// print the time length that we've been waiting
		unsigned long delta = now - start;
		if (800 < delta && delta < 900)
			Serial.print("0");
		else
		if (1600 < delta && delta < 1800)
			Serial.print("1");
		else
			Serial.print("?");
		//Serial.println(delta);
		start = now;

		// wait for the next falling edge
		high_length();
	}

	Serial.println();
}

