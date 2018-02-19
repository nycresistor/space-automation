/*
 * Turn off the Mitsubishi A/C units at NYCR.
 *
 * The IR is 36 KHz, which is non-standard compared to the normal 38 KHz.
 *
 * 1 bits are 1720us low , then modulated high for Xus
 * 0 bits are  860us low, then modulated high for Xus
 */

#define IR_INPUT_PIN 14

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


void loop()
{
/*
	const int bit = digitalRead(IR_INPUT_PIN);
		Serial.print(micros());
		Serial.print(" ");
		Serial.println(bit);
	return;
*/

	// wait for a sync pulse
	while(1)
	{
		unsigned long length = high_length();
		if (length > 3000)
			break;
		Serial.print("-");
		Serial.println(length);
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
			if (now - start > 100000)
				break;

			yield();
			continue;
		}

		// we've started another bit
		// print the time length that we've been waiting
		unsigned long delta = now - start;
		if (800 < delta && delta < 900)
			Serial.print("0 ");
		else
		if (1600 < delta && delta < 1800)
			Serial.print("1 ");
		else
			Serial.print("? ");
		Serial.println(delta);
		start = now;

		// wait for the next falling edge
		high_length();
	}
}
