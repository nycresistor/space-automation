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
 */
#include "thing.h"

static const unsigned long report_interval = 600000;

#include "ESPDMX.h"
DMXESPSerial dmx;
#define SWITCH_PIN 5

#define NUM_LIGHTS 8
static int channels[NUM_LIGHTS];
static int last_switch = 0;
static unsigned long last_update;

void light(int id, int bright)
{
	if (id < 1 || id > NUM_LIGHTS)
		return;

	if (bright < 0) bright = 0;
	if (bright > 255) bright = 255;

	channels[id-1] = bright;
	dmx.write(id, bright);

	char topic[32];
	snprintf(topic, sizeof(topic), "dmx%d/brightness_state", id);
	thing_publish(topic, "%d", bright);

	snprintf(topic, sizeof(topic), "dmx%d/state", id);
	thing_publish(topic, "%s", bright == 0 ? "OFF" : "ON");
}


int brightness(int id)
{
	if (id < 1 || id > NUM_LIGHTS)
		return -1;
	return channels[id-1];
}


void light_bright_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;
	const unsigned id = topic[3] - '0';

	int bright = atoi((const char*) payload);

	light(id, bright);
}

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
	pinMode(SWITCH_PIN, INPUT_PULLUP);

	Serial.begin(115200);

	thing_setup();

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

	if(digitalRead(SWITCH_PIN) != 0)
	{
		if (last_switch == 1)
		{
			Serial.println("ALL OFF");
			for(int i = 0 ; i < NUM_LIGHTS ; i++)
				light(i+1, 0);
		}
		last_switch = 0;
	} else
	if (last_switch == 0)
	{
		// the switch has been turned on.
		// if we had been turned off before,
		// go full bright
		Serial.println("ALL ON");
		for(int i = 0 ; i < NUM_LIGHTS ; i++)
			light(i+1, 255);

		last_switch = 1;
	}

	dmx.update();

	light_report();
}
