#include "relay.h"
#include "uart.h"
#include "tm4c123gh6pm.h"
#include "sevenseg.h"
#include <stdint.h>

static void init_hw();

const uint8_t COMMAND_CODE_MAP_OUTPUT = 'o';

const uint8_t COMMAND_CODE_ENABLE_CONFIG_SANITY_CHECKING = 'C';
const uint8_t COMMAND_CODE_DISABLE_CONFIG_SANITY_CHECKING = 'c';
const uint8_t COMMAND_CODE_ENABLE_RELAY_OUTPUT = 'E';
const uint8_t COMMAND_CODE_DISABLE_RELAY_OUTPUT = 'e';

const uint8_t COMMAND_IDLE = 0;
const uint8_t COMMAND_ACTIVE = 1;

typedef struct command {
    uint8_t payload[256];
} command_t;

static int8_t hex_digit_to_number(char ch) {
    if ((ch >= '0') && (ch <= '9')) return (ch - '0');
    if ((ch >= 'A') && (ch <= 'F')) return (ch - 'A' + 10);
    if ((ch >= 'a') && (ch <= 'f')) return (ch - 'a' + 10);
    return -1;
}

int main()
{
    uart_init_buffers();
    init_hw();

    asm volatile("cpsie i\r\n\t");

    for (int i = 0; i < NUM_SEGS; i++) {
        for (int k = 0; k < 2; k++) {
            pixmap[i][k] = i * (k + 1);
        }
    }

    uint8_t relay_grid[RELAY_MAP_NUM_INPUTS * RELAY_MAP_NUM_OUTPUTS] = { 0 };
    relay_state_t relay_state = {
        .num_inputs = RELAY_MAP_NUM_INPUTS,
        .num_outputs = RELAY_MAP_NUM_OUTPUTS,
        .grid = relay_grid
    };
    set_relays(&relay_state);
    relays_enable();

    int escape = 0;
    int command_index = 0;
    int command_valid = 0;
    int command_state = COMMAND_IDLE;
    uint8_t pending_command[256];
    while(1) {
        // poll parent UART for new characters
        int ch;
        while (!command_valid &&
               ((ch = uart0_getch()) != -1)) {
            uart0_putch(ch);
            if (command_state == COMMAND_IDLE) {
                command_index = 0;
                if (ch == '<') {
                    command_state = COMMAND_ACTIVE;
                }
            } else if (command_state == COMMAND_ACTIVE) {
                if (!escape && (ch == '<')) {
                    command_index = 0;
                } else if (!escape && (ch == '>')) {
                    command_valid = 1;
                    command_state = COMMAND_IDLE;
                } else if (!escape && (ch == '=')) {
                    escape = 1;
                } else {
                    pending_command[command_index] = ch;
                    escape = 0;
                }

                command_index++;
            }
        }

        // execute parsed command
        if (command_valid) {
            switch (pending_command[0]) {
                case 'o': {
                    int input_relay = hex_digit_to_number(pending_command[2]);
                    int output_relay = hex_digit_to_number(pending_command[1]);

                    if ((output_relay >= 0) && (output_relay < 8)) {
                        relay_map[output_relay] = input_relay;
                    }

                    set_relays(relay_map);
                    break;
                }

                case 'E': {
                    relays_enable();
                    break;
                }

                case 'e': {
                    relays_disable();
                    break;
                }
            }
            command_valid = 0;
        }

        // update display
        display_relay_map(relay_map, pixmap);
    }
}

#define SYSTICK_CSR *((volatile uint32_t*)0xe000e010)
#define SYSTICK_RVR *((volatile uint32_t*)0xe000e014)
#define SYSTICK_CVR *((volatile uint32_t*)0xe000e018)

static void init_hw()
{
    // Setup SYSTICK to loop
    SYSTICK_RVR = 0x00ffffff;
    SYSTICK_CSR = 1;

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
    SSI1_CR0_R      = ((255 << 8) |       // divide clock down by factor of 8 with prescaler 1
                       (0 << 7) |       // SPI phase (CPHA) is 0
                       (0 << 6) |       // SPI polarity (CPOL) is 0
                       (0 << 4) |       // Freescale frame format
                       (7 << 0));       // 8-bit data

    // enable SSI1
    SSI1_CR1_R     |= SSI_CR1_SSE;

    ////////////////////////////////////////////////////////////////
    // Setup parent UART connection on UART 0
    // PA0 and PA1 are controlled by the UART.
    GPIO_PORTA_AFSEL_R |=  ((1 << 1) | (1 << 0));
    GPIO_PORTA_ODR_R   &= ~((1 << 1) | (1 << 0));
    GPIO_PORTA_DEN_R   |=  ((1 << 1) | (1 << 0));
    GPIO_PORTA_PCTL_R  &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M);
    GPIO_PORTA_PCTL_R  |=  (GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX);

    // enable UART0
    SYSCTL_RCGCUART_R |= (1 << 0);
    for (volatile int i = 0; i < 100; i++);

    // configure UART0
    UART0_CTL_R &= ~UART_CTL_UARTEN;
    UART0_IBRD_R = ((16000000 * 64) / (16 * 115200)) / 64;
    UART0_FBRD_R = ((16000000 * 64) / (16 * 115200)) % 64;
    UART0_LCRH_R = (UART_LCRH_WLEN_8);          // bytes are 8 bits
    UART0_CC_R   =  UART_CC_CS_SYSCLK;          // select sysclk for UART0
    UART0_CTL_R |=  UART_CTL_UARTEN;

    // enable RX interrupt
    UART0_IM_R = UART_IM_RXIM;

    (&NVIC_EN0_R)[(INT_UART0 - 16) / 32] |= (1ul << ((INT_UART0 - 16) % 32));

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
    GPIO_PORTC_PCTL_R  |=  (GPIO_PCTL_PC5_U4TX | GPIO_PCTL_PC4_U4RX);

    // enable UART 4
    SYSCTL_RCGCUART_R |= (1 << 4);
    for (volatile int i = 0; i < 100; i++);

    // Configure UART4
    UART4_CTL_R &= ~UART_CTL_UARTEN;
    UART4_IBRD_R = ((16000000 * 64) / (16 * 115200)) / 64;
    UART4_FBRD_R = ((16000000 * 64) / (16 * 115200)) % 64;
    UART4_LCRH_R = (UART_LCRH_WLEN_8);          // bytes are 8 bits
    UART4_CC_R   =  UART_CC_CS_SYSCLK;          // select sysclk for UART4
    UART4_CTL_R |=  UART_CTL_UARTEN;

    (&NVIC_EN0_R)[(INT_UART4 - 16) / 32] |= (1ul << ((INT_UART4 - 16) % 32));
}
