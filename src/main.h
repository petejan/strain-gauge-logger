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

enum RUN_MODE {RUN_MODE_STOP, RUN_MODE_DETECT, RUN_MODE_RUN, RUN_MODE_START_LATER};

enum STATE {ADC_STOP, WAIT_TICK, WAIT_PWR_TICK, WAIT_ADC, WAIT_RXDMA};

extern volatile STATE adc_read_state;

extern const uint8_t channel_config_reg0[];
extern const uint8_t channel_config_reg1[];

extern volatile int16_t sample_write;
extern volatile int16_t sample_ready;

extern bool missed_sample;
extern STATE missed_state;
extern volatile bool waiting_for_sample;
extern uint8_t current_sample_channel;
extern uint8_t *sample_write_addr;

extern const uint8_t ADC_INT;
extern const uint8_t ADC_PWR_PIN;

extern void get_next_sample_buf(void);

extern int16_t burst;

typedef File32 File;
typedef File file_t;

extern file_t binFile;
extern SdFile fout;
extern Adafruit_SPIFlash flash;

extern RTCZero rtc;

extern bool can_sleep;
extern uint8_t can_sleep_count;
extern int8_t debug;

typedef struct {char txt[80];} char_80;

extern void print_date(tm *t);

extern time_t now;
extern struct tm now_tm;
extern char file_name[80];
extern char_80 file_name_template_mem;


#include <FlashStorage.h>

#define FlashStoragePtr(name, T) \
  extern FlashStorageClass<T> name;

#define FlashStorageDef(name, T) \
  __attribute__((__aligned__(256))) \
  static const uint8_t PPCAT(_data,name)[(sizeof(T)+255)/256*256] = { }; \
  __attribute__((section(".rodata#"))) FlashStorageClass<T> name(PPCAT(_data,name));

FlashStoragePtr(adc_reg2, uint8_t);
FlashStoragePtr(adc_reg3, uint8_t);

FlashStoragePtr(fcorr_set, uint8_t);
FlashStoragePtr(debug_set, int8_t);
FlashStoragePtr(run_mode, RUN_MODE);
FlashStoragePtr(datetime_start, uint32_t);
FlashStoragePtr(sampleRate, uint32_t);
FlashStoragePtr(pwrUpTime, uint32_t);
FlashStoragePtr(deployment_text, char_80);
FlashStoragePtr(file_name_template, char_80);
FlashStoragePtr(nburst, uint16_t);

#endif /* MAIN_H_ */
