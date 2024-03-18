/*
 * i2c.h
 *
 *  Created on: 13 Mar 2024
 *      Author: pete
 */

#ifndef I2C_H_
#define I2C_H_

#define I2C_TIMEOUT 20000

#ifdef ADAFRUIT_FEATHER_M0
	#define I2C_SERCOM SERCOM3
#endif
#ifdef ARDUINO_QTPY_M0
	#define I2C_SERCOM SERCOM1
#endif
#ifdef ARDUINO_ITSYBITSY_M0
	#define I2C_SERCOM SERCOM3
#endif

#define ADC122C04_ADDRESS 0x40

void i2c_master_init();
int i2c_master_adc_cmd(uint8_t cmd);
void i2c_master_adc_cmd_reg(uint8_t cmd, uint8_t reg);
void i2c_master_adc_rdata(uint8_t len, uint8_t buf[]);



#endif /* I2C_H_ */
