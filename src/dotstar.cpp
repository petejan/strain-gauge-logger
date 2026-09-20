
#include <Arduino.h>
#include "dotstar.h"

// There is only one pixel on the board
#define NUMPIXELS 1

#ifdef ARDUINO_ITSYBITSY_M0
void dotstar_send(uint32_t led)
{
	uint32_t d, msk;
	int i;

	pinMode(PIN_DOTSTAR_DATA, OUTPUT);
	pinMode(PIN_DOTSTAR_CLK, OUTPUT);
	digitalWrite(PIN_DOTSTAR_DATA, 0);
	digitalWrite(PIN_DOTSTAR_CLK, 1);

	d = 0;
	msk = 0x80000000ul;
	for (i=0;i<32;i++)
	{
		digitalWrite(PIN_DOTSTAR_CLK, 0);

		if (d & msk)
		{
			digitalWrite(PIN_DOTSTAR_DATA, 1);
		}
		else
		{
			digitalWrite(PIN_DOTSTAR_DATA, 0);
		}

		digitalWrite(PIN_DOTSTAR_CLK, 1);

		msk = msk >> 1;
	}
	d = 0xe0000000ul | led;
	msk = 0x80000000ul;
	for (i=0;i<32;i++)
	{
		digitalWrite(PIN_DOTSTAR_CLK, 0);

		if (d & msk)
		{
			digitalWrite(PIN_DOTSTAR_DATA, 1);
		}
		else
		{
			digitalWrite(PIN_DOTSTAR_DATA, 0);
		}

		digitalWrite(PIN_DOTSTAR_CLK, 1);

		msk = msk >> 1;
	}

	d = 0xffffffffl;
	msk = 0x80000000ul;
	for (i=0;i<32;i++)
	{
		digitalWrite(PIN_DOTSTAR_CLK, 0);

		if (d & msk)
		{
			digitalWrite(PIN_DOTSTAR_DATA, 1);
		}
		else
		{
			digitalWrite(PIN_DOTSTAR_DATA, 0);
		}

		digitalWrite(PIN_DOTSTAR_CLK, 1);

		msk = msk >> 1;
	}
}
#endif
