#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2024-03-21 20:03:41

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
#include <Adafruit_SPIFlash.h>
#include "src/flash_config.h"
#include "zmodem.h"
#include "zmodem_zm.h"

void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) ;
void rtc_int() ;
void get_next_sample_buf(void) ;
void EVSYS_Handler(void) ;
void card_det_isr(void) ;
void setup() ;
void print_date(tm *t) ;
void open_bin_file(char *new_name) ;
void loop() ;

#include "adc122c04_direct.ino"


#endif
