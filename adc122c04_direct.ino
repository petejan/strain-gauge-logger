#include "Arduino.h"
#include "ZeroRegs.h"
#include "SdFat.h"

#include <RTCZero.h>

#include <time.h>
#include <flashstorage.h>

#include "LowPower.h"

#include "adc122c04.h"

#include "wiring_private.h"

//#define SerialDEBUG SERIAL_PORT_USBVIRTUAL
#define SerialDEBUG SERIAL_PORT_HARDWARE

#define BYTES_PER_SAMPLE 3

int cmd_regs(char *line);
int cmd_help(char *line);
int cmd_time(char *line);
int cmd_debug(char *line);
int cmd_sleep(char *line);
int cmd_fcorr(char *line);
int cmd_ls(char *line);
int cmd_start(char *line);
int cmd_stop(char *line);
int cmd_starttime(char *line);
int cmd_startlater(char *line);
int cmd_set(char *line);
int cmd_adc(char *line);
int cmd_sample(char *line);
int cmd_text(char *line);
int cmd_file(char *line);
int cmd_format(char *line);
int cmd_sync(char *line);

enum RUN_MODE {RUN_MODE_STOP, RUN_MODE_RUN, RUN_MODE_START_LATER};

typedef struct {char txt[80];} char_80;

FlashStorage(adc_reg1, uint8_t);
FlashStorage(adc_reg2, uint8_t);
FlashStorage(adc_reg3, uint8_t);

FlashStorage(fcorr_set, uint8_t);
FlashStorage(debug_set, uint8_t);
FlashStorage(run_mode, RUN_MODE);
FlashStorage(datetime_start, uint32_t);
FlashStorage(sampleRate, uint32_t);
FlashStorage(deployment_text, char_80);
FlashStorage(file_name_template, char_80);


typedef struct
{
  const char * cmd_name;
  int (*cmd_handler)(char *line);
  const char * cmd_help;
} CMD;

void * findCommand(char * command);

CMD cmds[] =
{
  {"adc",        cmd_adc,        "display/set adc <rate> <gain>"},
  {"time",       cmd_time,       "display/set time <YYYY-MM-DD hh:mm:ss>"},
  {"regs",       cmd_regs,       "display all SAMD21 regs"},
  {"sleep",      cmd_sleep,      "set can sleep"},
  {"fcorr",      cmd_fcorr,      "display/set fcorr reg"},
  {"debug",      cmd_debug,      "set debug level"},
  {"ls",         cmd_ls,         "display card file list"},
  {"startnow",   cmd_start,      "start sampling now"},
  {"stop",       cmd_stop,       "start stop sampling"},
  {"starttime",  cmd_starttime,  "set/display start time"},
  {"startlater", cmd_startlater, "start sampling at starttime"},
  {"set",        cmd_set,        "display settings"},
  {"samplerate", cmd_sample,     "display/set sample rate"},
  {"text",       cmd_text,       "display/set deployment text note"},
  {"file",       cmd_file,       "display/set file name %Y-%M-%DT%H%M.BIN"},
  {"format",     cmd_format,     "format card 'format yes' to format"},
  {"sync",       cmd_sync,  	 "sync file"},
  {"help",       cmd_help,       "command help, this message"}
};

/* Create an rtc object */
RTCZero rtc;

#ifdef ADAFRUIT_FEATHER_M0
const uint8_t ADC_INT = 5;
const uint8_t PIN_WRITE_LED = 8;
const uint8_t SAMPLE_PIN = 13;
const uint8_t SLEEP_PIN = 11;
const uint8_t ADC_PWR_PIN = 10;
const uint8_t SD_CAR_DET_PIN = 7;

const uint8_t MISSED_SAMPLE_PIN = A2;

const uint8_t LED_RED_PIN = 13;
const uint8_t LED_GREEN_PIN = 8;

// SD chip select pin.
const uint8_t SD_CS_PIN = 4;

#define I2C_SERCOM SERCOM3
#endif
#ifdef ARDUINO_QTPY_M0

const uint8_t ADC_INT = A0;
const uint8_t ADC_PWR_PIN = A1;

const uint8_t SD_CAR_DET_PIN = A2;

// SD chip select pin.
const uint8_t SD_CS_PIN = A3;

//const uint8_t PIN_WRITE_LED = 8;
//const uint8_t SAMPLE_PIN = 13;
//const uint8_t SLEEP_PIN = 11;

//const uint8_t MISSED_SAMPLE_PIN = A2;

//const uint8_t LED_RED_PIN = 13;
//const uint8_t LED_GREEN_PIN = 8;

#define I2C_SERCOM SERCOM1
#endif

//------------------------------------------------------------------------------
// Note: Uno will not support SD_FAT_TYPE = 3.
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 1

// Max SPI rate for AVR is 10 MHz for F_CPU 20 MHz, 8 MHz for F_CPU 16 MHz.
#define SPI_CLOCK SD_SCK_MHZ(4)

#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)

#if SD_FAT_TYPE == 0
SdFat sd;
typedef File file_t;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
typedef File32 file_t;
#elif SD_FAT_TYPE == 2
SdExFat sd;
typedef ExFile file_t;
#elif SD_FAT_TYPE == 3
SdFs sd;
typedef FsFile file_t;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// Call back for file timestamps.  Only called for file create and sync().
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10)
{
	time_t now = rtc.getEpoch();

	struct tm t;
	gmtime_r(&now, &t);

	// Return date using FS_DATE macro to format fields.
	*date = FS_DATE(t.tm_year+1900, t.tm_mon+1, t.tm_mday);

	// Return time using FS_TIME macro to format fields.
	*time = FS_TIME(t.tm_hour, t.tm_min, t.tm_sec);

	// Return low time bits in units of 10 ms.
	*ms10 = 0;
}

void hexdump(
    Stream& stream,
    const uint8_t* const data,
    const uint32_t byteCount,
    const bool header = true,
    const char* const message = nullptr,
    const uint8_t linefeeds = 1,
    const uint16_t offset = 0)
{
    // Actual memory address where data points to is unknown so first byte is shown as address 0x0000.
    // Optionally an offset for the address labeling can be specified. This imitates as if memory starts
    // at the offset. The offset feature can be useful when having to compare a hexdump with another hexdump.

    if (header)
    {
        if (message == nullptr || strlen(message) == 0)
        {
            stream.printf("Memory hexdump - 0x%X (%u) bytes\n", byteCount, byteCount);
        }
        else
        {
            stream.printf("Memory hexdump %s - 0x%X (%u) bytes\n", message, byteCount, byteCount);
        }
    }

    if (byteCount == 0)
    {
        stream.println("Byte count is 0 (hexdump).");
        for (uint8_t i = 0; i < linefeeds; ++i)
        {
            stream.println();
        }
        return;
    }

    if (data == nullptr)
    {
        stream.println("Data nullpointer (hexdump).");
        for (uint8_t i = 0; i < linefeeds; ++i)
        {
            stream.println();
        }
        return;
    }

    if (header)
    {
        if (offset > 0)
        {
            stream.printf("Showing addresses with simulated offset 0x%02X (%u).\n", offset, offset);
        }
        stream.println();

        // Output header
        stream.print("     ");
        for (uint8_t i = 0; i < 16; ++i)
        {
            if (i == 8)
            {
                stream.print("  ");
            }
            stream.printf(" %02X", i);
        }
        stream.print("\n     ");
        for (uint8_t i = 0; i < 16; ++i)
        {
            if (i == 8)
            {
                stream.print("  ");
            }
            stream.printf(" --");
        }
        stream.println();
    }

    // Output data

    uint16_t lineNr = 0;
    uint16_t address = offset;
    if ((address & 0xF) > 0)
    {
        ++lineNr;
        // Output first adreslabel and indent
        stream.printf("%04X:", address & 0xFFF0);
        for (uint8_t i = 0; i < (offset & 0xF); ++i)
        {
            if (i == 8 && (offset & 0xF) >= 8)
            {
                stream.print("  ");
            }
            stream.print("   ");
        }
    }

    // Output byte values
    for (uint16_t i = 0; i < byteCount; ++i)
    {
        // stream.printf("[i=%u, address=0x%04X, lineNr=%u]", i, address, lineNr); //debug
        if ((address & 0xF) == 0)
        {
            ++lineNr;
            if (lineNr > 1)
            {
                stream.println();
            }
            // Write address label
            stream.printf("%04X:", address & 0xFFF0);
        }
        if ((address & 0xF) ==  8)
        {
            stream.print(" -");
        }
        stream.printf(" %02X", data[i]);
        ++address;
    }
    stream.println();
    for (uint8_t i = 0; i < linefeeds; ++i)
    {
        stream.println();
    }
}

