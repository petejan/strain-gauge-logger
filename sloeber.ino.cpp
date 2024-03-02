#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2024-03-02 18:17:27

#include "Arduino.h"
#include "Arduino.h"
#include "ZeroRegs.h"
#include "SdFat.h"
#include <RTCZero.h>
#include <time.h>
#include <flashstorage.h>
#include "LowPower.h"
#include "adc122c04.h"
#include "wiring_private.h"
#include "zmodem.h"
#include "zmodem_zm.h"

void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) ;
void rtc_int() ;
int i2c_master_adc_cmd(uint8_t cmd) ;
void i2c_master_adc_cmd_reg(uint8_t cmd, uint8_t reg) ;
void i2c_master_adc_rdata(uint8_t len, uint8_t buf[]) ;
bool tcIsSyncing() ;
void tcStartCounter() ;
void tcReset() ;
void tcDisable() ;
uint8_t adc_detect_input(void) ;
void adc_setup(int ch) ;
void get_next_sample_buf(void) ;
void TC5_Handler (void) ;
void tcConfigure(uint32_t sampleRate) ;
void i2c_master_init() ;
void adc_convert_isr(void) ;
void EVSYS_Handler(void) ;
void card_det_isr(void) ;
void setup() ;
void print_date(tm *t) ;
void open_bin_file(char *new_name) ;
void * findCommand(char * command) ;
int executeCommand(char *line) ;
int cmd_help(char *line) ;
int cmd_regs(char *line) ;
int cmd_adc(char *line) ;
int cmd_start(char *line) ;
int cmd_startdetect(char *line) ;
int cmd_stop(char *line) ;
int cmd_startlater(char *line) ;
int cmd_starttime(char *line) ;
int cmd_time(char *line) ;
int cmd_ls(char *line) ;
int cmd_debug(char *line) ;
int cmd_burst(char *line) ;
int cmd_sleep(char *line) ;
int cmd_fcorr(char *line) ;
int cmd_sample(char *line) ;
int cmd_set(char *line) ;
int cmd_text(char *line) ;
int cmd_file(char *line) ;
int cmd_sz(char *line) ;
int cmd_rz(char *line) ;
int cmd_format(char *line) ;
int cmd_sync(char *line) ;
int read_line(char c) ;
void loop() ;

#include "adc122c04.ino"


#endif
