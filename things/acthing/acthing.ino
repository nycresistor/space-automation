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
 */

#define IR_INPUT_PIN 14

static const char off_cmd[] =
"11100010" "01101001" "10110010" "01000000"
"00000000" "00000000" "00001000" "01011000"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01010011" "0";

// this is the on command for 85F, swing h+v, 3 fan
static const char on_cmd[] = 
"11100010" "01101001" "10110010" "01000000"
"00000000" "00000010" "00001000" "01011000"
"00000001" "11101111" "00000000" "00000000"
"00000000" "00000000" "00000000" "00000000"
"00000000" "01010000" "1";

void setup()
{
	pinMode(IR_INPUT_PIN, INPUT);
	Serial.begin(115200);
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

#define MODULATION_WIDTH	  12 // usec == 35.7 kHz
#define SYNC_WIDTH		3440
#define HIGH_WIDTH		 430
#define ZERO_WIDTH		 430
#define ONE_WIDTH		1320

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


void loop()
{
/*
	const int bit = digitalRead(IR_INPUT_PIN);
		Serial.print(micros());
		Serial.print(" ");
		Serial.println(bit);
	return;
*/

	// wait for a sync pulse or a command from the serial port
	while(1)
	{
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
