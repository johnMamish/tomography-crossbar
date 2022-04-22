#ifndef _UART_H
#define _UART_H

#include "saeclib/src/saeclib_circular_buffer.h"

#define UART0_RX_BUFFER_SIZE 128
extern saeclib_circular_buffer_t uart0_rx_buffer;
#define UART0_TX_BUFFER_SIZE 256
extern saeclib_circular_buffer_t uart0_tx_buffer;

#define UART4_RX_BUFFER_SIZE 128
extern saeclib_circular_buffer_t uart4_rx_buffer;
#define UART4_TX_BUFFER_SIZE 256
extern saeclib_circular_buffer_t uart4_tx_buffer;

/**
 * This function should be called before the UARTs are turned on.
 */
void uart_init_buffers();


void uart0_putch(char ch);
int uart0_getch();

void uart4_putch(char ch);
int uart4_getch();

void uart5_putch(char ch);
int uart5_getch();

void uart7_putch(char ch);
int uart7_getch();

#endif
