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
 *
 * The DHT22 pinout is
 *   ___________
 *  /           \
 * /______O______\
 * |             |
 * |             |
 * |             |
 * |   AM2302    |
 * +-------------+
 *  |   |   |   |
 * Vcc Data NC Gnd
 *

	Configuration (HA):
	sensor temperature:
	  platform: mqtt
	  state_topic: '/18fe34d44eb8/sensor/temperature'
	  name: 'Temperature'
	  unit_of_measurent: 'ºC'

	sensor humidity:
	  platform: mqtt
	  state_topic: '/18fe34d44eb8/sensor/humidity'
	  name: 'Humidity'
	  unit_of_measurent: '%'

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
// TODO: fix the encoding to not have an off-by-one error
// TODO: test with the LED from 5v to ground
#define IR_INPUT_PIN 13
#define IR_OUTPUT_PIN 14

static const char off_cmd[] =
"1100010011010011011001001000000"
"00000000000000000000100001011000"
"00000001111011110000000000000000"
"00000000000000000000000000000000"
"00000000010100110";

// this is the on command for 85F, swing h+v, 3 fan
static const char heat_cmd[] = 
"1100010" "01101001" "10110010" "01000000"
"00000000" "00000010" "00001000" "01011000"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01010000" "1";

// this is the on command for 70F, swing h+v, 3 fan
static const char cool_cmd[] =
"1100010" "01101001" "10110010" "01000000"
"00000000" "00000010" "00001000" "01010100"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01011000" "1";

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

	while(1)
	{
		send_high(HIGH_WIDTH);

		char bit = *cmd++;
		if(bit == '\0')
			break;

		if (bit == '1')
			delayMicroseconds(ONE_WIDTH);
		else
			delayMicroseconds(ZERO_WIDTH);

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
