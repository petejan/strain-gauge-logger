/*
 * i2c.c
 *
 *  Created on: 11 Mar 2024
 *      Author: pete
 */


#include <Arduino.h>
#include "adc122c04.h"
#include "i2c.h"
#include "wiring_private.h"


/* I2C master initialization */
void i2c_master_init()
{
	// when the pin mux is not I2C, make the outputs low so as not to draw current
	pinMode(PIN_WIRE_SCL, OUTPUT);
	pinMode(PIN_WIRE_SDA, OUTPUT);

	digitalWrite(PIN_WIRE_SCL, 0);
	digitalWrite(PIN_WIRE_SDA, 0);

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

	pinPeripheral(PIN_WIRE_SCL, PIO_SERCOM);
	pinPeripheral(PIN_WIRE_SDA, PIO_SERCOM);
}

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


