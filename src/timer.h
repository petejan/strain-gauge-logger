/*
 * timer.h
 *
 *  Created on: 13 Mar 2024
 *      Author: pete
 */

#ifndef TIMER_H_
#define TIMER_H_

extern void tcConfigure(uint32_t sampleRate);
extern void tcDisable();
extern void tcStartCounter();

#endif /* TIMER_H_ */
