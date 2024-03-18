/*
 * hexdump.h
 *
 *  Created on: 11 Mar 2024
 *      Author: pete
 */

#ifndef HEXDUMP_HPP_
#define HEXDUMP_HPP_

#include <Arduino.h>

extern void hexdump(
	    Stream& out_stream,
	    const uint8_t* const data,
	    const uint32_t byteCount,
	    const bool header = true,
	    const char* const message = nullptr,
	    const uint8_t linefeeds = 1,
	    const uint16_t offset = 0);

#endif /* HEXDUMP_HPP_ */