bool rtc_alarm_flag = false;

void rtc_int()
{
	rtc_alarm_flag = true;
}

#define ADC122C04_ADDRESS 0x40

/* ADC sample state machine
 *   state ADC_STOP			: Stopped
 *         WAIT_TICK     	: enable ADC pwr
 *         WAIT_PWR_TICK    : send load CONFIG0 reg, sets channel and PGA
 *         WAIT_ADC 		: adc isr, sample finished, send ADS122C04_RDATA_CMD using TxDMA to ADC
 *         WAIT_RXDMA       : RxDMA finished, send ADS122C04_CONFIG_0_REG using TxDMA to ADC, next state WAIT_TICK
 *
 * Sample Rate set by the Timer 5 tick
 *
 */

enum STATE {ADC_STOP, WAIT_TICK, WAIT_PWR_TICK, WAIT_ADC, WAIT_RXDMA};
volatile STATE adc_read_state;

uint8_t current_sample_channel = 0;
/*
const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
									   ADS122C04_MUX_AIN2_AVSS | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
										0xff};
*/

const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_128 | ADS122C04_PGA_ENABLED,
										0xff};

/*
const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
										0xff};
*/

#define I2C_TIMEOUT 20000

volatile uint16_t i2c_timer;

int i2c_master_adc_cmd(uint8_t cmd)
{
	/* Acknowledge section is set as ACK signal by writing 0 in ACKACT bit */
	I2C_SERCOM->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	/* slave address with Write(0) */
	I2C_SERCOM->I2CM.ADDR.reg = (ADC122C04_ADDRESS << 1) | 0;

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	I2C_SERCOM->I2CM.DATA.reg = cmd;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	I2C_SERCOM->I2CM.CTRLB.bit.CMD = 0x3;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	return i2c_timer;
}

void i2c_master_adc_cmd_reg(uint8_t cmd, uint8_t reg)
{
	/* Acknowledge section is set as ACK signal by writing 0 in ACKACT bit */
	I2C_SERCOM->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	/* slave address with Write(0) */
	I2C_SERCOM->I2CM.ADDR.reg = (ADC122C04_ADDRESS << 1) | 0;

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	I2C_SERCOM->I2CM.DATA.reg = cmd;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	I2C_SERCOM->I2CM.DATA.reg = reg;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	I2C_SERCOM->I2CM.CTRLB.bit.CMD = 0x3;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
}

void i2c_master_adc_rdata(uint8_t len, uint8_t buf[])
{
	uint8_t i;

	/* Acknowledge section is set as ACK signal by writing 0 in ACKACT bit */
	I2C_SERCOM->I2CM.CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	/* slave address with Write(0) */
	I2C_SERCOM->I2CM.ADDR.reg = (ADC122C04_ADDRESS << 1) | 0;

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	I2C_SERCOM->I2CM.DATA.reg = ADS122C04_RDATA_CMD;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

	i2c_timer = I2C_TIMEOUT;
	while ((!I2C_SERCOM->I2CM.INTFLAG.bit.MB) && (i2c_timer > 0)) { i2c_timer--; };

	/* slave address with Read(1) */
	I2C_SERCOM->I2CM.ADDR.reg = (ADC122C04_ADDRESS << 1) | 1;

	i = 0;
	while (len > 0)
	{
		i2c_timer = I2C_TIMEOUT;
		while ((!I2C_SERCOM->I2CM.INTFLAG.bit.SB) && (i2c_timer > 0)) { i2c_timer--; };

		buf[i] = I2C_SERCOM->I2CM.DATA.reg;

		i++;
		len--;

		if (len == 0)
		{
			/* Acknowledge section is set as NACK signal by writing 1 in ACKACT bit */
			I2C_SERCOM->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_ACKACT;
			while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
		}
	}

	I2C_SERCOM->I2CM.CTRLB.bit.CMD = 0x3;
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
}



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
	TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0 | TC_INTENSET_MC1;
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

volatile bool waiting_for_sample = false;
volatile int16_t sample_write = -1;
volatile int16_t sample_ready = -BYTES_PER_SAMPLE;

//uint8_t data_buffer[512 * BYTES_PER_SAMPLE * 2];
uint8_t data_buffer[512 * 6 * 2];
//uint8_t data_buffer[32 * BYTES_PER_SAMPLE * 2];

time_t now;
struct tm now_tm;

bool missed_sample = false;
STATE missed_state;

bool run_sample = false;

uint8_t *sample_write_addr;

void adc_setup(int ch)
{
	// i2c_master_adc_cmd(ADS122C04_RESET_CMD); // reset resets the sample count, reset only at startup

	// setup the 4 config registers
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_0_REG), channel_config_reg0[ch]);
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_1_REG), adc_reg1.read());

	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_2_REG), adc_reg2.read());
	i2c_master_adc_cmd_reg(ADS122C04_WRITE_CMD(ADS122C04_CONFIG_3_REG), adc_reg3.read());

	// start the sample
	i2c_master_adc_cmd(ADS122C04_START_CMD);
}

uint32_t rtc_regs;

