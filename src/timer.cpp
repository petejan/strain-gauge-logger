
#include <Arduino.h>
#include "adc122c04.h"
#include "main.h"

//Function that is used to check if TC5 is done syncing
//returns true when it is done syncing
bool tcIsSyncing()
{
	return TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
}

//This function enables TC5 and waits for it to be ready
void tcStartCounter()
{
	// Enable the TC5 interrupt request
	if (pwrUpTime->read() > 0)
	{
		TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0 | TC_INTENSET_MC1;
	}
	else
	{
		TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
	}
	while (tcIsSyncing()); //wait until TC5 is done syncing

	TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; //set the CTRLA register
	while (tcIsSyncing()); //wait until snyc'd
}

//Reset TC5
void tcReset()
{
	TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
	while (tcIsSyncing());
	while (TC5->COUNT16.CTRLA.bit.SWRST);
}

//disable TC5
void tcDisable()
{
	TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
	while (tcIsSyncing());
}


//this function gets called by the interrupt at <sampleRate> ms
void TC5_Handler (void)
{
	// Overflow in count up mode every timer period
	if (TC5->COUNT16.INTFLAG.bit.MC0)
	{
		if ((channel_config_reg1[0] & ADS122C04_CONVERSION_MODE_CONTINUOUS) == 0)
		{
			if (adc_read_state != ADC_STOP)
			{
				sample_ready = sample_write; // last sample is now ready to read

				// missed a tick or ADC did not sample
				if (adc_read_state != WAIT_TICK)
				{
#ifdef ADAFRUIT_FEATHER_M0
					digitalWrite(MISSED_SAMPLE_PIN, 0);
#endif

					missed_sample = true;
					missed_state = adc_read_state;

					waiting_for_sample = false;
				}
				adc_read_state = WAIT_PWR_TICK;

				// for sampling < 10ms don't power off the ADC, skip directly to sampling
				if (sampleRate->read() < 10)
				{
					adc_read_state = WAIT_ADC;

					adc_setup(current_sample_channel);

					// enable fall edge when ADC sample is ready
					attachInterrupt(ADC_INT, adc_convert_isr, LOW);

					waiting_for_sample = true;
				}

				get_next_sample_buf();

				if (adc_read_state == WAIT_PWR_TICK)
				{
					// Return the SDA pin back to I2C
					PORT->Group[g_APinDescription[SCL].ulPort].PINCFG[g_APinDescription[SCL].ulPin].bit.PMUXEN = 1;
					PORT->Group[g_APinDescription[SDA].ulPort].PINCFG[g_APinDescription[SDA].ulPin].bit.PMUXEN = 1;

					digitalWrite(ADC_PWR_PIN, 1);
#ifdef ADAFRUIT_FEATHER_M0
					digitalWrite(SAMPLE_PIN, 1);
#endif
				}
			}
		}
		TC5->COUNT16.INTFLAG.bit.MC0 = 1; //Writing a 1 to INTFLAG.bit.MC0 clears the interrupt so that it will run again
	}
	// Compare 1 interrupt, used for power up delay
	if ((TC5->COUNT16.INTFLAG.bit.MC1) || (pwrUpTime->read() == 0))
	{
		if (adc_read_state == WAIT_PWR_TICK)
		{
#ifdef ADAFRUIT_FEATHER_M0
			digitalWrite(SAMPLE_PIN, 0);
#endif
			adc_read_state = WAIT_ADC;

			// i2c_master_adc_cmd(ADS122C04_RESET_CMD); // reset resets the sample count, reset only at startup

			// setup the 4 config registers
			adc_setup(current_sample_channel);

			// enable fall edge when ADC sample is ready
			attachInterrupt(ADC_INT, adc_convert_isr, LOW);

			waiting_for_sample = true;
		}

		TC5->COUNT16.INTFLAG.bit.MC1 = 1; //Writing a 1 to INTFLAG.bit.MC1 clears the interrupt so that it will run again
	}
}

/*
 *  TIMER SPECIFIC FUNCTIONS FOLLOW
 *  you shouldn't change these unless you know what you're doing
 */

// Configures the TC to generate output events at the sample frequency.
// Configures the TC in Frequency Generation mode, with an event output once
// each time the sample frequency period expires.
void tcConfigure(uint32_t sampleRate)
{
	// select the generic clock generator used as source to the generic clock multiplexer
	// use GEN2 which is on in standby, also feeds RTC from XOSC32k
	GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | GCLK_CLKCTRL_ID(GCM_TC4_TC5)) ;
	while (GCLK->STATUS.bit.SYNCBUSY);

	tcReset(); //reset TC5

	// Set Timer counter 5 Mode to 16 bits, it will become a 16bit counter ('mode1' in the datasheet)
	TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
	// Set TC5 waveform generation mode to 'match frequency'
	TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
	//set prescaler
	//the clock normally counts at the GCLK_TC frequency, but we can set it to divide that frequency to slow it down
	//you can use different prescaler divisons here like TC_CTRLA_PRESCALER_DIV1 to get a different range
	// use 2nd capture register to implement ADC power up delay
	// TODO: for sample rates < 100ms probably don't want to power down the ADC, just sample

	uint32_t pwr_time = pwrUpTime->read();
	if (sampleRate < 30000)
	{
		TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_RUNSTDBY; //it will divide GCLK_TC frequency by 1
		TC5->COUNT16.CC[0].reg = (uint16_t) ((sampleRate* 1024ul) / 1000ul) - 1;
		while (tcIsSyncing());
		TC5->COUNT16.CC[1].reg = pwr_time;
		while (tcIsSyncing());
	}
	else
	{
		TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1024 | TC_CTRLA_RUNSTDBY; //it will divide GCLK_TC frequency by 1024
		TC5->COUNT16.CC[0].reg = (uint16_t) ((sampleRate* 1024ul) / 1000ul / 1024ul) - 1;
		while (tcIsSyncing());
		TC5->COUNT16.CC[1].reg = pwr_time/1024;
		while (tcIsSyncing());
	}

	// Configure interrupt request
	NVIC_DisableIRQ(TC5_IRQn);
	NVIC_ClearPendingIRQ(TC5_IRQn);
	NVIC_SetPriority(TC5_IRQn, 1);
	NVIC_EnableIRQ(TC5_IRQn);
}

/*
 * timer.c
 *
 *  Created on: 11 Mar 2024
 *      Author: pete
 */




