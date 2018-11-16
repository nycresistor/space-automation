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
#include <Adafruit_NeoPixel.h>

static const unsigned long report_interval = 600000;

#define BUTTON_PIN      14
#define NEOPIXEL_PIN    15
#define NUM_PIXELS 		16

static int last_press = 0;
static unsigned long last_update;

int idle_brightness = 0;
int idle_direction = 1;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(
    NUM_PIXELS,
    NEOPIXEL_PIN,
    NEO_GRB + NEO_KHZ800
);

// read message payload and make light's brightness 0 for OFF, 255 for ON
/*
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
*/
 
// Allow control via the serial monitor for testing purposes
/*
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
*/

void setup()
{
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	strip.begin();

	Serial.begin(115200);

	thing_setup();

	// all-at-once commands
	//thing_subscribe(all_status_callback, "dmx/command");
	//thing_subscribe(all_bright_callback, "dmx/brightness");
}


void loop()
{
	thing_loop();
	
	//if (Serial.available())
		//serial_console();

	// see what's happening with the button
	const unsigned button_state = digitalRead(BUTTON_PIN);

	// omg someone is pressing the button!
	if (button_state != 0) {
		// report via mqtt
		char topic[32];
		snprintf(topic, sizeof(topic), "bigred/command");
		thing_publish(topic, "%s", "OFF");
		// run the animation
		all_off_pixel_sequence();
	}

	idle_pixel_sequence();
}

void idle_pixel_sequence() {
	for (int i = 0; i < NUM_PIXELS; i++) 
		strip.setPixelColor(i, idle_brightness, idle_brightness, idle_brightness);

	strip.show();
	delay(10);
	
	idle_brightness += idle_direction;

	if (idle_brightness == 255)
		idle_direction = -1;
	else if (idle_brightness == 0)
		idle_direction = +1;
}

void all_off_pixel_sequence()
{
	for (int i = 0; i < 4; i++) {
		for (int i = 0; i < NUM_PIXELS; i++) 
			strip.setPixelColor(i, 255, 255, 255);
		delay(10);
		for (int i = 0; i < NUM_PIXELS; i++) 
			strip.setPixelColor(i, 0, 0, 0);
		delay(10);
	}	
	
}