void get_next_sample_buf(void)
{
	uint16_t buf_len;

	if (sample_write >= 0)
	{
		sample_write_addr = &data_buffer[sample_write];
		memset(sample_write_addr, 0, BYTES_PER_SAMPLE); // clear buffer for next sample
	}

	if (sample_write <= 0) // first sample data is RTC, LEN
	{
		if (run_sample)
		{
			sample_write = 0;
			sample_write_addr = &data_buffer[sample_write];

//			// first sample in block is RTC time

			// RTC read blocks for 5 ms for sync due to different clock domains
//		    RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
//		    while (RTC->MODE2.STATUS.bit.SYNCBUSY);
//			rtc_regs = RTC->MODE2.CLOCK.reg; // REG is year/month/day/hour/min/sec

			memcpy(sample_write_addr, &rtc_regs, sizeof(rtc_regs)); // copy to first 4 bytes
			buf_len = sizeof(data_buffer);
			memcpy(sample_write_addr+4, &buf_len, sizeof(buf_len)); // copy to length

			sample_write = BYTES_PER_SAMPLE;
			sample_write_addr = &data_buffer[sample_write];
			memset(sample_write_addr, 1, BYTES_PER_SAMPLE); // clear buffer for next sample

			current_sample_channel = 0; // always start a new block at channel 0
		}
		else
		{
			adc_read_state = ADC_STOP;

			sample_write_addr = NULL;

			sample_write = 0;
		}
	}
	if (adc_read_state != ADC_STOP)
	{
		sample_write += BYTES_PER_SAMPLE;
		if (sample_write >= sizeof(data_buffer))
		{
			sample_write = 0;
		}
	}

}
//this function gets called by the interrupt at <sampleRate> ms
void TC5_Handler (void)
{
	// Overflow in count up mode every timer period
	if (TC5->COUNT16.INTFLAG.bit.MC0)
	{
		if ((adc_reg1.read() & ADS122C04_CONVERSION_MODE_CONTINUOUS) == 0)
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
				if (sampleRate.read() < 10)
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
	if (TC5->COUNT16.INTFLAG.bit.MC1)
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
void tcConfigure(int sampleRate)
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
	TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_RUNSTDBY; //it will divide GCLK_TC frequency by 1
	//TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1024 | TC_CTRLA_ENABLE | TC_CTRLA_RUNSTDBY; //it will divide GCLK_TC frequency by 1024
	//set the compare-capture register.
	//The counter will count up to this value (it's a 16bit counter so we use uint16_t)
	//this is how we fine-tune the frequency, make it count to a lower or higher value
	// clock is 32768/32 = 1024 Hz
	TC5->COUNT16.CC[0].reg = (uint16_t) (((uint32_t)(sampleRate - 1)* 1024ul) / 1000ul);
	while (tcIsSyncing());

	// use 2nd capture register to implement ADC power up delay
	// TODO: for sample rates < 100ms probably don't want to power down the ADC, just sample
	TC5->COUNT16.CC[1].reg = 3; // 3 ms power up delay
	while (tcIsSyncing());

	// Configure interrupt request
	NVIC_DisableIRQ(TC5_IRQn);
	NVIC_ClearPendingIRQ(TC5_IRQn);
	NVIC_SetPriority(TC5_IRQn, 1);
	NVIC_EnableIRQ(TC5_IRQn);
}

/* I2C master initialization */
void i2c_master_init()
{
	/* By setting the SPEED bit field as 0x01, I2C Master runs at SM or FM,
	   By setting the SDAHOLD bit field as 0x0, SDA hold time disabled,
	   I2C_SERCOM is configured as an I2C Master by writing the MODE bitfield as 0x5 */

	I2C_SERCOM->I2CM.CTRLA.reg = SERCOM_I2CM_CTRLA_SPEED (0) |
			SERCOM_I2CM_CTRLA_SDAHOLD(0x0) |
			SERCOM_I2CM_CTRLA_MODE_I2C_MASTER;

	/* smart mode enabled by setting the bit SMEN as 1 */
	I2C_SERCOM->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;
	/* synchronization busy */
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
	/* BAUDLOW is non-zero, and baud register is loaded */
#if F_CPU == 48000000L
	I2C_SERCOM->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(50) | SERCOM_I2CM_BAUD_BAUDLOW(0);
#elif F_CPU == 8000000L
	I2C_SERCOM->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(5) | SERCOM_I2CM_BAUD_BAUDLOW(0); // 400 kHz
#endif
	/* synchronization busy */
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
	/* I2C_SERCOM peripheral enabled by setting the ENABLE bit as 1*/
	I2C_SERCOM->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;
	/* SERCOM Enable synchronization busy */
	while((I2C_SERCOM->I2CM.SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_ENABLE));
	/* bus state is forced into idle state */
	I2C_SERCOM->I2CM.STATUS.bit.BUSSTATE = 0x1;
	/* synchronization busy */
	while(I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
}

uint8_t tx_buf[8];
#define BUF_SIZE sizeof(tx_buf)

volatile int16_t sample_read = -BYTES_PER_SAMPLE;
volatile int16_t sample_write_sd = -1;

uint8_t hdr[512];

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
		if (sample_write_addr)
		{
			i2c_master_adc_rdata(BYTES_PER_SAMPLE, sample_write_addr);
		}

		// check for continuous mode
		if ((adc_reg1.read() & ADS122C04_CONVERSION_MODE_CONTINUOUS) != 0)
		{
			sample_ready = sample_write; // last sample is now ready to read

			adc_read_state = WAIT_ADC;

			get_next_sample_buf();
		}
		else
		{
			// select next channel next sample
			current_sample_channel ++;
			if (channel_config_reg0[current_sample_channel] != 0xff)
			{
				// sample next channel directly without waiting for the next tick
				adc_read_state = WAIT_ADC;
				get_next_sample_buf();

				adc_setup(current_sample_channel);
			}
			else
			{
				current_sample_channel = 0;

				adc_read_state = WAIT_TICK;

				// disable this interrupt now, and go back to waiting for the next sample tick
				detachInterrupt(ADC_INT);

				// de-power the ADC at sample rates more that 10ms
				if (sampleRate.read() >= 10)
				{
					digitalWrite(ADC_PWR_PIN, 0);
				}
			}
		}

	}
	adc_convert = true;

#ifdef ADAFRUIT_FEATHER_M0
	digitalWrite(SAMPLE_PIN, 0);
#endif

}

bool can_sleep = false;
uint8_t can_sleep_count = 20;

uint8_t debug = 0;

bool event_sys_flag = false;

void EVSYS_Handler(void)
{
	event_sys_flag = true;

	EVSYS->INTFLAG.bit.EVD0 = 1; // clear flag
}

bool card_det_flag = true; // force update on reset

void card_det_isr(void)
{
	card_det_flag = true;
}

char file_name[80];
char_80 file_name_template_mem;

char line[80];
file_t binFile;

//The setup function is called once at startup of the sketch
void setup()
{
	rtc.begin(); // initialize RTC

	SYSCTRL->XOSC32K.reg |= SYSCTRL_XOSC32K_RUNSTDBY;

	/* enable the event system and the RTC event PERE7, this wakes every second, for long sample times this makes sure it
	 * wakes occasionally. Probably draws some port, although only 13 ms every second
	 */
    RTC->MODE2.CTRL.reg &= ~RTC_MODE2_CTRL_ENABLE; // disable RTC
    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

	RTC->MODE2.EVCTRL.bit.PEREO7 = 1; // bit is enable protected, enable per7, 1 / second

	RTC->MODE2.CTRL.reg |= RTC_MODE2_CTRL_ENABLE; // enable RTC
    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

    PM->APBCMASK.reg |= PM_APBCMASK_EVSYS; // Switch on the event system peripheral

    // enable event sys clock
    GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | (EVSYS_GCLK_ID_0 << GCLK_CLKCTRL_ID_Pos)));
    while (GCLK->STATUS.bit.SYNCBUSY);

    EVSYS->CHANNEL.reg = EVSYS_CHANNEL_CHANNEL(0) | EVSYS_CHANNEL_PATH_RESYNCHRONIZED | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_RTC_PER_7) | EVSYS_CHANNEL_EDGSEL_RISING_EDGE; // RTC PER6
    EVSYS->INTENSET.bit.EVD0 = 1;

	// Configure interrupt request
	NVIC_DisableIRQ(EVSYS_IRQn);
	NVIC_ClearPendingIRQ(EVSYS_IRQn);
	NVIC_SetPriority(EVSYS_IRQn, 1);
	NVIC_EnableIRQ(EVSYS_IRQn);

	/* disable USB, might be enabled from startup, running at 8 MHz clock, because 48 MHz clock draws power.
	 * 48 MHz is needed for USB clock
	 */

	PM->AHBMASK.bit.USB_ = 0; // disable USB clock
	USB->DEVICE.CTRLA.reg &= ~(USB_CTRLA_RUNSTDBY | USB_CTRLA_ENABLE); // disable USB

#ifdef ARDUINO_QTPY_M0
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM1; // Switch on the I2C_SERCOM (I2C) clock

    // enable I2C_SERCOM sys clock
    GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | (SERCOM1_GCLK_ID_CORE << GCLK_CLKCTRL_ID_Pos)));
#endif
#ifdef ADAFRUIT_FEATHER_M0
	/* implement IIC here, not using Wire library, as library is interrupt driven, and we use an interrupt to read/write I2C,
	 * improves sample timing jitter.
	 */
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3; // Switch on the I2C_SERCOM (I2C) clock

    // enable I2C_SERCOM sys clock
    GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | (SERCOM3_GCLK_ID_CORE << GCLK_CLKCTRL_ID_Pos)));
