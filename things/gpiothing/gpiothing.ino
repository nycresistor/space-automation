/** \file
 * Home automation thing with MQTT.
 *
 * This configures some of the GPIO lines as inputs with pullup and
 * reports them to the MQTT server.  It also has some outputs and
 * accepts commands from the server to turn them on and off.
 *
 * On the Huzzah board these are all 3V, very limited power otuput.
 * Don't try to run high current things!
 *
 * The motion sensor seems to need a pull-down resistor to reliably
 * signal no motion.
 *
 * TODO: Add support for NeoPixel LEDs (on pin 15).
 * TODO: Add PWM support for analog output.
 */

#include "thing.h"

#if 0
// Neopixel hat
#include <Adafruit_NeoPixel.h>
#define WIDTH		8
#define HEIGHT		4
#define NEOPIXEL_PIN	15
Adafruit_NeoPixel pixels(WIDTH*HEIGHT, NEOPIXEL_PIN, NEO_GRB);
#endif


#define REPORT_TIMEOUT	1000	// 1 Hz serial output
#define INPUT_TIMEOUT	100	// wait 100 ms to debounce


static const unsigned input_pins[] = {
	14, 12, 13,
};


// gpio 0 is the red LED on the Huzzah board
// gpio 2 is the blue LED on the Huzzah board
static const unsigned output_pins[] = {
	5, 4, 0, 2,
};

static const unsigned input_pin_count
	= sizeof(input_pins)/sizeof(*input_pins);

static const unsigned output_pin_count
	= sizeof(output_pins)/sizeof(*output_pins);

static unsigned long last_change[input_pin_count]; // time in ms
static unsigned last_input[input_pin_count];
static unsigned last_output[output_pin_count];


/*
 * we've had a transition, notify the MQTT server
 * This works for both input and output ports
 */
void notify(unsigned id, const char * dir, unsigned state)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%s%d/state",
		dir,
		id
	);

	thing_publish(buf, state ? "ON" : "OFF");
}


static void
gpio_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;

	// we trust that the topic looks right, so it
	// should be of the form 'outX/state'
	const char id = topic[3]; // the digit after out
	if (id < '0' || id >= '0' + output_pin_count)
	{
		Serial.println("UNKNOWN TOPIC");
		return;
	}

	const unsigned pin = output_pins[id - '0'];
	unsigned val = 0;

	if (strncmp(msg, "ON", len) == 0)
	{
		val = 1;
	} else
	if (strncmp(msg, "OFF", len) == 0)
	{
		val = 0;
	} else {
		Serial.println("UNKNOWN COMMAND");
		return;
	}

	digitalWrite(pin, val);
	last_output[pin] = val;
	notify(id - '0', "out", val);
}
 

void setup()
{ 
	Serial.begin(115200);
	thing_setup();

	for(unsigned i = 0 ; i < input_pin_count ; i++)
	{
		pinMode(input_pins[i], INPUT_PULLUP);
		delay(10);
		last_input[i] = digitalRead(input_pins[i]);
	}
		
	for(unsigned i = 0 ; i < output_pin_count ; i++)
	{
		pinMode(output_pins[i], OUTPUT);
		digitalWrite(output_pins[i], 0);
		last_output[i] = 0;
	}

	// subscribe to all of our output status bits
	for(unsigned i = 0 ; i < output_pin_count ; i++)
		thing_subscribe(gpio_callback, "out%d/status", i);

	// and send all of our states
	for(unsigned i = 0 ; i < output_pin_count ; i++)
		notify(i, "out", last_output[i]);
	for(unsigned i = 0 ; i < input_pin_count ; i++)
		notify(i, "in", last_input[i]);
}


void loop()
{
	thing_loop();

	const unsigned long now = millis();

	// look for any change in inputs
	for(unsigned i = 0 ; i < input_pin_count ; i++)
	{
		const unsigned val = digitalRead(input_pins[i]);

		if (val == last_input[i])
		{
			last_change[i] = 0;
			continue;
		}

		// if this is the first switch, start our timeout
		if (last_change[i] == 0)
			last_change[i] = now;

		// if the change hasn't lasted long enough, keep waiting
		if (now - last_change[i] < INPUT_TIMEOUT)
			continue;

		// we've had a change and it has lasted long enough
		// to be real.  update the notifications
		last_input[i] = val;
		notify(i, "in", val);
	}


	int c = Serial.read();
	if (c == '\n')
	{
		// what to do?
	}

	static unsigned long last_report;

	if (now - last_report > REPORT_TIMEOUT)
	{
		last_report = now;

		Serial.print(device_id);
		Serial.print(" ");
		Serial.print(WiFi.localIP());
		Serial.print(" ");
		Serial.print(mqtt.connected() ? "MQTT" : "disconnected");

		for(unsigned i = 0 ; i < input_pin_count ; i++)
		{
			Serial.print(" ");
			Serial.print(last_input[i]);
		}

		Serial.print(" out");

		for(unsigned i = 0 ; i < output_pin_count ; i++)
		{
			Serial.print(" ");
			Serial.print(last_output[i]);
		}

		Serial.println();
	}
}
