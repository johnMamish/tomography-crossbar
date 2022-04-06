/**
 * Commands
 *     Set relays
 *     get ID
 *     enable relay output
 *     disable relay output
 *     transmit to child left
 *     transmit to child right
 *     get child left
 *     get child right
 */

#include "tm4c123gh6pm.h"
#include "sevenseg.h"
#include <stdint.h>

static void init_hw();

void ssi1_blocking_write(uint8_t data)
{
    while (!(SSI1_SR_R & SSI_SR_TNF));
    SSI1_DR_R = data;
}

int hot_relay[16] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

int main()
{
    init_hw();

    asm volatile("cpsie i\r\n\t");

    for (int i = 0; i < NUM_SEGS; i++) {
        for (int k = 0; k < 2; k++) {
            pixmap[i][k] = i * (k + 1);
        }
    }

    for (int i = 0; i < 16; i++) {
        hot_relay[i] = i % 8;
    }

    GPIO_PORTD_DATA_BITS_R[(1 << 1)] = 0;

    int count = 0;
    while(1) {
        for (int k = 0; k < NUM_SEGS; k++) {
            pixmap[k][0] = digit_map[(k + count) % 10];
            pixmap[k][1] = digit_map[(k + count + 1) % 10];
        }

        for (volatile int i = 0; i < 5000000; i++);
        count++;

        for (int i = 0; i < 16; i++)
            hot_relay[i] = (hot_relay[i] + 1) % 8;

        // bring strobe low
        GPIO_PORTD_DATA_BITS_R[(1 << 2)] = 0;
        for (int i = 0; i < 16; i++) {
            if (hot_relay[i] < 8)
                ssi1_blocking_write(1 << hot_relay[i]);
            else
                ssi1_blocking_write(0);
        }


        // busywait before bringing strobe high
        while (SSI1_SR_R & SSI_SR_BSY);
        GPIO_PORTD_DATA_BITS_R[(1 << 2)] = (1 << 2);
    }
}


