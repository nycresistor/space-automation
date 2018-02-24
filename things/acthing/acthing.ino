/*
 * Turn off the Mitsubishi A/C units at NYCR.
 *
 * The IR is 36 KHz, which is non-standard compared to the normal 38 KHz.
 *
 * Start pulse 3440us high, 1720us low
 * 1 bits are   430us high, 1320us low
 * 0 bits are   430us high,  860us low
 *
 */
#include "thing.h"

#define MODULATION_WIDTH	  12 // usec == 35.7 kHz
#define SYNC_WIDTH		3440
#define SYNC_LOW_WIDTH		1760
#define HIGH_WIDTH		 430
#define ZERO_WIDTH		 430
#define ONE_WIDTH		1320


// DHT sensor library for temp and humidity
#include <DHTesp.h>
#define DHT_PIN 12
DHTesp dht;
unsigned long last_measurement;
unsigned long last_report;
unsigned long report_interval = 300000; // 5 minutes
unsigned long measurement_interval = 2000; // 2 seconds


// IR remote control
#define IR_INPUT_PIN 13
#define IR_OUTPUT_PIN 14

static const char off_cmd[]	= "C4D36480000010B003DE00000000000000A6";
// this is the on command for 85F, swing h+v, 3 fan
static const char heat_cmd[]	= "C4D36480000410B003DE00000000000000A1";
// this is the on command for 70F, swing h+v, 3 fan
static const char cool_cmd[]	= "C4D36480000410A803DE00000000000000B1";

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


void ir_led(int state)
{
	pinMode(IR_OUTPUT_PIN, state ? OUTPUT : INPUT);
}

void send_high(unsigned long length)
{
	const unsigned long start = micros();
	unsigned long last_toggle = start;
	int state = 1;

	// Drive the output pin low, turning on the LED
	ir_led(state);

	while(1)
	{
		unsigned long now = micros();

		if (now - start > length)
			break;

		if (now - last_toggle < MODULATION_WIDTH)
			continue;

		state = !state;
		last_toggle = now;

		// toggle the LED by driving the pin low (to turn it on)
		// or letting it float (to turn it off)
		ir_led(state);
	}

	// let the pin float, turning off the LED
	ir_led(0);
}

void send_ir(const char * cmd)
{
	// send the sync pulse
	pinMode(IR_INPUT_PIN, OUTPUT);
	send_high(SYNC_WIDTH);
	delayMicroseconds(SYNC_LOW_WIDTH);
	send_high(HIGH_WIDTH);


	while(1)
	{
		char nib = *cmd++;
		if(nib == '\0')
			break;
		if ('0' <= nib && nib <= '9')
			nib = nib - '0';
		else
		if ('a' <= nib && nib <= 'f')
			nib = nib - 'a' + 0xA;
		else
		if ('A' <= nib && nib <= 'F')
			nib = nib - 'A' + 0xA;

		for(int i = 0 ; i < 4 ; i++, nib <<= 1)
		{
			const int bit = nib & 0x08;

			if (bit)
				delayMicroseconds(ONE_WIDTH);
			else
				delayMicroseconds(ZERO_WIDTH);

			send_high(HIGH_WIDTH);
		}

	}

	pinMode(IR_INPUT_PIN, INPUT);
}


void ac_heat()
{
	send_ir(heat_cmd);
	delay(10);
	send_ir(heat_cmd);

	thing_publish("aricon/state", "HEAT");

	// turn on the RED led
	red_led(1);
	blue_led(0);
}


void ac_off()
{
	send_ir(off_cmd);
	delay(100);
	send_ir(off_cmd);
	thing_publish("aircon/state", "OFF");

	red_led(0);
	blue_led(0);
}


void ac_cool()
{
	send_ir(cool_cmd);
	delay(100);
	send_ir(cool_cmd);
	thing_publish("aircon/state", "COOL");

	red_led(0);
	blue_led(1);
}


void
ac_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;

	if (strncmp(msg, "OFF", len) == 0)
	{
		ac_off();
	} else
	if (strncmp(msg, "HEAT", len) == 0)
	{
		ac_heat();
	} else
	if (strncmp(msg, "COOL", len) == 0)
	{
		ac_cool();
	} else {
		Serial.print(msg);
		Serial.println(" UNKNOWN COMMAND");
		return;
	}
}
 

void read_ir()
{
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


void send_temp(TempAndHumidity th)
{
	if (dht.getStatus() == DHTesp::ERROR_NONE)
	{
		thing_publish("sensor/temperature", "%.2f", th.temperature);
		thing_publish("sensor/humidity", "%.2f", th.humidity);
	} else
		thing_publish("sensor/state", "%s", dht.getStatusString());
}


void serial_console()
{
	const char c = Serial.read();

	if (c == 'x') ac_off();
	if (c == 'h') ac_heat();
	if (c == 'c') ac_cool();

	if (c == 't')
	{
		TempAndHumidity th = dht.getTempAndHumidity();
		send_temp(th);
	}
}


void setup()
{
	dht.setup(DHT_PIN);
	pinMode(IR_INPUT_PIN, INPUT);

	// turn the LED off by 
	digitalWrite(IR_OUTPUT_PIN, 0);
	ir_led(0);
	
	Serial.begin(115200);

	thing_setup();
	thing_subscribe(ac_callback, "aircon/status");
	ac_off();
}


void loop()
{
	thing_loop();

	if (Serial.available())
		serial_console();

	if(digitalRead(IR_INPUT_PIN) != 0)
	{
		unsigned long length = high_length();
		if (length > 3000)
			read_ir();
	}

	const unsigned long now = millis();
	if (now - last_measurement > measurement_interval)
	{
		last_measurement = now;
		TempAndHumidity th = dht.getTempAndHumidity();

		if (now - last_report > report_interval)
		{
			last_report = now;
			Serial.print("MQTT: ");
			send_temp(th);
		}
	}
}
