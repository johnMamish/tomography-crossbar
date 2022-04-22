#include "uart.h"
#include "tm4c123gh6pm.h"

uint8_t uart0_rx_buffer_space[UART0_RX_BUFFER_SIZE];
saeclib_circular_buffer_t uart0_rx_buffer;
uint8_t uart0_tx_buffer_space[UART0_TX_BUFFER_SIZE];
saeclib_circular_buffer_t uart0_tx_buffer;

uint8_t uart4_rx_buffer_space[UART4_RX_BUFFER_SIZE];
saeclib_circular_buffer_t uart4_rx_buffer;
uint8_t uart4_tx_buffer_space[UART4_TX_BUFFER_SIZE];
saeclib_circular_buffer_t uart4_tx_buffer;

void uart_init_buffers()
{
    saeclib_circular_buffer_init((saeclib_circular_buffer_t*)(&uart0_rx_buffer),
                                 uart0_rx_buffer_space,
                                 sizeof(uart0_rx_buffer_space),
                                 sizeof(uint8_t));
    saeclib_circular_buffer_init((saeclib_circular_buffer_t*)(&uart0_tx_buffer),
                                 uart0_tx_buffer_space,
                                 sizeof(uart0_tx_buffer_space),
                                 sizeof(uint8_t));

    saeclib_circular_buffer_init((saeclib_circular_buffer_t*)(&uart4_rx_buffer),
                                 uart4_rx_buffer_space,
                                 sizeof(uart4_rx_buffer_space),
                                 sizeof(uint8_t));
    saeclib_circular_buffer_init((saeclib_circular_buffer_t*)(&uart4_tx_buffer),
                                 uart4_tx_buffer_space,
                                 sizeof(uart4_tx_buffer_space),
                                 sizeof(uint8_t));
}

void uart0_putch(char ch)
{
    asm volatile("cpsid if\r\n\t");

    if ((UART0_FR_R & UART_FR_TXFE) &&
        (saeclib_circular_buffer_empty(&uart0_tx_buffer))) {
        // if there are no characters in the buffer already and the UART is empty, we can just put
        // the new character directly in the tx buffer.
        UART0_DR_R = ch;
    } else {
        // otherwise, we need to add it to the end of the buffer and make sure that interrupts are
        // enabled
        saeclib_circular_buffer_pushone(&uart0_tx_buffer, &ch);
        UART0_IM_R |= UART_IM_TXIM;
    }

    asm volatile("cpsie if\r\n\t");
}

int uart0_getch()
{
    uint8_t retval;

    asm volatile("cpsid if\r\n\t");

    saeclib_error_e er = saeclib_circular_buffer_popone(&uart0_rx_buffer, &retval);

    asm volatile("cpsie if\r\n\t");
    if (er == SAECLIB_ERROR_UNDERFLOW) {
        return -1;
    } else {
        return (int)retval;
    }
}

void uart4_putch(char ch)
{
    asm volatile("cpsid if\r\n\t");

    if ((UART4_FR_R & UART_FR_TXFE) &&
        (saeclib_circular_buffer_empty(&uart4_tx_buffer))) {
        // if there are no characters in the buffer already and the UART is empty, we can just put
        // the new character directly in the tx buffer.
        UART4_DR_R = ch;
    } else {
        // otherwise, we need to add it to the end of the buffer and make sure that interrupts are
        // enabled
        saeclib_circular_buffer_pushone(&uart4_tx_buffer, &ch);
        UART4_IM_R |= UART_IM_TXIM;
    }

    asm volatile("cpsie if\r\n\t");
}

int uart4_getch()
{
    uint8_t retval;

    asm volatile("cpsid if\r\n\t");

    saeclib_error_e er = saeclib_circular_buffer_popone(&uart4_rx_buffer, &retval);

    asm volatile("cpsie if\r\n\t");
    if (er == SAECLIB_ERROR_UNDERFLOW) {
        return -1;
    } else {
        return (int)retval;
    }
}

void uart5_putch(char ch)
{

}

int uart5_getch()
{
    return -1;
}

void uart7_putch(char ch)
{

}

int uart7_getch()
{
    return -1;
}

void UART0Handler()
{
    if (UART0_MIS_R & UART_MIS_RXMIS) {
        // uart rx interrupt triggered
        uint8_t ch = UART0_DR_R;
        saeclib_circular_buffer_pushone(&uart0_rx_buffer, &ch);
    }

    if (UART0_MIS_R & UART_MIS_TXMIS) {
        // uart tx interrupt triggered
        if (saeclib_circular_buffer_empty(&uart0_tx_buffer)) {
            // disable the interrupt, there are no characters left to send.
            UART0_IM_R &= ~UART_IM_TXIM;
        } else {
            // transmit the next character in the circular buffer
            uint8_t ch;
            saeclib_circular_buffer_popone(&uart0_tx_buffer, &ch);
            UART0_DR_R = ch;
        }
    }
}


void UART4Handler()
{
    if (UART4_MIS_R & UART_MIS_RXMIS) {
        // uart rx interrupt triggered
    }

    if (UART4_MIS_R & UART_MIS_TXMIS) {
        // uart tx interrupt triggered
        if (saeclib_circular_buffer_empty(&uart4_tx_buffer)) {
            // disable the interrupt, there are no characters left to send.
            UART4_IM_R &= ~UART_IM_RXIM;
        } else {
            // transmit the next character in the circular buffer
            uint8_t ch;
            saeclib_circular_buffer_popone(&uart4_tx_buffer, &ch);
            UART4_DR_R = ch;
        }
    }
}
