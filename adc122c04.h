/*
 * adc122c04.h
 *
 *  Created on: 9 Apr 2023
 *      Author: pete
 */

#ifndef ADC122C04_H_
#define ADC122C04_H_

// ADS122C04 Table 16 in Datasheet
#define ADS122C04_RESET_CMD          0x06     //0000 011x      Reset
#define ADS122C04_START_CMD          0x08     //0000 100x      Start/Sync
#define ADS122C04_POWERDOWN_CMD      0x02     //0000 001x      PowerDown
#define ADS122C04_RDATA_CMD          0x10     //0001 xxxx      RDATA
#define ADS122C04_RREG_CMD           0x20     //0010 rrxx      Read REG rr= register address 00 to 11
#define ADS122C04_WREG_CMD           0x40     //0100 rrxx      Write REG rr= register address 00 to 11

#define ADS122C04_WRITE_CMD(reg)     (ADS122C04_WREG_CMD | (reg << 2))    //Shift is 2-bit in ADS122C04
#define ADS122C04_READ_CMD(reg)      (ADS122C04_RREG_CMD | (reg << 2))    //Shift is 2-bit in ADS122C04

// ADS122C04 Table 16 in Datasheet
#define ADS122C04_CONFIG_0_REG      0 // Configuration Register 0
#define ADS122C04_CONFIG_1_REG      1 // Configuration Register 1
#define ADS122C04_CONFIG_2_REG      2 // Configuration Register 2
#define ADS122C04_CONFIG_3_REG      3 // Configuration Register 3

// Unshifted register definitions
// The bit field register definitions will do the bit shifting

// Configuration Register 0
// ADS122C04 Table 19 in Datasheet

// Input Multiplexer Configuration
#define ADS122C04_MUX_AIN0_AIN1     0x0 << 4
#define ADS122C04_MUX_AIN0_AIN2     0x1 << 4
#define ADS122C04_MUX_AIN0_AIN3     0x2 << 4
#define ADS122C04_MUX_AIN1_AIN0     0x3 << 4
#define ADS122C04_MUX_AIN1_AIN2     0x4 << 4
#define ADS122C04_MUX_AIN1_AIN3     0x5 << 4
#define ADS122C04_MUX_AIN2_AIN3     0x6 << 4
#define ADS122C04_MUX_AIN3_AIN2     0x7 << 4
#define ADS122C04_MUX_AIN0_AVSS     0x8 << 4
#define ADS122C04_MUX_AIN1_AVSS     0x9 << 4
#define ADS122C04_MUX_AIN2_AVSS     0xa << 4
#define ADS122C04_MUX_AIN3_AVSS     0xb << 4
#define ADS122C04_MUX_REFPmREFN     0xc << 4
#define ADS122C04_MUX_AVDDmAVSS     0xd << 4
#define ADS122C04_MUX_SHORTED       0xe << 4

// Gain Configuration
#define ADS122C04_GAIN_1            0x0 << 1
#define ADS122C04_GAIN_2            0x1 << 1
#define ADS122C04_GAIN_4            0x2 << 1
#define ADS122C04_GAIN_8            0x3 << 1
#define ADS122C04_GAIN_16           0x4 << 1
#define ADS122C04_GAIN_32           0x5 << 1
#define ADS122C04_GAIN_64           0x6 << 1
#define ADS122C04_GAIN_128          0x7 << 1

// PGA Bypass (PGA is disabled when the PGA_BYPASS bit is set)
#define ADS122C04_PGA_DISABLED      0x1
#define ADS122C04_PGA_ENABLED       0x0

// Configuration Register 1
// ADS122C04 Table 19 in Datasheet

// Data Rate
// Turbo mode = Normal mode * 2 (Samples per Second)
// Normal mode
#define ADS122C04_DATA_RATE_20SPS   0x0 << 5
#define ADS122C04_DATA_RATE_45SPS   0x1 << 5
#define ADS122C04_DATA_RATE_90SPS   0x2 << 5
#define ADS122C04_DATA_RATE_175SPS  0x3 << 5
#define ADS122C04_DATA_RATE_330SPS  0x4 << 5
#define ADS122C04_DATA_RATE_600SPS  0x5 << 5
#define ADS122C04_DATA_RATE_1000SPS 0x6 << 5

// Operating Mode
#define ADS122C04_OP_MODE_NORMAL    0x0 << 4
#define ADS122C04_OP_MODE_TURBO     0x1 << 4

// Conversion Mode
#define ADS122C04_CONVERSION_MODE_SINGLE_SHOT   0x0 << 3
#define ADS122C04_CONVERSION_MODE_CONTINUOUS    0x1 << 3

// Voltage Reference Selection
#define ADS122C04_VREF_INTERNAL            0x0 << 1 //2.048V internal
#define ADS122C04_VREF_EXT_REF_PINS        0x1 << 1//REFp and REFn external
#define ADS122C04_VREF_AVDD                0x2 << 1//Analog Supply AVDD and AVSS

// Temperature Sensor Mode
#define ADS122C04_TEMP_SENSOR_OFF          0x0
#define ADS122C04_TEMP_SENSOR_ON           0x1

// Configuration Register 2
// ADS122C04 Table 22 in Datasheet

// Conversion Result Ready Flag (READ ONLY)

// Data Counter Enable
#define ADS122C04_DCNT_DISABLE             0x0 << 6
#define ADS122C04_DCNT_ENABLE              0x1 << 6

// Data Integrity Check Enable
#define ADS122C04_CRC_DISABLED             0x0 << 4
#define ADS122C04_CRC_INVERTED             0x1 << 4
#define ADS122C04_CRC_CRC16_ENABLED        0x2 << 4

// Burn-Out Current Source
#define ADS122C04_BURN_OUT_CURRENT_OFF     0x0 << 3
#define ADS122C04_BURN_OUT_CURRENT_ON      0x1 << 3

// IDAC Current Setting
#define ADS122C04_IDAC_CURRENT_OFF         0x0
#define ADS122C04_IDAC_CURRENT_10_UA       0x1
#define ADS122C04_IDAC_CURRENT_50_UA       0x2
#define ADS122C04_IDAC_CURRENT_100_UA      0x3
#define ADS122C04_IDAC_CURRENT_250_UA      0x4
#define ADS122C04_IDAC_CURRENT_500_UA      0x5
#define ADS122C04_IDAC_CURRENT_1000_UA     0x6
#define ADS122C04_IDAC_CURRENT_1500_UA     0x7

// Configuration Register 3
// ADS122C04 Table 23 in Datasheet

// IDAC1 Routing Configuration
#define ADS122C04_IDAC1_DISABLED           0x0 << 5
#define ADS122C04_IDAC1_AIN0               0x1 << 5
#define ADS122C04_IDAC1_AIN1               0x2 << 5
#define ADS122C04_IDAC1_AIN2               0x3 << 5
#define ADS122C04_IDAC1_AIN3               0x4 << 5
#define ADS122C04_IDAC1_REFP               0x5 << 5
#define ADS122C04_IDAC1_REFN               0x6 << 5

// IDAC2 Routing Configuration
#define ADS122C04_IDAC2_DISABLED           0x0 << 2
#define ADS122C04_IDAC2_AIN0               0x1 << 2
#define ADS122C04_IDAC2_AIN1               0x2 << 2
#define ADS122C04_IDAC2_AIN2               0x3 << 2
#define ADS122C04_IDAC2_AIN3               0x4 << 2
#define ADS122C04_IDAC2_REFP               0x5 << 2
#define ADS122C04_IDAC2_REFN               0x6 << 2




#endif /* ADC122C04_H_ */
