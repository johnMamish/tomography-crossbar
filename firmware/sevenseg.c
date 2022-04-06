#include "tm4c123gh6pm.h"
#include "sevenseg.h"

int segment_index = 0;
volatile uint8_t pixmap[NUM_SEGS][2];

const uint8_t digit_map[] = {
    0b11000000,    // 0
    0b11111001,    // 1
    0b10100100,    // 2
    0b10110000,    // 3
    0b10011001,    // 4
    0b10010010,    // 5
    0b10000010,    // 6
    0b11111000,    // 7
    0b10000000,    // 8
    0b10010000,    // 9
};

void Timer0AHandler()
{
    // clear interrupt
    TIMER0_ICR_R |= (1 << 0);

    // bring strobe high
    GPIO_PORTA_DATA_BITS_R[(1 << 4)] = (1 << 4);

    // queue up new SPI write to displays
    uint16_t segment_mask = (1 << segment_index);

    SSI0_DR_R = pixmap[segment_index][1];
    SSI0_DR_R = pixmap[segment_index][0];
    SSI0_DR_R = (uint8_t)(segment_mask & 0xff);
    SSI0_DR_R = (uint8_t)((segment_mask & 0xff00) >> 8);

    segment_index++;
    if (segment_index > NUM_SEGS) segment_index = 0;

    // bring strobe low
    GPIO_PORTA_DATA_BITS_R[(1 << 4)] = 0;
}