static void init_hw()
{
    // enable clock to all GPIO ports
    SYSCTL_RCGCGPIO_R |= 0x3f;

    ////////////////////////////////////////////////////////////////
    // Setup display driver
    // PA2 and PA5 should be controlled by SSI0 as clk and tx respectively.
    GPIO_PORTA_AFSEL_R |=  ((1 << 5) | (1 << 2));
    GPIO_PORTA_ODR_R   &= ~((1 << 5) | (1 << 2));
    GPIO_PORTA_DEN_R   |=  ((1 << 5) | (1 << 2));
    GPIO_PORTA_PCTL_R  &= ~(GPIO_PCTL_PA5_M | GPIO_PCTL_PA2_M);
    GPIO_PORTA_PCTL_R  |=  (GPIO_PCTL_PA5_SSI0TX | GPIO_PCTL_PA2_SSI0CLK);

    // PA3 is ~output_enable, PA4 is the display strobe, and PA6 is the display clear.
    // these pins are all controlled directly by the GPIO module.
    GPIO_PORTA_AFSEL_R &= ~((1 << 6) | (1 << 4) | (1 << 3));
    GPIO_PORTA_DIR_R   |=  ((1 << 6) | (1 << 4) | (1 << 3));
    GPIO_PORTA_ODR_R   &= ~((1 << 6) | (1 << 4) | (1 << 3));
    GPIO_PORTA_DEN_R   |=  ((1 << 6) | (1 << 4) | (1 << 3));

    // display clear signal starts high and remains there
    GPIO_PORTA_DATA_BITS_R[(1 << 6)] = (1 << 6);

    // ~output_enable signal starts low and remains there
    GPIO_PORTA_DATA_BITS_R[(1 << 3)] = 0;

    // enable SSI0's clock
    SYSCTL_RCGCSSI_R |= (1 << 0);
    for (volatile int i = 0; i < 100; i++);

    // Configure SSI0 to be a controller
    SSI0_CR1_R     &= ~SSI_CR1_SSE;     // disable SSI0 before configuring
    SSI0_CR1_R     &= ~SSI_CR1_MS;      // controller mode
    SSI0_CR1_R     |=  SSI_CR1_EOT;     // TXRIS interrupt fires when transmission finished
    SSI0_CC_R       = 0;                // use system clock, not 16MHz PIOSC (don't care actually)
    SSI0_CPSR_R     = 2;                // use minimum value of 2 for clock prescaler 2
    SSI0_CR0_R      = ((7 << 8) |       // divide clock down by factor of 8 with prescaler 1
                       (0 << 7) |       // SPI phase (CPHA) is 0
                       (0 << 6) |       // SPI polarity (CPOL) is 0
                       (0 << 4) |       // Freescale frame format
                       (7 << 0));       // 8-bit data

    // enable SSI0
    SSI0_CR1_R     |= SSI_CR1_SSE;

    ////////////////////////////////////////////////////////////////
    // Setup timer0 for driving display; should fire an interrupt at 1kHz
    SYSCTL_RCGCTIMER_R |= (1 << 0);
    for (volatile int i = 0; i < 100; i++);

    TIMER0_CTL_R  &= ~TIMER_CTL_TAEN;
    TIMER0_CFG_R   =  4;
    TIMER0_TAMR_R &= ~TIMER_TAMR_TAMR_M;
    TIMER0_TAMR_R |=  TIMER_TAMR_TAMR_PERIOD;
    TIMER0_TAILR_R  = 16000;

    TIMER0_IMR_R   =  TIMER_IMR_TATOIM;

    //(&NVIC_EN0_R)[INT_TIMER0A / 32] |= (1ul << (INT_TIMER0A % 32));
    NVIC_EN0_R = (1 << 19);

    TIMER0_CTL_R  |= TIMER_CTL_TAEN;
    TIMER0_ICR_R |= (1 << 0);

    ////////////////////////////////////////////////////////////////
    // Setup SSI1 for driving relays
    // PD0 and PD3 are controlled by SSIx
    GPIO_PORTD_AFSEL_R |=  ((1 << 3) | (1 << 0));
    GPIO_PORTD_ODR_R   &= ~((1 << 3) | (1 << 0));
    GPIO_PORTD_DEN_R   |=  ((1 << 3) | (1 << 0));
    GPIO_PORTD_PCTL_R  &= ~(GPIO_PCTL_PD3_M | GPIO_PCTL_PD0_M);
    GPIO_PORTD_PCTL_R  |=  (GPIO_PCTL_PD3_SSI1TX | GPIO_PCTL_PD0_SSI1CLK);

    // PD1 is ~output_enable and PD2 is the output strobe
    GPIO_PORTD_AFSEL_R &= ~((1 << 2) | (1 << 1));
    GPIO_PORTD_DIR_R   |=  ((1 << 2) | (1 << 1));
    GPIO_PORTD_ODR_R   &= ~((1 << 2) | (1 << 1));
    GPIO_PORTD_DEN_R   |=  ((1 << 2) | (1 << 1));

    // start ~output_enable signal high
    GPIO_PORTD_DATA_BITS_R[(1 << 1)] = (1 << 1);

    // enable SSI1 clock
    SYSCTL_RCGCSSI_R |= (1 << 1);
    for (volatile int i = 0; i < 100; i++);

    // Configure SSI1 to be an SPI controller
    SSI1_CR1_R     &= ~SSI_CR1_SSE;     // disable SSI1 before configuring
    SSI1_CR1_R     &= ~SSI_CR1_MS;      // controller mode
    SSI1_CR1_R     |=  SSI_CR1_EOT;     // TXRIS interrupt fires when transmission finished
    SSI1_CC_R       = 0;                // use system clock, not 16MHz PIOSC (don't care actually)
    SSI1_CPSR_R     = 2;                // use minimum value of 2 for clock prescaler 2
    SSI1_CR0_R      = ((7 << 8) |       // divide clock down by factor of 8 with prescaler 1
                       (0 << 7) |       // SPI phase (CPHA) is 0
                       (0 << 6) |       // SPI polarity (CPOL) is 0
                       (0 << 4) |       // Freescale frame format
                       (7 << 0));       // 8-bit data

    // enable SSI1
    SSI1_CR1_R     |= SSI_CR1_SSE;

    ////////////////////////////////////////////////////////////////
    // Setup parent UART connection on UART 0

    ////////////////////////////////////////////////////////////////
    // Setup child 1 UART connection on UART 5

    ////////////////////////////////////////////////////////////////
    // Setup child 2 UART connection on UART 7

    ////////////////////////////////////////////////////////////////
    // Setup Debug UART on UART 4
    // PC4 and PC5 should be debug RX and TX respectively
    GPIO_PORTC_AFSEL_R |=  ((1 << 5) | (1 << 4));
    GPIO_PORTC_ODR_R   &= ~((1 << 5) | (1 << 4));
    GPIO_PORTC_DEN_R   |=  ((1 << 5) | (1 << 4));
    GPIO_PORTC_PCTL_R  &= ~(GPIO_PCTL_PC5_M | GPIO_PCTL_PC4_M);
    GPIO_PORTC_PCTL_R  |=  (GPIO_PCTL_PC5_XXXX | GPIO_PCTL_PC4_XXXX);

    // enable UART



}
