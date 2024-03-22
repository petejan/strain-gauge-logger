#include "Arduino.h"
#include "ZeroRegs.h"
#include "SdFat.h"

#include <RTCZero.h>

#include <time.h>
#include <flashstorage.h>

#include "LowPower.h"

#include "src/adc122c04.h"
#include "src/i2c.h"
#include "src/cmd.h"
#include "src/timer.h"
#include "src/dotstar.h"
#include "src/hexdump.h"
#include "src/main.h"

#include "wiring_private.h"

#define SENTRON

#define SerialDEBUG SERIAL_PORT_HARDWARE

#ifdef ARDUINO_ITSYBITSY_M0
#include <Adafruit_SPIFlash.h>
// for flashTransport definition
#include "src/flash_config.h"
#endif

//#include "zmodem_config.h"

#include "zmodem.h"
#include "zmodem_zm.h"

SdFile fout;

//#define SerialDEBUG SERIAL_PORT_USBVIRTUAL


/* flash storage for setup params */

FlashStorageDef(adc_reg2, uint8_t);
FlashStorageDef(adc_reg3, uint8_t);

FlashStorageDef(fcorr_set, uint8_t);
FlashStorageDef(debug_set, int8_t);
FlashStorageDef(run_mode, RUN_MODE);
FlashStorageDef(datetime_start, uint32_t);
FlashStorageDef(sampleRate, uint32_t);
FlashStorageDef(pwrUpTime, uint32_t);
FlashStorageDef(deployment_text, char_80);
FlashStorageDef(file_name_template, char_80);
FlashStorageDef(nburst, uint16_t);

/* Create an rtc object */
RTCZero rtc;

#ifdef ADAFRUIT_FEATHER_M0
	const uint8_t ADC_INT = 5;
	const uint8_t PIN_WRITE_LED = 8;
	const uint8_t SAMPLE_PIN = 13;
	const uint8_t SLEEP_PIN = 11;
	const uint8_t ADC_PWR_PIN = 10;

	#define SD_CAR_DET_PIN  7

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

	#define SD_CAR_DET_PIN  A2

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
#ifdef ARDUINO_ITSYBITSY_M0

	#define FLASH_SS       (39u)                    // Flash chip SS pin.
	#define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?

	Adafruit_SPIFlash flash(&flashTransport);

	// file system object from SdFat
	FatVolume fatfs;
	typedef File file_t;

	const uint8_t ADC_INT = 7;
	const uint8_t ADC_PWR_PIN = 9;

	// SD chip select pin.
	const uint8_t SD_CS_PIN = A1;

	#define I2C_SERCOM SERCOM3
#else
	//------------------------------------------------------------------------------
	// Note: Uno will not support SD_FAT_TYPE = 3.
	// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
	// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
	#define SD_FAT_TYPE 1

	// Max SPI rate for AVR is 10 MHz for F_CPU 20 MHz, 8 MHz for F_CPU 16 MHz.
	#define SPI_CLOCK SD_SCK_MHZ(4)

	#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)

	#if SD_FAT_TYPE == 0
		SdFat flash;
		typedef File file_t;
	#elif SD_FAT_TYPE == 1
		SdFat32 flash;
		typedef File32 file_t;
	#elif SD_FAT_TYPE == 2
		SdExFat flash;
		typedef ExFile file_t;
	#elif SD_FAT_TYPE == 3
		SdFs flash;
		typedef FsFile file_t;
	#else  // SD_FAT_TYPE
		#error Invalid SD_FAT_TYPE
	#endif  // SD_FAT_TYPE
#endif

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


bool rtc_alarm_flag = false;

void rtc_int()
{
	rtc_alarm_flag = true;
}

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

volatile STATE adc_read_state;

int16_t burst;

uint8_t current_sample_channel = 0;
/*
const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
									   ADS122C04_MUX_AIN2_AVSS | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
										0xff};
*/

#ifndef SENTRON
/* strain gauge logger */
const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_128 | ADS122C04_PGA_ENABLED,
										0xff};

