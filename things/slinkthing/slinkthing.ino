/** \file
 * Sony Control-A1 II protocol interface over MQTT.
 *
 * For the Adafruit Huzzah ESP8266 board.  Since S-link is a 5 V
 * protocol with pullups and the board is not 5V tolerant the
 * signal is fed into pin 12 through a resistor.
 *
 * Best list of commands: http://boehmel.de/slink.htm
 */
#include "thing.h"

#define SLINK_PIN 12
#define DEVID 0x70

static const int debug = 0;
static const uint8_t mute_on_cmd[]	= { DEVID, 0x06 };
static const uint8_t mute_off_cmd[]	= { DEVID, 0x07 };
static const uint8_t power_on_cmd[]	= { DEVID, 0x2E };
static const uint8_t power_off_cmd[]	= { DEVID, 0x2F };
static const uint8_t volume_up_cmd[]	= { DEVID, 0x14 };
static const uint8_t volume_down_cmd[]	= { DEVID, 0x15 };


void mute_command(
	const char * topic,
	const uint8_t * msg,
	size_t len
)
{
	if (memcmp(msg, "ON", len) == 0)
	{
		slink_send(mute_on_cmd, sizeof(mute_on_cmd));
		thing_publish("state/mute", "ON");
	} else
	if (memcmp(msg, "OFF", len) == 0)
	{
		slink_send(mute_off_cmd, sizeof(mute_off_cmd));
		thing_publish("state/mute", "OFF");
	} else {
		Serial.println("???");
		// ?
	}
}

void power_command(
	const char * topic,
	const uint8_t * msg,
	size_t len
)
{
	if (memcmp(msg, "ON", len) == 0)
	{
		slink_send(power_off_cmd, sizeof(power_off_cmd));
		thing_publish("state/power", "ON");
	} else
	if (memcmp(msg, "ON", len) == 0)
	{
		slink_send(power_on_cmd, sizeof(power_on_cmd));
		thing_publish("state/power", "ON");
	} else {
		// ?
	}
}

void volume_command(
	const char * topic,
	const uint8_t * msg,
	size_t len
)
{
	if (memcmp(msg, "UP", len) == 0)
	{
		slink_send(volume_up_cmd, sizeof(volume_up_cmd));
	} else
	if (memcmp(msg, "DOWN", len) == 0)
	{
		slink_send(volume_down_cmd, sizeof(volume_down_cmd));
	} else {
		// ?
	}
}


void source_command(
	const char * topic,
	const uint8_t * msg,
	size_t len
)
{
	uint8_t cmd[] = { DEVID, 0x50, 0x00 };

	if (memcmp(msg, "Video1", len) == 0)
	{
		cmd[2] = 0x10;
	} else
	if (memcmp(msg, "Video2", len) == 0)
	{
		cmd[2] = 0x11;
	} else
	if (memcmp(msg, "Video3", len) == 0)
	{
		cmd[2] = 0x12;
	} else
	if (memcmp(msg, "DVD", len) == 0)
	{
		cmd[2] = 0x19;
	} else
	if (memcmp(msg, "TV", len) == 0)
	{
		cmd[2] = 0x17;
	} else {
		return;
	}

	slink_send(cmd, sizeof(cmd));
	thing_publish("state/source", "%s", msg);
}

void setup()
{ 
	// we should probably use an external pullup to detect
	// a disconnected bus, although the Sony devices will
	// include their own
	pinMode(SLINK_PIN, INPUT_PULLUP);

	Serial.begin(115200);
	thing_setup();
	thing_subscribe(mute_command, "mute");
	thing_subscribe(source_command, "source");
	thing_subscribe(volume_command, "volume");
	thing_subscribe(power_command, "power");
}


void slink_loop(void)
{
	static unsigned bits = 0;
	static unsigned data = 0;
	static unsigned fail = 0;
	unsigned bit = 0;

	if (digitalRead(SLINK_PIN) != 0)
		return;

	// read the pulse width of an entire low

	const unsigned long start = micros();

	red_led(1);
	while(digitalRead(SLINK_PIN) == 0)
		yield();

	unsigned long delta = micros() - start;
	red_led(0);

	if (delta > 2000)
	{
		if (fail != 0)
			Serial.print(" FAIL!");
		if (bits != 0)
			Serial.print(" partial?");
		data = fail = bits = 0;
		Serial.println();
		return;
	} else
	if (1100 < delta && delta < 1300)
	{
		bit = 1;
	} else
	if (500 < delta && delta < 700)
	{
		bit = 0;
	} else {
		fail = 1;
		bit = 0;
	}

	if (debug)
	{
		Serial.print(bit);
		Serial.print(" ");
		Serial.println(delta);
	}

	data = (data << 1) | bit;
	if (++bits == 8)
	{
		static const char hexdigit[] = "0123456789abcdef";
		Serial.print(hexdigit[(data >> 4) & 0xF]);
		Serial.print(hexdigit[(data >> 0) & 0xF]);

		if (fail)
			Serial.print("?");

		fail = data = bits = 0;
	}

}

void slink_pulse(unsigned int width)
{
	// pull it low for the user pulse
	digitalWrite(SLINK_PIN, 0);
	pinMode(SLINK_PIN, OUTPUT);
	delayMicroseconds(width);

	// let it rise for 600 usec
	pinMode(SLINK_PIN, INPUT_PULLUP);
	delayMicroseconds(600);
}


// busywait during the send for now
// Bytes are send MSB first
// 1 == 1200 us low, 600 us high
// 0 ==  600 us low, 600 us high
void slink_send_byte(uint8_t byte)
{
	for(unsigned i = 0 ; i < 8 ; i++, byte <<= 1)
	{
		if (byte & 0x80)
			slink_pulse(1200);
		else
			slink_pulse(600);
	}
}

void slink_send(const uint8_t * bytes, unsigned len)
{
	blue_led(1);

	// start bit is 2400 usec
	slink_pulse(2400);

	for(unsigned i = 0 ; i < len ; i++)
		slink_send_byte(bytes[i]);

	blue_led(0);
}


void serial_loop()
{
	static unsigned len = 0;
	static uint8_t buf[32];

	int c = Serial.read();
	if (c == -1)
		return;

	if(c == '\n')
	{
		Serial.print(" : ");
		Serial.print(len/2);
		Serial.println(" bytes");
		slink_send(buf, len/2);
		len = 0;
		return;
	}

	if (len == 0)
		Serial.println("-> ");
	Serial.print((char) c);

	if ('0' <= c && c <= '9')
		c = c - '0';
	else
	if ('a' <= c && c <= 'f')
		c = c - 'a' + 0xa;
	else
		return;

	if (len & 1)
	{
		// second nibble
		buf[len/2] |= c;
	} else {
		// first nibble
		buf[len/2] = c << 4;
	}

	len++;
}
 
void loop()
{
	thing_loop();
	serial_loop();
	slink_loop();
}
