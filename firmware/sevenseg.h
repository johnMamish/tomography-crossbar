#ifndef _SEVENSEG_H
#define _SEVENSEG_H

#include <stdint.h>

#define NUM_SEGS 9
extern volatile uint8_t pixmap[NUM_SEGS][2];

extern const uint8_t digit_map[];

void display_relay_map(int8_t relay_map[8], volatile uint8_t pixmap[NUM_SEGS][2]);

#endif