const uint8_t channel_config_reg1[] = {ADS122C04_DATA_RATE_20SPS | ADS122C04_OP_MODE_NORMAL | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_EXT_REF_PINS | ADS122C04_TEMP_SENSOR_OFF,
									   0xff};
#endif

#ifdef SENTRON
//const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AVSS | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
//									   ADS122C04_MUX_AIN2_AIN3 | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
//									   0,
//									   0xff};
//
//const uint8_t channel_config_reg1[] = {ADS122C04_DATA_RATE_20SPS | ADS122C04_OP_MODE_NORMAL | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_INTERNAL | ADS122C04_TEMP_SENSOR_OFF,
//									   ADS122C04_DATA_RATE_20SPS | ADS122C04_OP_MODE_NORMAL | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_EXT_REF_PINS | ADS122C04_TEMP_SENSOR_OFF,
//									   ADS122C04_DATA_RATE_20SPS | ADS122C04_OP_MODE_NORMAL | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_INTERNAL | ADS122C04_TEMP_SENSOR_ON,
//									   0xff};
const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AVSS | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
									   0xff};

const uint8_t channel_config_reg1[] = {ADS122C04_DATA_RATE_20SPS | ADS122C04_OP_MODE_NORMAL | ADS122C04_CONVERSION_MODE_SINGLE_SHOT | ADS122C04_VREF_INTERNAL | ADS122C04_TEMP_SENSOR_OFF,
									   0xff};
#endif
/*
const uint8_t channel_config_reg0[] = {ADS122C04_MUX_AIN0_AIN1 | ADS122C04_GAIN_1 | ADS122C04_PGA_ENABLED,
										0xff};
*/

volatile bool waiting_for_sample = false;
volatile int16_t sample_write = -BYTES_PER_SAMPLE;
volatile int16_t sample_ready = -BYTES_PER_SAMPLE;

#ifdef SENTRON
uint8_t data_buffer[512 * BYTES_PER_SAMPLE * 2];
#else
uint8_t data_buffer[512 * 6 * 2];
#endif

time_t now;
struct tm now_tm;

bool missed_sample = false;
STATE missed_state;

bool run_sample = false;

uint8_t *sample_write_addr;

uint32_t rtc_regs;

void get_next_sample_buf(void)
{
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
			memcpy(sample_write_addr+4, &current_sample_channel, sizeof(current_sample_channel)); // copy current channel
			memcpy(sample_write_addr+5, &burst, sizeof(burst)); // copy burst

			sample_write = BYTES_PER_SAMPLE;
			sample_write_addr = &data_buffer[sample_write];
			memset(sample_write_addr, 1, BYTES_PER_SAMPLE); // clear buffer for next sample

			//current_sample_channel = 0; // always start a new block at channel 0
			//burst = 0;
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
uint8_t tx_buf[8];
#define BUF_SIZE sizeof(tx_buf)

volatile int16_t sample_read = -BYTES_PER_SAMPLE;
volatile int16_t sample_write_sd = 0; // last write position

uint8_t hdr[512];

bool can_sleep = false;
uint8_t can_sleep_count = 20;

int8_t debug = 0;

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

file_t binFile;

//The setup function is called once at startup of the sketch
void setup()
{
	rtc.begin(); // initialize RTC

	SYSCTRL->XOSC32K.reg |= SYSCTRL_XOSC32K_RUNSTDBY;

	/* enable the event system and the RTC event PERE7, this wakes every second, for long sample times this makes sure it
	 * wakes occasionally. Probably draws some port, although only 13 ms every second
	 */
    RTC->MODE2.CTRL.reg &= ~RTC_MODE2_CTRL_ENABLE; // disable RTC as bit is only writable when disabled
    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

	RTC->MODE2.EVCTRL.bit.PEREO7 = 1; // bit is enable protected, enable per7, 1 / second

	RTC->MODE2.CTRL.reg |= RTC_MODE2_CTRL_ENABLE; // enable RTC
    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

    PM->APBCMASK.reg |= PM_APBCMASK_EVSYS; // Switch on the event system peripheral

    // enable the I2C serial port
#ifdef ARDUINO_ITSYBITSY_M0
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3; // Switch on the I2C_SERCOM (I2C) clock

    // enable I2C_SERCOM sys clock
    GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | (SERCOM3_GCLK_ID_CORE << GCLK_CLKCTRL_ID_Pos)));
