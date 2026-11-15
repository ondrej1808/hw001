/*
 * UART2 pres ST-LINK virtual COM port (USB): PA2 = TX, PA3 = RX, 115200 8N1.
 * Prijem v preruseni: ulozi znak a nastavi flag run_rx_uart, zpracovani v main.
 */
#ifndef UART_H_HDR
#define UART_H_HDR

#include <stdbool.h>
#include <stdint.h>

#define UART_BAUDRATE 115200U

extern volatile bool run_rx_uart;   /*< nastavi IRQ po prijmu znaku, maze main */
extern volatile char uart_rx_char;  /*< posledni prijaty znak */

/** Nastav USART2 (PA2/PA3) a povol preruseni od prijmu **/
void uartInit(void);

/** Odesli jeden znak (blokujici, ceka na volny TX registr) **/
void uart_putc(char c);

#endif /* UART_H_HDR */
