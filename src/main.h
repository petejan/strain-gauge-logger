/*
 * main.h
 *
 *  Created on: 12 Mar 2024
 *      Author: pete
 */

#include <FlashStorage.h>
#include <FatLib/FatFile.h>
#include <SdFat.h>
#include <flashstorage.h>
#include <Adafruit_SPIFlash.h>
//#include "flash_config.h"
#include "RTCZero.h"

#ifndef MAIN_H_
#define MAIN_H_

#define SerialDEBUG SERIAL_PORT_HARDWARE

#define BYTES_PER_SAMPLE 6

#ifdef ADAFRUIT_FEATHER_M0
	extern const uint8_t ADC_INT;
	extern const uint8_t PIN_WRITE_LED;
	extern const uint8_t SLEEP_PIN;
	extern const uint8_t ADC_PWR_PIN;

	//extern const uint8_t SD_CAR_DET_PIN;
	#define SD_CAR_DET_PIN 7
	extern const uint8_t SD_CS_PIN;

	extern const uint8_t MISSED_SAMPLE_PIN;
	extern const uint8_t SAMPLE_PIN;

	extern const uint8_t LED_RED_PIN;
	extern const uint8_t LED_GREEN_PIN;
#endif

// Max SPI rate for AVR is 10 MHz for F_CPU 20 MHz, 8 MHz for F_CPU 16 MHz.
#define SPI_CLOCK SD_SCK_MHZ(1)
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)

enum RUN_MODE {RUN_MODE_STOP, RUN_MODE_DETECT, RUN_MODE_RUN, RUN_MODE_START_LATER};

enum STATE {ADC_STOP, WAIT_TICK, WAIT_PWR_TICK, WAIT_ADC, WAIT_RXDMA};

extern volatile STATE adc_read_state;

extern const uint8_t channel_config_reg0[];
extern const uint8_t channel_config_reg1[];

extern uint8_t data_buffer[512 * BYTES_PER_SAMPLE * 2];
extern uint8_t *sample_write_ptr;

extern bool missed_sample;
extern STATE missed_state;
extern volatile bool waiting_for_sample;
extern uint8_t current_sample_channel;

extern const uint8_t ADC_INT;
extern const uint8_t ADC_PWR_PIN;

extern void get_next_sample_buf(void);

extern int16_t burst;

typedef File32 File;
typedef File file_t;

extern file_t binFile;
extern SdFile fout;
extern SdFat32 flash;

extern RTCZero rtc;

extern bool can_sleep;
extern uint8_t can_sleep_count;
extern int8_t debug;

typedef struct {char txt[80];} char_80;

extern uint8_t hdr[512];
extern uint32_t rtc_regs;

extern void print_date(tm *t);

extern time_t now;
extern struct tm now_tm;
extern char file_name[80];
extern char_80 file_name_template_mem;

extern FlashStorageClass<uint8_t> adc_reg2;
extern FlashStorageClass<uint8_t> adc_reg3;

extern FlashStorageClass<uint8_t> fcorr_set;
extern FlashStorageClass<int8_t> debug_set;
extern FlashStorageClass<RUN_MODE> run_mode;
extern FlashStorageClass<uint32_t> datetime_start;
extern FlashStorageClass<uint32_t> sampleRate;
extern FlashStorageClass<uint32_t> pwrUpTime;
extern FlashStorageClass<char_80> deployment_text;
extern FlashStorageClass<char_80> file_name_template;
extern FlashStorageClass<uint16_t> nburst;

#endif /* MAIN_H_ */