#endif
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

    GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_GEN_GCLK0 | (GCLK_CLKCTRL_ID_ADC_Val << GCLK_CLKCTRL_ID_Pos)));
    while (GCLK->STATUS.bit.SYNCBUSY);
    GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_GEN_GCLK0 | (GCLK_CLKCTRL_ID_DAC_Val << GCLK_CLKCTRL_ID_Pos)));
    while (GCLK->STATUS.bit.SYNCBUSY);


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

	SerialDEBUG.begin(115200); // start serial communication at 115200bps

	SerialDEBUG.printf("ADC122C04 " __DATE__ " " __TIME__ "\r\n");

	i2c_master_init();

	volatile uint32_t val1, val2, val3, val4;
	volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
	val1 = *ptr1;
	volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
	val2 = *ptr;
	ptr++;
	val3 = *ptr;
	ptr++;
	val4 = *ptr;

	SerialDEBUG.printf("chip id: 0x%08x %08x %08x %08x\r\n", val1, val2, val3, val4);

#ifdef SD_CAR_DET_PIN
	pinMode(SD_CAR_DET_PIN, INPUT_PULLUP);
#endif
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

	SerialDEBUG.printf("SystemClock %d\r\n", SystemCoreClock);

	// attachInterrupt(ADC_INT, adc_convert_isr, FALLING);
#ifdef SD_CAR_DET_PIN
	attachInterrupt(SD_CAR_DET_PIN, card_det_isr, CHANGE);
#endif
    // Set callback
    FsDateTime::setCallback(dateTime);

    // Setup the RTC frequency correction
	SerialDEBUG.printf("fcorr %d\r\n", fcorr_set.read());

	RTC->MODE2.FREQCORR.reg = fcorr_set.read();

	// Setup the sample rate, default to 1 sample/second (2000 ms)
	if (sampleRate.read() == 0)
	{
		sampleRate.write(2000);

#ifdef SENTRON
		pwrUpTime.write(1000);
#else
		pwrUpTime_f.write(3);
#endif

#ifdef SENTRON
		uint8_t reg2 = ADS122C04_BURN_OUT_CURRENT_OFF | ADS122C04_IDAC_CURRENT_1000_UA;
#else
		uint8_t reg2 = ADS122C04_BURN_OUT_CURRENT_OFF | ADS122C04_IDAC_CURRENT_OFF;
#endif
#if BYTES_PER_SAMPLE == 6
		reg2 |= ADS122C04_CRC_CRC16_ENABLED | ADS122C04_DCNT_ENABLE;
#elif BYTES_PER_SAMPLE == 5
		reg2 |= ADS122C04_CRC_CRC16_ENABLED;
#elif BYTES_PER_SAMPLE == 4
		reg2 |= ADS122C04_DCNT_ENABLE;
#endif
		adc_reg2.write(reg2);
#ifdef SENTRON
		adc_reg3.write(ADS122C04_IDAC1_AIN1 | ADS122C04_IDAC2_DISABLED);
#else
		adc_reg3.write(ADS122C04_IDAC1_DISABLED | ADS122C04_IDAC2_DISABLED);
#endif
	}
	if (nburst.read() == 0)
	{
		nburst.write(1);
	}
	burst = nburst.read();

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

		SerialDEBUG.printf("Start Later Mode : ");
		print_date(&tm_elements);
		SerialDEBUG.printf("\r\n");
	}
	else
	{
		rtc.setAlarmTime(0, 0, 0);
		rtc.enableAlarm(rtc.MATCH_SS);
	}

	// RTC alarm
	rtc.attachInterrupt(rtc_int);

	if (i2c_master_adc_cmd(ADS122C04_RESET_CMD) == 0)
	{
		SerialDEBUG.printf("ERROR: ADC did not respond\r\n");
	}

	/* setup the logging file */
	file_name_template.read(&file_name_template_mem);

	if (strlen((char *)file_name_template_mem.txt) == 0)
	{
#ifndef ADAFRUIT_ITSYBITSY_M0
		char_80 file_name_def = { "SGL-%Y-%m-%d-T%H.BIN" };
#else
		char_80 file_name_def = { "DATA.BIN" };
#endif

		file_name_template.write(file_name_def);
	}
	file_name_template.read(&file_name_template_mem);
	SerialDEBUG.printf("file template : %s\r\n", file_name_template_mem.txt);

	/* read setup commands from file */