#endif
    while (GCLK->STATUS.bit.SYNCBUSY);

    pinPeripheral(SCL, PIO_SERCOM);
    pinPeripheral(SDA, PIO_SERCOM);

    i2c_master_init();

	SerialDEBUG.begin(115200); // start serial communication at 115200bps

	SerialDEBUG.println(F("ADC122C04 direct " __DATE__ " " __TIME__));

	volatile uint32_t val1, val2, val3, val4;
	volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
	val1 = *ptr1;
	volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
	val2 = *ptr;
	ptr++;
	val3 = *ptr;
	ptr++;
	val4 = *ptr;

	SerialDEBUG.print("chip id: 0x");
	char buf[33];
	sprintf(buf, "%08x %08x %08x %08x", val1, val2, val3, val4);
	SerialDEBUG.println(buf);

	pinMode(SD_CAR_DET_PIN, INPUT_PULLUP);
	pinMode(ADC_INT, INPUT_PULLUP);

	pinMode(SD_CS_PIN, OUTPUT);
#ifdef ADAFRUIT_FEATHER_M0
	pinMode(PIN_WRITE_LED, OUTPUT);
	pinMode(SAMPLE_PIN, OUTPUT);
	pinMode(SLEEP_PIN, OUTPUT);
	pinMode(MISSED_SAMPLE_PIN, OUTPUT);

	pinMode(LED_RED_PIN, OUTPUT);
	pinMode(LED_GREEN_PIN, OUTPUT);
	digitalWrite(LED_RED_PIN, 0);
	digitalWrite(LED_GREEN_PIN, 0);

	digitalWrite(MISSED_SAMPLE_PIN, 1);
#endif

	pinMode(ADC_PWR_PIN, OUTPUT);
	digitalWrite(ADC_PWR_PIN, 1); // start powered up

	SerialDEBUG.print(F("SystemClock "));
	SerialDEBUG.println(SystemCoreClock);

	// attachInterrupt(ADC_INT, adc_convert_isr, FALLING);
	attachInterrupt(SD_CAR_DET_PIN, card_det_isr, CHANGE);

    // Set callback
    FsDateTime::setCallback(dateTime);

    // Setup the RTC frequency correction
	SerialDEBUG.print(F("fcorr "));

	RTC->MODE2.FREQCORR.reg = fcorr_set.read();
	SerialDEBUG.println(fcorr_set.read());

	// Setup the sample rate, default to 1 sample/second (1000 ms)
	if (sampleRate.read() == 0)
	{
		sampleRate.write(150);
	}

	// tcConfigure(sampleRate.read()); //configure the timer to run at <sampleRate>Hertz
	// tcStartCounter(); //starts the timer

	debug = debug_set.read();

	// if mode is start-later, load with start time, otherwise alarm every minute
	if (run_mode.read() == RUN_MODE_START_LATER)
	{
		time_t start_time = datetime_start.read();
		tm tm_elements;

		gmtime_r(&start_time, &tm_elements);

		rtc.setAlarmTime(tm_elements.tm_hour, tm_elements.tm_min, tm_elements.tm_sec);
		rtc.setAlarmDate(tm_elements.tm_mday, tm_elements.tm_mon + 1, tm_elements.tm_year + 1900 - 2000);

		rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);

		SerialDEBUG.print(F("Start Later Mode : "));
		print_date(&tm_elements);
		SerialDEBUG.println();
	}
	else
	{
		rtc.setAlarmTime(0, 0, 0);
		rtc.enableAlarm(rtc.MATCH_SS);
	}

	// RTC alarm
	rtc.attachInterrupt(rtc_int);

	if (adc_reg1.read() == 0)
	{
		adc_reg1.write(ADS122C04_DATA_RATE_20SPS | ADS122C04_OP_MODE_NORMAL | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_EXT_REF_PINS | ADS122C04_TEMP_SENSOR_OFF);

#if BYTES_PER_SAMPLE == 6
		adc_reg2.write(ADS122C04_DCNT_ENABLE | ADS122C04_CRC_CRC16_ENABLED | ADS122C04_BURN_OUT_CURRENT_OFF | ADS122C04_IDAC_CURRENT_OFF);
#elif BYTES_PER_SAMPLE == 5
		adc_reg2.write(ADS122C04_CRC_CRC16_ENABLED | ADS122C04_BURN_OUT_CURRENT_OFF | ADS122C04_IDAC_CURRENT_OFF);
#elif BYTES_PER_SAMPLE == 3
		adc_reg2.write(ADS122C04_BURN_OUT_CURRENT_OFF | ADS122C04_IDAC_CURRENT_OFF);
#endif
		adc_reg3.write(ADS122C04_IDAC1_DISABLED | ADS122C04_IDAC2_DISABLED);
	}
	SerialDEBUG.print(F("ADC REG1 : 0x"));
	SerialDEBUG.println(adc_reg1.read(), HEX);

	if (i2c_master_adc_cmd(ADS122C04_RESET_CMD) == 0)
	{
		SerialDEBUG.println("ERROR: ADC did not respond");
	}

	/* setup the logging file */
	file_name_template.read(&file_name_template_mem);

	if (strlen((char *)file_name_template_mem.txt) == 0)
	{
		const char file_name_def[80] = "SGL-%Y-%m-%d-T%H.BIN";
		char_80 *p;
		p = (char_80 *)file_name_def;

		file_name_template.write(*p);
	}
	file_name_template.read(&file_name_template_mem);
	SerialDEBUG.print("file template : ");
	SerialDEBUG.println(file_name_template_mem.txt);

	/* read setup commands from file */

	int det = digitalRead(SD_CAR_DET_PIN);

	if (det)
	{
		if (!sd.begin(SD_CONFIG))
		{
			SerialDEBUG.println("sd.begin failed");
		}
		else
		{
			SerialDEBUG.print("Fat Type : ");
			SerialDEBUG.println(sd.vol()->fatType());
		}
	}
	else
	{
		SerialDEBUG.println("no card");
	}

	int n;
	binFile = sd.open("CONFIG.TXT", FILE_READ);

	// check for open error
	if (binFile.isOpen())
	{
		SerialDEBUG.println();
		SerialDEBUG.println("CONFIG.TXT file .... ");

		// read lines from the file
		while ((n = binFile.fgets(line, sizeof(line))) > 0)
		{
			if (line[n - 1] == '\n')
			{
				// remove '\n'
				line[n-1] = 0;

				SerialDEBUG.print("file line : ");
				// replace next line with LCD call to display line
				SerialDEBUG.println(line);

				executeCommand(line);
			}
			else
			{
				// no '\n' - line too long or missing '\n' at EOF
				// handle error
			}
		}

		binFile.close();

		SerialDEBUG.println("... end CONFIG.TXT file");
		SerialDEBUG.println();
	}
	else
	{
		SerialDEBUG.println("could not open CONFIG.TXT file");
	}

	/* Sampling is started in main loop */
	if ((run_mode.read() == RUN_MODE_RUN) || ((run_mode.read() == RUN_MODE_START_LATER) && (now >= datetime_start.read())))
	{
		run_sample = true;
	}
	else
	{
		run_sample = false;
	}

	//ZeroRegOptions opts = { SerialDEBUG, false };
	//printZeroRegs(opts);

	// switch EIC clock to GEN02 so it works in sleep mode
    GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | GCLK_CLKCTRL_ID(GCM_EIC));

//    for (int i=0;i<PERIPH_COUNT_IRQn;i++)
//    {
//    	NVIC_SetPriority((enum IRQn)i, 1);
//    }
//    NVIC_SetPriority(EIC_IRQn, 0);
}

int line_idx = 0;

void print2digits(int number)
{
	if (number < 10)
	{
	  SerialDEBUG.print(F("0")); // print a 0 before if the number is < than 10
	}
	SerialDEBUG.print(number);
}

