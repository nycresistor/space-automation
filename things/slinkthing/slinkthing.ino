/** \file
 * Sony Control-A1 II protocol interface over MQTT.
 *
 * For the Adafruit Huzzah ESP8266 board.  Since S-link is a 5 V
 * protocol with pullups and the board is not 5V tolerant the
 * signal is fed into pin 12 through a resistor.
 */
#include "thing.h"

#define SLINK_PIN 12

void setup()
{ 
	Serial.begin(115200);

	// we should probably use an external pullup to detect
	// a disconnected bus, although the Sony devices will
	// include their own
	pinMode(SLINK_PIN, INPUT_PULLUP);
}


void slink_read(void)
{
	if (digitalRead(SLINK_PIN) != 0)
		return;

	// read the pulse width of an entire low

	const unsigned long start = micros();

	red_led(1);
	while(digitalRead(SLINK_PIN) == 0)
		yield();

	unsigned long delta = micros() - start;
	red_led(0);

	//Serial.println(delta);
	if (delta > 2000)
		Serial.println("----");
	else
	if (1100 < delta && delta < 1300)
		Serial.print("1");
	else
	if (500 < delta && delta < 700)
		Serial.print("0");
	else {
		Serial.println();
		Serial.println(delta);
	}
		
}


// busywait during the send for now
// Bytes are send MSB first
// 1 == 1200 us low, 600 us high
// 0 ==  600 us low, 600 us high
void slink_send_byte(uint8_t byte)
{
	for(unsigned i = 0 ; i < 8 ; i++, byte <<= 1)
	{
		digitalWrite(SLINK_PIN, 0);
		if (byte & 0x80)
			delayMicroseconds(1200);
		else
			delayMicroseconds(600);

		digitalWrite(SLINK_PIN, 1);
		delayMicroseconds(600);
	}
}

void slink_send(uint8_t bytes[], unsigned len)
{
	pinMode(SLINK_PIN, OUTPUT);

	// start bit is 2400 usec
	digitalWrite(SLINK_PIN, 0);
	delayMicroseconds(2400);
	digitalWrite(SLINK_PIN, 1);
	delayMicroseconds(600);

	for(unsigned i = 0 ; i < len ; i++)
		slink_send_byte(bytes[i]);

	// reset bus to high for at least several bits
	digitalWrite(SLINK_PIN, 1);
	delayMicroseconds(30000);

	// and return to an input pin
	pinMode(SLINK_PIN, INPUT);
}


void serial_read()
{
	static unsigned len = 0;
	static uint8_t buf[32];

	int c = Serial.read();
	if (c == -1)
		return;

	if(c == '\n')
	{
		Serial.print(len/2);
		Serial.println(" bytes");
		slink_send(buf, len/2);
		len = 0;
		return;
	}

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
	// check for input from the USB serial port
	serial_read();

	// check for IO on input pin
	slink_read();
}