#ifndef ADAFRUIT_ITSYBITSY_M0
	int det = digitalRead(SD_CAR_DET_PIN);

	if (det)
	{
		if (!flash.begin(SD_CONFIG))
		{
			SerialDEBUG.printf("sd.begin failed\r\n");
		}
		else
		{
			SerialDEBUG.printf("Fat Type : %d\r\n", flash.vol()->fatType());
		}
	}
	else
	{
		SerialDEBUG.printf("no card\r\n");
	}
#else
	  // Initialize flash library and check its chip ID.
	  if (!flash.begin())
	  {
		  SerialDEBUG.printf("Error, failed to initialize flash chip!\r\n");
	  }
	  else
	  {
		  SerialDEBUG.printf("Flash chip JEDEC ID: 0x%04x\r\n", flash.getJEDECID());
		  SerialDEBUG.printf("Flash size: %d KB\r\n", flash.size() / 1024);
	  }
	  // First call begin to mount the filesystem.  Check that it returns true
	  // to make sure the filesystem was mounted.
	  if (!fatfs.begin(&flash))
	  {
		  SerialDEBUG.printf("Error, failed to mount newly formatted filesystem!\r\n");
	  }
	  else
	  {
		  SerialDEBUG.printf("Mounted filesystem!\r\n");
	  }

#endif

	int n;
	binFile.open("CONFIG.TXT", FILE_READ);

	// check for open error
	if (binFile.isOpen())
	{
		char line[80];

		SerialDEBUG.printf("\r\nCONFIG.TXT file .... ");

		// read lines from the file
		while ((n = binFile.fgets(line, sizeof(line))) > 0)
		{
			if (line[n - 1] == '\n')
			{
				// remove '\n'
				line[n-1] = 0;

				SerialDEBUG.printf("file line : %s\r\n", line);
				// replace next line with LCD call to display line

				executeCommand(line);
			}
			else
			{
				// no '\n' - line too long or missing '\n' at EOF
				// handle error
			}
		}

		binFile.close();

		SerialDEBUG.printf("... end CONFIG.TXT file\r\n\n");
	}
	else
	{
		SerialDEBUG.printf("could not open CONFIG.TXT file\r\n");
	}

#ifdef ADAFRUIT_ITSYBITSY_M0
	dotstar_send(0x01000001); // dim RED
#endif

	/* Sampling is started in main loop */

	run_sample = false;
	if (run_mode.read() == RUN_MODE_START_LATER)
	{
		digitalWrite(ADC_PWR_PIN, 0); // start powered down
		if (now >= datetime_start.read())
		{
			run_sample = true;
		}
	}

	if (run_mode.read() == RUN_MODE_RUN)
	{
		run_sample = true;
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

    uint32_t sample_rt;

    sample_rt = sampleRate.read();

    SerialDEBUG.printf("sample rate %d\r\n", sample_rt);

}