void print_date(tm *t)
{
	// Print date...
	print2digits(t->tm_year + 1900);
	SerialDEBUG.print(F("-"));
	print2digits(t->tm_mon+1);
	SerialDEBUG.print(F("-"));
	print2digits(t->tm_mday);
	SerialDEBUG.print(F(" "));

	// ...and time
	print2digits(t->tm_hour);
	SerialDEBUG.print(F(":"));
	print2digits(t->tm_min);
	SerialDEBUG.print(F(":"));
	print2digits(t->tm_sec);
}

void open_bin_file(char *new_name)
{
	char name_tmp[80];

	bool write_hdr = false;

	if (binFile.isOpen())
	{
		binFile.getName(name_tmp, sizeof(name_tmp));

		binFile.sync();
		if (strcmp(new_name, name_tmp) != 0)
		{
			SerialDEBUG.println(F("file was open, create new file"));

			binFile.close();

			binFile = sd.open(new_name, FILE_WRITE);

			can_sleep = false;
			can_sleep_count = 5;
		}
	}
	else
	{
		SerialDEBUG.println(F("file not open, append"));

		binFile = sd.open(new_name, FILE_WRITE);

		binFile.getSFN(name_tmp, sizeof(name_tmp));
		SerialDEBUG.print("short file name after open : ");
		SerialDEBUG.println(name_tmp);

		can_sleep = false;
		can_sleep_count = 5;
	}

	SerialDEBUG.print(F("file pos : "));
	SerialDEBUG.println(binFile.position());

	// write header to start of new file
	if (binFile.isOpen() && (binFile.position() == 0))
	{
		write_hdr = true;
	}

	/* did the file open, if not try and restart the SD card */
	/* this can also happen if the file name clashes with one on the card already */

	if (!binFile.isOpen())
	{
		SerialDEBUG.print(F("open failed : "));
		SerialDEBUG.println(new_name);

		int det = digitalRead(SD_CAR_DET_PIN);

		if (det)
		{
			// try and close file, and re-open the SD card
			sd.end();

			if (!sd.begin(SD_CONFIG))
			{
				SerialDEBUG.println("sd.begin failed");
			}
			else
			{
				SerialDEBUG.print("Fat Type : ");
				SerialDEBUG.println(sd.vol()->fatType());
			}
		}
		else
		{
			SerialDEBUG.println("no card");
		}
	}

	/* for a new file, write the file header to the start of the file */
	if (write_hdr)
	{
		char *p = (char *)hdr;
		int l;
		int ad_ch = 0;

		memset(hdr, 0, sizeof(hdr));
		p += sprintf(p, "adc122c04 " __DATE__ " " __TIME__ "\r\n");
		p += sprintf(p, "sample rate = %d (ms)\r\n", sampleRate.read());
		p += sprintf(p, "bytes per sample = \r\n", BYTES_PER_SAMPLE);
		p += sprintf(p, "ADC REG1 = 0x%02x\r\n", adc_reg1.read());
		while (channel_config_reg0[ad_ch] != 0xff)
		{
			p += sprintf(p, "ADC Channel %2d = 0x%02x\r\n", ad_ch, channel_config_reg0[ad_ch]);

			ad_ch++;
		}
		char_80 txt;
		deployment_text.read(&txt);
		p += sprintf(p, "deployment text : %s\r\n", txt.txt);

		volatile uint32_t val1, val2, val3, val4;
		volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
		val1 = *ptr1;
		volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
		val2 = *ptr;
		ptr++;
		val3 = *ptr;
		ptr++;
		val4 = *ptr;

		p += sprintf(p, "chip id : 0x%08x %08x %08x %08x\r\n", val1, val2, val3, val4);

		SerialDEBUG.println(F("Write Hdr:"));
		SerialDEBUG.println(F("-------------------"));
		SerialDEBUG.print((char *)hdr);
		SerialDEBUG.println(F("-------------------"));

		binFile.write(hdr, sizeof(hdr));
		binFile.sync();
	}

	strcpy(file_name, new_name);

	if (binFile.isOpen())
	{
		SerialDEBUG.print("File open : ");
		SerialDEBUG.println(file_name);
	}
}

STATE last_state = ADC_STOP;

#define num_commands sizeof(cmds)/sizeof(cmds[0])

void * findCommand(char * command)
{
	//SerialDEBUG.println("find command");

	if(!command || strlen(command) == 0)
	{
		return NULL;
	}

	for(int i=0; i<num_commands; i++)
	{
		//Search by name
		if(!strcmp(command, cmds[i].cmd_name))
		{
			return &cmds[i];
		}
	}

	return NULL;
}

int executeCommand(char *line)
{
	char *tok = strtok(line, " ");

	CMD * c = (CMD *)findCommand(tok);

	if (c)
	{
		tok = strtok(NULL, " ");

		return (*c->cmd_handler)(tok);
	}

	Serial.println(F("Invalid command. Type \"help\" for more."));

	return 0;
}

int cmd_help(char *line)
{
	SerialDEBUG.println(F("help command:"));

	for(int i=0; i<num_commands; i++)
	{
		SerialDEBUG.print(cmds[i].cmd_name);
		SerialDEBUG.print(" : ");
		SerialDEBUG.println(cmds[i].cmd_help);
	}

	return 0;
}

int cmd_regs(char *line)
{
	ZeroRegOptions opts = { SerialDEBUG, false };
	printZeroRegs(opts);

	//opts.showDisabled = true;
	//printZeroRegDMAC(opts);

	return 0;
}

int cmd_adc(char *line)
{
	int new_reg1 = adc_reg1.read();
	SerialDEBUG.print("adc_reg1 ");

	if (line)
	{
	    sscanf(line, "%d", &new_reg1);

		adc_reg1.write(new_reg1);
	}

	new_reg1 = adc_reg1.read();
	SerialDEBUG.println(new_reg1);

	SerialDEBUG.print(" Mode : ");
	SerialDEBUG.println(new_reg1 & ADS122C04_OP_MODE_TURBO ? "Turbo" : "Normal");
	SerialDEBUG.print(" CM : ");
	SerialDEBUG.println(new_reg1 & ADS122C04_CONVERSION_MODE_CONTINUOUS ? "Continuous" : "Single");
	SerialDEBUG.print(" VREF : ");
	switch ((new_reg1 & 0x6) >> 1)
	{
		case 0:
			SerialDEBUG.println("Internal");
			break;
		case 1:
			SerialDEBUG.println("External");
			break;
		case 2:
			SerialDEBUG.println("AVdd/AVss");
			break;
	}
	// TODO: update for turbo mode rates (x2)
	const int data_rate[] = {20, 45, 90, 175, 330, 600, 1000};
	SerialDEBUG.print(" data rate : ");
	int dr = data_rate[(new_reg1 >> 5)];
	if (new_reg1 & ADS122C04_OP_MODE_TURBO)
	{
		dr = dr * 2;
	}
	SerialDEBUG.print(dr);
	SerialDEBUG.println(" SPS");

	SerialDEBUG.print(" Temp Sensor : ");
	SerialDEBUG.println(new_reg1 & ADS122C04_TEMP_SENSOR_ON);

	int new_reg2 = adc_reg2.read();
	SerialDEBUG.print("adc_reg2 ");
	SerialDEBUG.println(new_reg2);

	int new_reg3 = adc_reg3.read();
	SerialDEBUG.print("adc_reg3 ");
	SerialDEBUG.println(new_reg3);

	// Channels
	int ad_ch = 0;
	while (channel_config_reg0[ad_ch] != 0xff)
	{
		SerialDEBUG.print("channel ");
		SerialDEBUG.print(ad_ch);
		SerialDEBUG.print(" reg0 0x");
		SerialDEBUG.println(channel_config_reg0[ad_ch], HEX);

		ad_ch++;
	}

	return 0;
}

int cmd_start(char *line)
{
	run_mode.write(RUN_MODE_RUN);

	SerialDEBUG.println("set run mode");

	return 0;
}

