/*
 * adc122c04.c
 *
 *  Created on: 11 Mar 2024
 *      Author: pete
 */


#include <Arduino.h>
#include "adc122c04.h"
#include "main.h"
#include "i2c.h"

uint8_t adc_detect_input(void)
{
	// detect when the strain gauge is connected, by outputting a current on one of the bride points and seeing if it floats high

	unsigned char data_buffer[3] = {0, 0, 0};
	int32_t data;
	uint32_t timer = 0;

	digitalWrite(ADC_PWR_PIN, 1);
	delay(10);

	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_0_REG), ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_1 | ADS122C04_PGA_DISABLED);
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_1_REG), ADS122C04_DATA_RATE_20SPS | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_INTERNAL);

	// turn on the 10uA burn out current
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_2_REG), ADS122C04_BURN_OUT_CURRENT_ON | ADS122C04_IDAC_CURRENT_OFF);
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_3_REG), ADS122C04_IDAC1_DISABLED | ADS122C04_IDAC2_DISABLED);

	// start the sample
	i2c_master_adc_cmd(ADS122C04_START_CMD);

	// timeout is 60000 for 48 MHz and 6400 for 8 MHz

	while(digitalRead(ADC_INT) && (timer < 60000))
	{
		timer++;
	}
	i2c_master_adc_rdata(3, data_buffer);

	data = data_buffer[0];
	if (data > 128) // sign extend
	{
		data = data | 0xffffff00;
	}
	data |= (data << 8) + data_buffer[1];
	data |= (data << 8) + data_buffer[2];

	i2c_master_adc_cmd(ADS122C04_RESET_CMD);
	delay(10);
	digitalWrite(ADC_PWR_PIN, 0);
	delay(10);

	SerialDEBUG.printf("ADC DETECT %d timer %d : ", data, timer);

	uint8_t ret = 1;

	if (timer >= 60000)
	{
		SerialDEBUG.printf("timeout\r\n");

		ret = -1;
	}

	if (data == 8000000)
	{
		SerialDEBUG.printf("disconnected\r\n");

		ret = 0;
	}
	else
	{
		SerialDEBUG.printf("connected\r\n");
	}

	return ret;
}

void adc_setup(int ch)
{
	// i2c_master_adc_cmd(ADS122C04_RESET_CMD); // reset resets the sample count, reset only at startup

	// setup the 4 config registers
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_0_REG), channel_config_reg0[ch]);
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_1_REG), channel_config_reg1[ch]);

	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_2_REG), adc_reg2.read());
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_3_REG), adc_reg3.read());

	// start the sample
	i2c_master_adc_cmd(ADS122C04_START_CMD);
}

volatile int32_t channel_data[4];

volatile bool adc_convert = false;
void adc_convert_isr(void)
{
	uint16_t buf_len;

#ifdef ADAFRUIT_FEATHER_M0
	digitalWrite(SAMPLE_PIN, 1);
#endif

	if (adc_read_state == WAIT_ADC)
	{
		waiting_for_sample = false;

		// read data from ADC, 6 bytes <count><data2><data1><data0><CRC><CRC>
		if (sample_write_ptr)
		{
			uint8_t *p, *pSample;

			// first 'sample' in buffer is timestamp and channel/burst sample number
			if (sample_write_ptr == &data_buffer[0])
			{
				sample_write_ptr = &data_buffer[0];

				memcpy(sample_write_ptr, &rtc_regs, sizeof(rtc_regs)); // copy to first 4 bytes
				memcpy(sample_write_ptr+4, &current_sample_channel, sizeof(current_sample_channel)); // copy current channel
				memcpy(sample_write_ptr+5, &burst, sizeof(burst)); // copy burst

				sample_write_ptr += BYTES_PER_SAMPLE;
			}

			// read adc into data_buffer
			i2c_master_adc_rdata(BYTES_PER_SAMPLE, sample_write_ptr);

			// copy from sample_write_addr to channel_data
			p = (uint8_t *)&channel_data[current_sample_channel];
			pSample = sample_write_ptr + 3;
			*p++ = *pSample--;
			*p++ = *pSample--;
			*p++ = *pSample--;
			*p++ = 0;

			// sign extend the channel_data
			channel_data[current_sample_channel] = (channel_data[current_sample_channel] << 8) >> 8;

			// increment sample pointer, circular buffer
			sample_write_ptr += BYTES_PER_SAMPLE;
			if (sample_write_ptr >= &data_buffer[0] + sizeof(data_buffer))
			{
				sample_write_ptr = &data_buffer[0];
			}
		}

		// check for continuous mode
		if ((channel_config_reg1[0] & ADS122C04_CONVERSION_MODE_CONTINUOUS) != 0)
		{

			adc_read_state = WAIT_ADC;

			if (burst > 0)
			{
				burst--;
				if (burst == 0)
				{
					burst = nburst.read();
					adc_read_state = WAIT_TICK;

					// disable this interrupt now, and go back to waiting for the next sample tick
					detachInterrupt(ADC_INT);

					// start the sample
					i2c_master_adc_cmd(ADS122C04_POWERDOWN_CMD);
				}
			}
		}
		else
		{
			// select next channel next sample
			current_sample_channel ++;
			if (channel_config_reg0[current_sample_channel] != 0xff)
			{
				// sample next channel directly without waiting for the next tick
				adc_read_state = WAIT_ADC;

				adc_setup(current_sample_channel);
			}
			else
			{
				current_sample_channel = 0;

				burst--;
				if (burst == 0)
				{
					burst = nburst.read();

					adc_read_state = WAIT_TICK;

					// disable this interrupt now, and go back to waiting for the next sample tick
					detachInterrupt(ADC_INT);

					// de-power the ADC at sample rates more that 10ms
					if (sampleRate.read() >= 10)
					{
						digitalWrite(ADC_PWR_PIN, 0);

						// Return the SDA pin back to GPIO, makes sure lines are high
						// could also wait for bus IDLE
						//PORT->Group[g_APinDescription[SCL].ulPort].PINCFG[g_APinDescription[SCL].ulPin].bit.PMUXEN = 0;
						//PORT->Group[g_APinDescription[SDA].ulPort].PINCFG[g_APinDescription[SDA].ulPin].bit.PMUXEN = 0;
					}
				}
				else
				{
					// sample next channel directly without waiting for the next tick
					adc_read_state = WAIT_ADC;

					adc_setup(current_sample_channel);
				}
			}
		}

	}
	adc_convert = true;

#ifdef ADAFRUIT_FEATHER_M0
	digitalWrite(SAMPLE_PIN, 0);
#endif

}



