/*
 * Control the DMX lights at NYCR.
 *
 * There is a single pull-up for manual control.
 * otherwise listen for light commands.
 *
 * DMX level shifter is on gpio 2 (blue LED).
 *
 * The DMX dimmer wants *constant* updates or else it won't keep the
 * lights on.
 *
 * Messages it publishes:
 *   dmx[1-8]/brightness_state [0-255]
 *   dmx[1-8]/state ["ON" or "OFF"]
 *
 * Messages it subscribes to (leave off the ID for all on or all off):
 *   dmx[1-8]/command ["ON" or "OFF"]
 *   dmx[1-8]/brightness [0-255]
 *   dmx/command ["ON" or "OFF"] 
 *   dmx/brightness [0-255]
 */
#include "thing.h"

static const unsigned long report_interval = 600000;

#include "ESPDMX.h"
DMXESPSerial dmx;
#define SWITCH_PIN 5 	// the knife switch by the door

#define NUM_LIGHTS 8 	// aka number of channels in use on the DMX controller
static int channels[NUM_LIGHTS]; // brightness values for the lights
static int last_switch = 0;
static unsigned long last_update;

// sets a light to a particular brightness
void light(int id, int bright)
{
	if (id < 1 || id > NUM_LIGHTS)
		return;

	// keep values within range
	if (bright < 0) bright = 0;
	if (bright > 255) bright = 255;

	// update the brightness status for the light and send to the light
	channels[id-1] = bright;
	dmx.write(id, bright);

	// publish new brightness state
	char topic[32];
	snprintf(topic, sizeof(topic), "dmx%d/brightness_state", id);
	thing_publish(topic, "%d", bright);

	// publish new ON/OFF state
	snprintf(topic, sizeof(topic), "dmx%d/state", id);
	thing_publish(topic, "%s", bright == 0 ? "OFF" : "ON");
}

// verify that the channel exists
int brightness(int id)
{
	if (id < 1 || id > NUM_LIGHTS)
		return -1;
	return channels[id-1];
}

// read message payload and call light() to update all lights
void all_bright_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;

	// convert string to int
	int bright = atoi((const char*) payload);

	for (int id = 0; id < NUM_LIGHTS; id++) {
		light(id, bright);
	}
}

// read message payload and make all lights brightness 0 for OFF, 255 for ON
void all_status_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;

	for (int id = 0; id < NUM_LIGHTS; id++) {
		if (memcmp(payload, "OFF", len) == 0)
			light(id, 0);
		else
		if (memcmp(payload, "ON", len) == 0)
		{
			if (brightness(id) == 0)
				light(id, 255);
		}
	}
}
 
// read message payload and call light() to update the one light
void light_bright_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;
	const unsigned id = topic[3] - '0';

	// convert string to int
	int bright = atoi((const char*) payload);

	light(id, bright);
}

// read message payload and make light's brightness 0 for OFF, 255 for ON
void light_status_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;
	const unsigned id = topic[3] - '0';
	if (memcmp(payload, "OFF", len) == 0)
		light(id, 0);
	else
	if (memcmp(payload, "ON", len) == 0)
	{
		if (brightness(id) == 0)
			light(id, 255);
	}

}
 
// Resend the stored brightness values to every light periodically
// Because the DMX controller wants attention or it will turn off the lights
void light_report()
{
	static unsigned long last_report;
	const unsigned long now = millis();
	if (now - last_report < report_interval)
		return;

	last_report = now;
	for(int i = 0 ; i < NUM_LIGHTS ; i++)
		light(i+1, brightness(i+1));
}

// Allow control via the serial monitor for testing purposes
void serial_console()
{
	const char c = Serial.read();

	if ('1' <= c && c <= '9')
	{
		int chan = c - '0';
		int bright = (brightness(chan) == 0) ? 0xFF : 0x00;
		light(chan, bright);
		return;
	}

	if (c == '-')
	{
		for(int i = 0 ; i < NUM_LIGHTS ; i++)
			light(i+1, 0);
		Serial.println("all off");
		return;
	}

	if (c == '+')
	{
		for(int i = 0 ; i < NUM_LIGHTS ; i++)
			light(i+1, 0xFF);
		Serial.println("all on");
		return;
	}
}


void setup()
{
	dmx.init(NUM_LIGHTS);
	pinMode(SWITCH_PIN, INPUT_PULLUP); 	// the knife switch by the door

	Serial.begin(115200);

	thing_setup();

	// all-at-once commands
	thing_subscribe(all_status_callback, "dmx/command");
	thing_subscribe(all_bright_callback, "dmx/brightness");

	// single-light-specific commands
	for(int i = 1 ; i <= NUM_LIGHTS ; i++)
	{
		thing_subscribe(light_status_callback, "dmx%d/command", i);
		thing_subscribe(light_bright_callback, "dmx%d/brightness", i);
	}

}


void loop()
{
	thing_loop();
	
	if (Serial.available())
		serial_console();

	const unsigned long now = millis();

	// change lights if the knife switch by the door is triggered
	if(digitalRead(SWITCH_PIN) != 0) 	
	{
		// it was on
		if (last_switch == 1)
		{
			// now turn it off
			Serial.println("ALL OFF");
			for(int i = 0 ; i < NUM_LIGHTS ; i++)
				light(i+1, 0);
		}
		last_switch = 0;
	} else
	// it was off
	if (last_switch == 0)
	{
		// now turn it on (go full bright)
		Serial.println("ALL ON");
		for(int i = 0 ; i < NUM_LIGHTS ; i++)
			light(i+1, 255);

		last_switch = 1;
	}

	dmx.update();

	light_report();
}