int cmd_stop(char *line)
{
	run_mode.write(RUN_MODE_STOP);

	SerialDEBUG.println("set stop mode");

	return 0;
}

int cmd_startlater(char *line)
{
	time_t start_time = datetime_start.read();
	tm tm_elements;
	unsigned long delta;

	gmtime_r(&start_time, &tm_elements);

	run_mode.write(RUN_MODE_START_LATER);

	rtc.disableAlarm();

	rtc.setAlarmTime(tm_elements.tm_hour, tm_elements.tm_min, tm_elements.tm_sec);
	rtc.setAlarmDate(tm_elements.tm_mday, tm_elements.tm_mon + 1, tm_elements.tm_year + 1900 - 2000);

	rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);

	SerialDEBUG.println("set start_later mode");

	// output time when its doing to start

	now = rtc.getEpoch();
	delta = start_time - now;

	SerialDEBUG.print("start in ");
	SerialDEBUG.print(delta);
	SerialDEBUG.print(" (seconds) ");
	SerialDEBUG.print(delta/3600);
	SerialDEBUG.print(" (hours) ");
	SerialDEBUG.print(delta/3600/24);
	SerialDEBUG.println(" (days)");

	return 0;
}

int cmd_starttime(char *line)
{
	time_t start_time;
	tm tm_elements;

	if (line)
	{
		int yr, mnth, d, h, m, s;

		sscanf( line, "%4d-%2d-%2d", &yr, &mnth, &d);

		line = strtok(NULL, " ");

		sscanf( line, "%2d:%2d:%2d", &h, &m, &s);

		SerialDEBUG.print(F("Set start time : "));
		SerialDEBUG.print(yr, DEC);
		SerialDEBUG.print('-');
		SerialDEBUG.print(mnth, DEC);
		SerialDEBUG.print('-');
		SerialDEBUG.print(d, DEC);
		SerialDEBUG.print(' ');
		SerialDEBUG.print(h, DEC);
		SerialDEBUG.print(':');
		SerialDEBUG.print(m, DEC);
		SerialDEBUG.print(':');
		SerialDEBUG.print(s, DEC);
		SerialDEBUG.println();

		tm_elements.tm_year = yr - 1900;
		tm_elements.tm_mon = mnth - 1;
		tm_elements.tm_mday = d;
		tm_elements.tm_hour = h;
		tm_elements.tm_min = m;
		tm_elements.tm_sec = s;

		start_time = mktime(&tm_elements);

		datetime_start.write(start_time);
	}

	start_time = datetime_start.read();

	gmtime_r(&start_time, &tm_elements);

	SerialDEBUG.print("Start time : ");
	print_date(&tm_elements);
	SerialDEBUG.println();

	return 0;
}

time_t last_set_time;

int cmd_time(char *line)
{
	struct tm t;
	time_t now, set_time;

	if (line)
	{
		SerialDEBUG.print(F("args : "));
		SerialDEBUG.print(line);

		int yr, mnth, d, h, m, s;

		sscanf( line, "%4d-%2d-%2d", &yr, &mnth, &d);

		line = strtok(NULL, " ");
		SerialDEBUG.print(" ");
		SerialDEBUG.println(line);

		sscanf( line, "%2d:%2d:%2d", &h, &m, &s);

		SerialDEBUG.print(F("Set time : "));
		SerialDEBUG.print(yr, DEC);
		SerialDEBUG.print('-');
		SerialDEBUG.print(mnth, DEC);
		SerialDEBUG.print('-');
		SerialDEBUG.print(d, DEC);
		SerialDEBUG.print(' ');
		SerialDEBUG.print(h, DEC);
		SerialDEBUG.print(':');
		SerialDEBUG.print(m, DEC);
		SerialDEBUG.print(':');
		SerialDEBUG.print(s, DEC);
		SerialDEBUG.println();

		now = rtc.getEpoch();

		rtc.setSeconds(0); // to stop roll over

		rtc.setDate(d, mnth, yr-2000);
		rtc.setTime(h, m, s);

		t.tm_isdst = -1;
		t.tm_yday = 0;
		t.tm_wday = 0;
		t.tm_year = yr-2000+100;
		t.tm_mon = mnth - 1;
		t.tm_mday = d;
		t.tm_hour = h;
		t.tm_min = m;
		t.tm_sec = s;

		set_time = mktime(&t);
		gmtime_r(&now, &t);

		print_date(&t);

		SerialDEBUG.print(" time difference = ");
		SerialDEBUG.println(set_time - now);

		SerialDEBUG.print("last set time = ");
		SerialDEBUG.println(set_time - last_set_time, DEC);

		last_set_time = set_time;
	}

	now = rtc.getEpoch();
	gmtime_r(&now, &t);

	print_date(&t);
	SerialDEBUG.println();

	return 0;
}

int cmd_ls(char *line)
{
	bool det = digitalRead(SD_CAR_DET_PIN);

	File32 dir;
	File32 file;

	if (det)
	{
		SerialDEBUG.println(F("card detected"));
	}
	else
	{
		SerialDEBUG.println(F("no card detected"));
	}

	uint8_t ft = sd.vol()->fatType();

	SerialDEBUG.print(F("fattype : "));
	SerialDEBUG.println(ft);

	  // Open root directory
	  if (!dir.open("/"))
	  {
		  SerialDEBUG.println("dir.open failed");
	  }
	  // Open next file in root.
	  // Warning, openNext starts at the current position of dir so a
	  // rewind may be necessary in your application.
	  while (file.openNext(&dir, O_RDONLY))
	  {
	    file.printFileSize(&SerialDEBUG);
	    SerialDEBUG.write(' ');
	    file.printModifyDateTime(&SerialDEBUG);
	    SerialDEBUG.write(' ');
	    file.printSFN(&SerialDEBUG);
	    SerialDEBUG.write(' ');
	    file.printName(&SerialDEBUG);
	    if (file.isDir())
	    {
	      // Indicate a directory.
	    	SerialDEBUG.write('/');
	    }
	    SerialDEBUG.println();
	    file.close();
	  }
	  if (dir.getError())
	  {
		  SerialDEBUG.println("openNext failed");
	  }
	  else
	  {
		  SerialDEBUG.println("Done!");
	  }

//	if (ft != 0)
//	{
//		SerialDEBUG.println(F("ls:"));
//		sd.ls(&SerialDEBUG, LS_DATE | LS_SIZE);
//	}

	return 0;
}

int cmd_debug(char *line)
{
	int new_debug = debug;
	SerialDEBUG.print("debug ");

	if (line)
	{
	    sscanf(line, "%d", &new_debug);

		debug = new_debug;

		debug_set.write(debug);
	}

	SerialDEBUG.println(debug);

	return 0;
}

int cmd_sleep(char *line)
{
	can_sleep = true;

	SerialDEBUG.print("can sleep ");
	SerialDEBUG.println(can_sleep);

	return 0;
}

int cmd_fcorr(char *line)
{
	int new_fcorr = RTC->MODE2.FREQCORR.reg;
	SerialDEBUG.print("fcorr ");

	if (line)
	{
	    sscanf(line, "%d", &new_fcorr);
		RTC->MODE2.FREQCORR.reg = (uint8_t)new_fcorr;

		fcorr_set.write(new_fcorr);
	}

	SerialDEBUG.println(new_fcorr);

	return 0;
}

int cmd_sample(char *line)
{
	int new_sampleRate = sampleRate.read();
	SerialDEBUG.print("sample rate ");

	if (line)
	{
	    sscanf(line, "%d", &new_sampleRate);

	    if (new_sampleRate >= 2)
	    {
			sampleRate.write(new_sampleRate);

			tcDisable();
			tcConfigure(new_sampleRate);
			tcStartCounter();
	    }
	    else
	    {
	    	SerialDEBUG.println("minimum sample rate is 2 ms");
	    }
	}

	SerialDEBUG.println(new_sampleRate);

	return 0;
}

