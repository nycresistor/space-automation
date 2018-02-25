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

static const unsigned long report_interval = 30000;

#include "ESPDMX.h"
DMXESPSerial dmx;
#define SWITCH_PIN 5

#define NUM_LIGHTS 8
static int brightness = 0;
static int last_switch = 0;
static unsigned long last_update;
#define FADE_RATE 30

void dmx_callback(
	const char * topic,
	const uint8_t * payload,
	size_t len
)
{
	const char * msg = (const char *) payload;
}
 
int channels[32];

void serial_console()
{
	const char c = Serial.read();

	if ('0' <= c && c <= '9')
	{
		int chan = c - '0';
		channels[chan] = !channels[chan];
		dmx.write(chan, channels[chan] ? 0xFF : 0);
		Serial.println(c);
		return;
	}

	if (c == '-')
	{
		for(int i = 0 ; i < 32 ; i++)
		{
			channels[i] = 0;
			dmx.write(i, 0);
		}
		Serial.println("all off");
		return;
	}

	if (c == '+')
	{
		for(int i = 0 ; i < NUM_LIGHTS ; i++)
		{
			//channels[i] = 1;
			dmx.write(i, 0xFF);
		}
		Serial.println("all on");
		return;
	}
}


void setup()
{
	dmx.init(NUM_LIGHTS);
	pinMode(SWITCH_PIN, INPUT_PULLUP);

	Serial.begin(115200);

	//thing_setup();
	//thing_subscribe(ac_callback, "aircon/status");
	//ac_off();
}


void loop()
{
	//thing_loop();
	
	if (Serial.available())
		serial_console();

	const unsigned long now = millis();

	if(digitalRead(SWITCH_PIN) != 0)
	{
		// the switch is released; fade the lights
		if (brightness != 0 && now - last_update > FADE_RATE)
		{
			brightness--;
			last_update = now;
			for(int i = 0 ; i < NUM_LIGHTS ; i++)
				dmx.write(i, brightness);
		}
		last_switch = 0;
	} else {
		// the switch has been turned on.
		// if we were all the way off, go full bright
		// otherwise hold the brightness
		if (brightness == 0 && last_switch == 0)
		{
			brightness = 0xFF;
			for(int i = 0 ; i < NUM_LIGHTS ; i++)
				dmx.write(i, brightness);
		}

		last_switch = 1;
	}

	dmx.update();

	static unsigned long last_report;
	if (now - last_report > report_interval)
	{
		last_report = now;
	}
}