void print_date(tm *t)
{
	SerialDEBUG.printf("%d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
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
			SerialDEBUG.printf("file was open, create new file\t\n");

			binFile.close();

			binFile.open(new_name, FILE_WRITE);

			can_sleep = false;
			can_sleep_count = 5;
		}
	}
	else
	{
		SerialDEBUG.printf("file not open, append\r\n");

		binFile.open(new_name, FILE_WRITE);

		binFile.getSFN(name_tmp, sizeof(name_tmp));
		SerialDEBUG.printf("short file name after open : %s\r\n", name_tmp);

		can_sleep = false;
		can_sleep_count = 5;
	}

	SerialDEBUG.printf("file pos : %d\r\n", binFile.position());

	// write header to start of new file
	if (binFile.isOpen() && (binFile.position() == 0))
	{
		write_hdr = true;
	}

	/* did the file open, if not try and restart the SD card */
	/* this can also happen if the file name clashes with one on the card already */

	if (!binFile.isOpen())
	{
		SerialDEBUG.printf("open failed : %s\r\n", new_name);

#ifdef SD_CAR_DET_PIN
		int det = digitalRead(SD_CAR_DET_PIN);
#else
		int det = 1;
#endif

		if (det)
		{
#ifndef ADAFRUIT_ITSYBITSY_M0
			// try and close file, and re-open the SD card
			flash.end();

			if (!flash.begin(SD_CONFIG))
			{
				SerialDEBUG.printf("sd.begin failed\r\n");
			}
			else
			{
				SerialDEBUG.printf("Fat Type : %d\r\n", flash.vol()->fatType());
			}
		}
		else
		{
			SerialDEBUG.printf("no card\r\n");
		}
#else
		if (!flash.begin())
		{
			SerialDEBUG.printf("flash.begin failed\r\n");
		}
	}
	else
	{
		SerialDEBUG.printf("no card\r\n");
	}

#endif
	}

	/* for a new file, write the file header to the start of the file */
	if (write_hdr)
	{
		char *p = (char *)hdr;
		int l;
		int ad_ch = 0;

		memset(hdr, 0, sizeof(hdr));
		p += sprintf(p, "adc122c04 " __DATE__ " " __TIME__ "\r\n");
		p += sprintf(p, "sample rate = %d (ms)\r\n", sampleRate);
		p += sprintf(p, "power up time = %d (ms)\r\n", pwrUpTime);
		p += sprintf(p, "bytes per block = %d\r\n", sizeof(data_buffer));
		p += sprintf(p, "no. burst = %d\r\n", nburst);
		p += sprintf(p, "bytes per sample = %d\r\n", BYTES_PER_SAMPLE);
		p += sprintf(p, "buffer size = %d\r\n", sizeof(data_buffer));
		while (channel_config_reg0[ad_ch] != 0xff)
		{
			p += sprintf(p, "ADC Channel %2d = 0x%02x 0x%02x\r\n", ad_ch, channel_config_reg0[ad_ch], channel_config_reg1[ad_ch]);

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

		SerialDEBUG.printf("Write Hdr:\r\n-------------------\r\n");
		SerialDEBUG.print((char *)hdr);
		SerialDEBUG.printf("-------------------\r\n");

		binFile.write(hdr, sizeof(hdr));
		binFile.sync();
	}

	strcpy(file_name, new_name);

	if (binFile.isOpen())
	{
		SerialDEBUG.printf("File open : %s\r\n", file_name);
	}
}

STATE last_state = ADC_STOP;


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

#ifdef ADAFRUIT_ITSYBITSY_M0
	dotstar_send(0x00000001); // RED off