int cmd_set(char *line)
{
	SerialDEBUG.println();
	SerialDEBUG.print(__FILE__);
	SerialDEBUG.print(" ");
	SerialDEBUG.print(__DATE__);
	SerialDEBUG.print(" ");
	SerialDEBUG.println(__TIME__);
	SerialDEBUG.println();

	time_t start_time = datetime_start.read();
	tm tm_elements;

	gmtime_r(&start_time, &tm_elements);

	SerialDEBUG.print("Start time : ");
	print_date(&tm_elements);
	SerialDEBUG.println();

	now = rtc.getEpoch();

	gmtime_r(&now, &now_tm);

	SerialDEBUG.print("clock : ");
	print_date(&now_tm);
	SerialDEBUG.println();

	SerialDEBUG.print("mode : ");
	SerialDEBUG.println(run_mode.read());

	SerialDEBUG.print("fcorr : ");
	SerialDEBUG.println(fcorr_set.read());

	SerialDEBUG.print("can sleep : ");
	SerialDEBUG.println(can_sleep);

	SerialDEBUG.print("debug : ");
	SerialDEBUG.println(debug);

	SerialDEBUG.print("current file : ");
	SerialDEBUG.println(file_name);

	SerialDEBUG.print("sample rate : ");
	SerialDEBUG.print(sampleRate.read());
	SerialDEBUG.println(" ms ");

	return 0;
}

int cmd_text(char *line)
{
	char_80 txt;
	char *p = (char *)&txt;
	char_80 *p_80 = (char_80 *)&txt;
	bool update = false;

	while (line != NULL)
	{
		SerialDEBUG.println(line);
		update = true;

		strcpy(p, line);
		p += strlen(line);

		line = strtok(NULL, " ");
		if (line)
		{
			strcpy(p, " ");
			p += 1;
		}
	}
	if (update)
	{
		deployment_text.write(*p_80);
	}

	SerialDEBUG.print("deployment text : ");
	deployment_text.read(&txt);
	SerialDEBUG.println(txt.txt);

	return 0;
}

int cmd_file(char *line)
{
	if (line)
	{
		char_80 *p;
		p = (char_80 *)line;
		file_name_template.write(*p);
	}

	SerialDEBUG.print("file name : ");
	char_80 txt;
	file_name_template.read(&txt);
	SerialDEBUG.println(txt.txt);

	return 0;
}

SdCard* m_card = nullptr;

int cmd_format(char *line)
{
	FatFormatter fat_format;

	if (line)
	{
		SerialDEBUG.print("Format ");
		SerialDEBUG.println(line);

		if (strcmp(line, "yes") == 0)
		{
			sd.begin(SD_CONFIG);
			m_card = sd.card();
			fat_format.format(m_card, hdr, &SerialDEBUG);
		}
	}

	return 0;
}

int cmd_sync(char *line)
{
	binFile.sync();

	SerialDEBUG.print("sync, position ");
	SerialDEBUG.println(binFile.position());

	return 0;
}


int read_line(char c)
{
	if (line_idx < sizeof(line))
	{
		if ((c == '\b') | (c == 127))
		{
			if (line_idx >= 1)
			{
				line_idx -= 1;
			}
			SerialDEBUG.print(F("\b \b"));
		}
		else if (c == '\r')
		{
			line[line_idx] = 0;
			SerialDEBUG.print("Line : ");
			SerialDEBUG.println(line);

			executeCommand(line);

			line_idx = 0;
			memset(line, 0, sizeof(line));

			return 1;
		}
		else if (c == '\n')
		{
			// drop these
		}
		else
		{
			line[line_idx++] = c;
		}
	}
	else
	{
		line_idx = 0;
	}

	return 0;
}


uint32_t last_rtc;

char new_file_name[80];
bool last_card;

