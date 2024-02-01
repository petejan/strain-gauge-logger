#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2024-02-01 20:26:37

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

void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) ;
void rtc_int() ;
int i2c_master_adc_cmd(uint8_t cmd) ;
void i2c_master_adc_cmd_reg(uint8_t cmd, uint8_t reg) ;
void i2c_master_adc_rdata(uint8_t len, uint8_t buf[]) ;
static inline void i2c_master_dma_set_transfer_tx(uint16_t addr, uint8_t length) ;
static inline void i2c_master_dma_set_transfer_rx(uint16_t addr, uint8_t length) ;
void i2c_write_cmd_dma_nowait(byte cmd) ;
void i2c_write_cmd_dma(byte cmd) ;
void i2c_write_reg_dma_nowait(byte reg, byte value) ;
void i2c_write_reg_dma(byte reg, byte value) ;
void i2c_read_dma(uint8_t *data, byte len) ;
void i2c_read_dma_nowait(uint8_t *data, byte len) ;
bool tcIsSyncing() ;
void tcStartCounter() ;
void tcReset() ;
void tcDisable() ;
void adc_setup(int ch) ;
void get_next_sample_buf(void) ;
void TC5_Handler (void) ;
void tcConfigure(int sampleRate) ;
void i2c_master_init() ;
void adc_convert_isr(void) ;
void EVSYS_Handler(void) ;
void card_det_isr(void) ;
void i2c_error(void) ;
void dma_error(void) ;
void dmatx(Adafruit_ZeroDMA *dma) ;
void dmarx(Adafruit_ZeroDMA *dma) ;
void setup() ;
void print2digits(int number) ;
void print_date(tm *t) ;
void open_bin_file(char *new_name) ;
void * findCommand(char * command) ;
int executeCommand(char *line) ;
int cmd_help(char *line) ;
int cmd_regs(char *line) ;
int cmd_dma(char *line) ;
int cmd_adc(char *line) ;
int cmd_start(char *line) ;
int cmd_stop(char *line) ;
int cmd_startlater(char *line) ;
int cmd_starttime(char *line) ;
int cmd_time(char *line) ;
int cmd_ls(char *line) ;
int cmd_debug(char *line) ;
int cmd_sleep(char *line) ;
int cmd_fcorr(char *line) ;
int cmd_sample(char *line) ;
int cmd_set(char *line) ;
int cmd_text(char *line) ;
int cmd_file(char *line) ;
int cmd_format(char *line) ;
int cmd_sync(char *line) ;
int read_line(char c) ;
void loop() ;

#include "adc122c04_direct.ino"


#endif