#endif

	if (rtc_regs != last_rtc)
	{
		tick = true;

#ifdef ADAFRUIT_ITSYBITSY_M0
		dotstar_send(0x02000100); // GREEN on
#endif

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
				SerialDEBUG.printf("set can sleep\r\n");
			}
			can_sleep = true;
		}

		if (debug >= 2)
		{
			print_date(&now_tm);
			SerialDEBUG.printf(" rtc tick, mode %d\r\n", run_mode.read());
		}

		if (adc_read_state == ADC_STOP)
		{
			if ((run_mode.read() == RUN_MODE_RUN) | ((run_mode.read() == RUN_MODE_START_LATER) && (now >= datetime_start.read())))
			{
				run_sample = true;

				//digitalWrite(ADC_PWR_PIN, 1); // power on the ADC
				//delay(10);

				SerialDEBUG.printf("RUN_MODE: adc i2c channel set %d\r\n", current_sample_channel);

				// TODO: Read ADC122c04 regs here to check setup

				tcDisable();
				burst = nburst.read();

				if ((channel_config_reg1[0] & ADS122C04_CONVERSION_MODE_CONTINUOUS) != 0)
				{
					SerialDEBUG.printf("Continuous mode\r\n");

					int new_reg1 = channel_config_reg1[0];
					const int data_rate[] = {20, 45, 90, 175, 330, 600, 1000};
					int dr = data_rate[(new_reg1 >> 5)];
					if (new_reg1 & ADS122C04_OP_MODE_TURBO)
					{
						dr = dr * 2;
					}
					float drf = 1000/dr;
					SerialDEBUG.printf("Data Rate : %f (ms)\r\n", drf);

					digitalWrite(ADC_PWR_PIN, 1);

					waiting_for_sample = true;
					delay(10);

					get_next_sample_buf();
					adc_setup(0);

					i2c_master_adc_rdata(BYTES_PER_SAMPLE, sample_write_addr); // clears the DRDY line

					if (burst <= 0)
					{
						// enable fall edge when ADC sample is ready
						attachInterrupt(ADC_INT, adc_convert_isr, LOW);

						adc_read_state = WAIT_ADC;
					}
					else
					{
						SerialDEBUG.printf("burst mode\r\n");

						tcConfigure(sampleRate.read()); //configure the timer to run at <sampleRate>Hertz
						tcStartCounter(); //starts the timer

						adc_read_state = WAIT_TICK;
					}

				}
				else
				{
					SerialDEBUG.printf("Timer tick mode\r\n");

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

			SerialDEBUG.printf(" adc_read_state %d\r\n", adc_read_state);

			last_state = adc_read_state;
		}
	}
	if (event_sys_flag)
	{
		now = rtc.getEpoch();

		if (debug >= 2)
		{
			print_date(&now_tm);

			SerialDEBUG.printf(" event_sys_flag\r\n");
		}
		event_sys_flag = false;
	}

	if (rtc_alarm_flag)
	{
		if (debug >= 1)
		{
			print_date(&now_tm);

			SerialDEBUG.printf(" RTC match (seconds = 0)\r\n");
		}
		// Sync file, TODO: do this less often or maybe not at all...
		if (binFile.isOpen())
		{
			SerialDEBUG.printf("file sync %d\r\n", binFile.position());

			binFile.flush();
		}

		// reset alarm time if in start later mode
		if ((run_mode.read() == RUN_MODE_START_LATER) && (now >= datetime_start.read()))
		{
			rtc.setAlarmTime(0, 0, 0);
			rtc.enableAlarm(rtc.MATCH_SS);
		}

		if ((run_mode.read() == RUN_MODE_DETECT))
		{
			if (adc_detect_input() > 0)
			{
				print_date(&now_tm);
				SerialDEBUG.printf(" setting run mode\r\n");

				run_mode.write(RUN_MODE_RUN);
			}
		}
		rtc_alarm_flag = false;
	}
	if (missed_sample)
	{
		print_date(&now_tm);

		SerialDEBUG.printf(" missed sample state %d\r\n", missed_state);
#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(MISSED_SAMPLE_PIN, 1);
#endif

		missed_sample = false;
	}
#ifdef SD_CAR_DET_PIN
	// deal with any card removal/insert
	bool det = digitalRead(SD_CAR_DET_PIN);
	if ((card_det_flag) || (last_card != det))
	{
		print_date(&now_tm);

		SerialDEBUG.printf(" card detect change %d\r\n", det);

		if (det)
		{
			if (!flash.begin(SD_CONFIG))
			{
				SerialDEBUG.print("sd.begin failed\r\n");
			}
			else
			{
				SerialDEBUG.printf("Fat Type : %d\r\n", flash.vol()->fatType());
			}
		}
		else
		{
			flash.end();
		}

		can_sleep = false;
		can_sleep_count = 20;

		card_det_flag = false;

		last_card = det;
	}
#endif

	// write data to card if we have filled more than 1/2 the buffer
	int32_t to_write;
	int32_t size_2 = sizeof(data_buffer)/2;

	if (sample_ready >= 0)
	{
		if (sample_ready >= sample_write_sd)
		{
			to_write = (int32_t)sample_ready - (int32_t)sample_write_sd;
		}
		else
		{
			to_write = (int32_t)sample_ready - (int32_t)sample_write_sd + sizeof(data_buffer);
		}
	}
	else
	{
		to_write = 0;
	}
	if (tick)
	{
		if (debug >= 1)
		{
			print_date(&now_tm);
			SerialDEBUG.printf(" %d write %d ready %d read %d write_sd %d to %d\r\n", run_sample, sample_write, sample_ready, sample_read, sample_write_sd, to_write);
		}
	}

	if (to_write >= size_2)
	{
		now = rtc.getEpoch();
		gmtime_r(&now, &now_tm);

		print_date(&now_tm);

		SerialDEBUG.printf(" sample write sd %d\r\n", sample_write_sd);

		if (debug > 0)
		{
			now = rtc.getEpoch();
			gmtime_r(&now, &now_tm);

			print_date(&now_tm);

			SerialDEBUG.printf(" sample ready %d data", sample_ready);

			SerialDEBUG.printf("%d %d %d %d %d %d\r\n", data_buffer[sample_ready], data_buffer[sample_ready+1], data_buffer[sample_ready+2], data_buffer[sample_ready+3], data_buffer[sample_ready+4], data_buffer[sample_ready+5], data_buffer[sample_ready+6]);

			SerialDEBUG.printf("Write %d length %d file pos %d\r\n", sample_write_sd, sizeof(data_buffer)/2, binFile.position());
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
				uint8_t err = binFile.getError();
				SerialDEBUG.printf("Write Error %d code %d\r\n", size_2, err);
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
#ifdef ADAFRUIT_ITSYBITSY_M0
		dotstar_send(0x02010000); // BLUE on
#endif

		if (debug >= 2)
		{
			SerialDEBUG.printf("adc_conversion\r\n");
		}
		if (debug == -1)
		{
			if (current_sample_channel == 0)
			{
				now = rtc.getEpoch();
				gmtime_r(&now, &now_tm);

				print_date(&now_tm);

				SerialDEBUG.printf(" ch data %-8d %-8d %-8d\r\n", channel_data[0], channel_data[1], channel_data[2]);
			}
		}

		adc_convert = false;
	}

	if (sample_ready >= 0)
	{
		while (sample_read != sample_ready)
		{
			if (debug >= 2)
			{
				int32_t data;

				data = data_buffer[sample_read+1];
				if (data > 128) // sign extend
				{
					data = data | 0xffffff00;
				}
				data |= (data << 8) + data_buffer[sample_read+2];
				data |= (data << 8) + data_buffer[sample_read+3];
				SerialDEBUG.printf("sample @ %4d = count %3d data %8ld CRC %3d %3d\r\n", sample_read, data_buffer[sample_read], data, data_buffer[sample_read+4], data_buffer[sample_read+5]);
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

#ifndef ADAFRUIT_ITSYBITSY_M0
		if (flash.fatType() > 0)
		{
			while (flash.card()->isBusy()) {} // make sure the SD card has been flushed
		}
#endif

#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(SLEEP_PIN, 1);
#endif

#ifdef ADAFRUIT_ITSYBITSY_M0
		dotstar_send(0x00000000); // all off
#endif

		SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // disable the sys tick, otherwise we get worken by it

		LowPower.standby();
		//LowPower.idle(IDLE_2);

		SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk; // re-enable, used by SdFAT

#ifdef ADAFRUIT_ITSYBITSY_M0
		dotstar_send(0x01000001); // red
#endif

		// read clock now we're awake
		RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
	    while (RTC->MODE2.STATUS.bit.SYNCBUSY);

	    rtc_regs = RTC->MODE2.CLOCK.reg; // REG is year/month/day/hour/min/sec

#ifdef ADAFRUIT_FEATHER_M0
		digitalWrite(SLEEP_PIN, 0);
#endif
	}

}