// The loop function is called in an endless loop
void loop()
{
    RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

    rtc_regs = RTC->MODE2.CLOCK.reg; // REG is year/month/day/hour/min/sec
    bool tick = false;

	if (rtc_regs != last_rtc)
	{
		tick = true;

		now = rtc.getEpoch();
		gmtime_r(&now, &now_tm);

		if (can_sleep_count > 0)
		{
			can_sleep_count--;
		}
		else
		{
			if (!can_sleep)
			{
				SerialDEBUG.println("set can sleep");
			}
			can_sleep = true;
		}

		if (debug >= 2)
		{
			print_date(&now_tm);
			SerialDEBUG.print(" rtc tick, mode ");
			SerialDEBUG.println(run_mode.read());
		}

		if (adc_read_state == ADC_STOP)
		{
			if ((run_mode.read() == RUN_MODE_RUN) | ((run_mode.read() == RUN_MODE_START_LATER) && (now >= datetime_start.read())))
			{
				run_sample = true;

				//digitalWrite(ADC_PWR_PIN, 1); // power on the ADC
				//delay(10);

				SerialDEBUG.print("RUN_MODE: adc i2c channel set ");
				SerialDEBUG.println(current_sample_channel);

				// TODO: Read ADC122c04 regs here to check setup

				tcDisable();
				if ((adc_reg1.read() & ADS122C04_CONVERSION_MODE_CONTINUOUS) != 0)
				{
					SerialDEBUG.println("Continuous mode");

					int new_reg1 = adc_reg1.read();
					const int data_rate[] = {20, 45, 90, 175, 330, 600, 1000};
					int dr = data_rate[(new_reg1 >> 5)];
					if (new_reg1 & ADS122C04_OP_MODE_TURBO)
					{
						dr = dr * 2;
					}
					float drf = 1000/dr;
					SerialDEBUG.print("Data Rate : ");
					SerialDEBUG.print(drf);
					SerialDEBUG.println(" (ms)");

					digitalWrite(ADC_PWR_PIN, 1);

					waiting_for_sample = true;
					delay(10);

					get_next_sample_buf();
					adc_setup(0);

					i2c_master_adc_rdata(BYTES_PER_SAMPLE, sample_write_addr); // clears the DRDY line

					// enable fall edge when ADC sample is ready
					attachInterrupt(ADC_INT, adc_convert_isr, LOW);

					adc_read_state = WAIT_ADC;

				}
				else
				{
					SerialDEBUG.println("Timer tick mode");

					tcConfigure(sampleRate.read()); //configure the timer to run at <sampleRate>Hertz
					tcStartCounter(); //starts the timer

					adc_read_state = WAIT_TICK;
				}

			}
			else
			{
				run_sample = false; // don't start next block
			}
		}

		if ((run_sample) && (run_mode.read() == RUN_MODE_STOP))
		{
			run_sample = false; // stop now
		}

		// start a new file every day
		// sprintf(new_file_name, (char *)file_name_template_mem.txt, now_tm.tm_year+1900, now_tm.tm_mon+1, now_tm.tm_mday, now_tm.tm_hour, now_tm.tm_min);
		strftime(new_file_name, sizeof(new_file_name), (char *)file_name_template_mem.txt, &now_tm);

		last_rtc = rtc_regs;
	}
	if (debug >= 2)
	{
		if (last_state != adc_read_state)
		{
			print_date(&now_tm);

			SerialDEBUG.print(" adc_read_state ");

			SerialDEBUG.println(adc_read_state);

			last_state = adc_read_state;
		}
	}
	if (event_sys_flag)
	{
		now = rtc.getEpoch();

		if (debug >= 2)
		{
			print_date(&now_tm);

			SerialDEBUG.println(" event_sys_flag");
		}
		event_sys_flag = false;
	}

	if (rtc_alarm_flag)
	{
		if (debug >= 1)
		{
			print_date(&now_tm);

			SerialDEBUG.println(" RTC interrupt");
		}
		// Sync file, TODO: do this less often or maybe not at all...
		if (binFile.isOpen())
		{
			SerialDEBUG.print("file sync ");
			SerialDEBUG.println(binFile.position());

			binFile.flush();
		}

		// reset alarm time if in start later mode
		if ((run_mode.read() == RUN_MODE_START_LATER) && (now >= datetime_start.read()))
		{
			rtc.setAlarmTime(0, 0, 0);
			rtc.enableAlarm(rtc.MATCH_SS);
		}

		rtc_alarm_flag = false;
	}
	if (missed_sample)
	{
		print_date(&now_tm);

		SerialDEBUG.print(" missed sample state ");
		SerialDEBUG.println(missed_state);
#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(MISSED_SAMPLE_PIN, 1);
#endif

		missed_sample = false;
	}

	// deal with any card removal/insert
	bool det = digitalRead(SD_CAR_DET_PIN);
	if ((card_det_flag) || (last_card != det))
	{
		print_date(&now_tm);

		SerialDEBUG.print(" card detect change ");
		SerialDEBUG.println(det);

		if (det)
		{
			if (!sd.begin(SD_CONFIG))
			{
				SerialDEBUG.println("sd.begin failed");
			}
			else
			{
				SerialDEBUG.print("Fat Type : ");
				SerialDEBUG.println(sd.vol()->fatType());
			}
		}
		else
		{
			sd.end();
		}

		can_sleep = false;
		can_sleep_count = 20;

		card_det_flag = false;

		last_card = det;
	}

	// write data to card if we have filled more than 1/2 the buffer
	int32_t to_write = (int32_t)sample_ready - (int32_t)sample_write_sd;
	uint32_t size_2 = sizeof(data_buffer)/2;
	if (to_write < 0)
	{
		to_write += sizeof(data_buffer);
	}
	if (tick)
	{
		if (debug >= 1)
		{
			print_date(&now_tm);
			SerialDEBUG.print("  ");
			SerialDEBUG.print(run_sample);
			SerialDEBUG.print("  sample_write ");
			SerialDEBUG.print(sample_write);
			SerialDEBUG.print("  sample_ready ");
			SerialDEBUG.print(sample_ready);
			SerialDEBUG.print("  sample_read ");
			SerialDEBUG.print(sample_read);
			SerialDEBUG.print("  sample_write_sd ");
			SerialDEBUG.print(sample_write_sd);
			SerialDEBUG.print("  to write ");
			SerialDEBUG.println(to_write);
		}
	}

	if (to_write >= size_2)
	{
		now = rtc.getEpoch();
		gmtime_r(&now, &now_tm);

		print_date(&now_tm);

		SerialDEBUG.print(" sample write sd ");

		SerialDEBUG.println(sample_write_sd);

		if (debug > 0)
		{
			now = rtc.getEpoch();
			gmtime_r(&now, &now_tm);

			print_date(&now_tm);

			SerialDEBUG.print(" sample read ");

			SerialDEBUG.print(sample_ready);
			SerialDEBUG.print(" data ");

			SerialDEBUG.print(data_buffer[sample_ready]);
			SerialDEBUG.print(" ");
			SerialDEBUG.print(data_buffer[sample_ready+1]);
			SerialDEBUG.print(" ");
			SerialDEBUG.print(data_buffer[sample_ready+2]);
			SerialDEBUG.print(" ");
			SerialDEBUG.print(data_buffer[sample_ready+3]);
			SerialDEBUG.print(" ");
			SerialDEBUG.print(data_buffer[sample_ready+4]);
			SerialDEBUG.print(" ");
			SerialDEBUG.print(data_buffer[sample_ready+5]);
			SerialDEBUG.print(" next ");
			SerialDEBUG.print(data_buffer[sample_ready+6]);
			SerialDEBUG.println(" ....");

			SerialDEBUG.print("Write ");
			SerialDEBUG.print(sample_write_sd);
			SerialDEBUG.print(" length ");
			SerialDEBUG.print(sizeof(data_buffer)/2);
			SerialDEBUG.print(" file pos ");
			SerialDEBUG.println(binFile.position());
		}

		if (sample_write_sd <= 0) // only change file when sample number is zero
		{
			if (!binFile.isOpen())
			{
				// open a new file if file is not open
				open_bin_file(new_file_name);
			}
			else
			{
				if (run_mode.read() == RUN_MODE_STOP)
				{
					binFile.close();
				}
				else if (strcmp(file_name, new_file_name) != 0)
				{
					open_bin_file(new_file_name);
				}
			}

			sample_write_sd = 0;
		}

		// write data to file
		if (binFile.isOpen())
		{
#ifdef ADAFRUIT_FEATHER_M0
			digitalWrite(PIN_WRITE_LED, 1);
#endif

			if (size_2 != binFile.write(&data_buffer[sample_write_sd], size_2))
			{
				SerialDEBUG.print("Write Error ");
				SerialDEBUG.print(size_2);
				uint8_t err = binFile.getError();
				SerialDEBUG.print(" code ");
				SerialDEBUG.println(err);
			}
			if (debug >= 1)
			{
				hexdump(SerialDEBUG, &data_buffer[sample_write_sd], 32);
			}

			memset(&data_buffer[sample_write_sd], 0xFF, size_2); // clear buffer now that its written to SD card

			sample_write_sd += size_2;
			if (sample_write_sd >= sizeof(data_buffer))
			{
				sample_write_sd = 0;
			}
		}

		// Shutdown card by  pulling CS in high
		pinMode(SD_CS_PIN, OUTPUT);
		digitalWrite(SD_CS_PIN, 1);

#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(PIN_WRITE_LED, 0);
#endif

	}
	if (adc_convert)
	{
		if (debug >= 2)
		{
			SerialDEBUG.println("adc_conversion");
		}

		adc_convert = false;
	}

	if (sample_ready >= 0)
	{
		while (sample_read != sample_ready)
		{
			if (debug >= 2)
			{
				SerialDEBUG.print("sample @ ");
				SerialDEBUG.print(sample_read);
				SerialDEBUG.print(" = ");

				SerialDEBUG.print(" count ");
				SerialDEBUG.print(data_buffer[sample_read]);
				SerialDEBUG.print(" data ");
				SerialDEBUG.print(data_buffer[sample_read+1]);
				SerialDEBUG.print(" ");
				SerialDEBUG.print(data_buffer[sample_read+2]);
				SerialDEBUG.print(" ");
				SerialDEBUG.print(data_buffer[sample_read+3]);
				SerialDEBUG.print(" CRC ");
				SerialDEBUG.print(data_buffer[sample_read+4]);
				SerialDEBUG.print(" ");
				SerialDEBUG.print(data_buffer[sample_read+5]);
				SerialDEBUG.println();
			}

			sample_read += BYTES_PER_SAMPLE;
			if (sample_read >= sizeof(data_buffer))
			{
				sample_read = 0;
			}
		}
	}

	while (SerialDEBUG.available())
	{
		char c = SerialDEBUG.read();

		SerialDEBUG.write(c);

		read_line(c);

		can_sleep_count = 20;
		can_sleep = false;

	} // SerialDEBUG.avaiable

	if (can_sleep)
	{
		SerialDEBUG.flush();

		if (sd.fatType() > 0)
		{
			while (sd.card()->isBusy()) {} // make sure the SD card has been flushed
		}

#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(SLEEP_PIN, 1);
#endif

		SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // disable the sys tick, otherwise we get worken by it

		LowPower.standby();
		//LowPower.idle(IDLE_2);

		SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk; // re-enable, used by SdFAT

		// read clock now we're awake
		RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
	    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

	    rtc_regs = RTC->MODE2.CLOCK.reg; // REG is year/month/day/hour/min/sec

		//		if (adc_read_state == WAIT_TICK)
//		{
//			LowPower.standby();
//		}
//		else
//		{
//			LowPower.idle(IDLE_2);
//		}

#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(SLEEP_PIN, 0);
#endif
	}

}

