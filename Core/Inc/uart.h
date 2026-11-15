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

#define UART_TX_BUF_SIZE 128U   /*< kapacita odesilaciho bufferu */

/** Odesli jeden znak (blokujici, ceka na volny TX registr) **/
void uart_putc(char c);

/** Vloz znak do odesilaciho bufferu (neblokujici).
  * \return 1 = vlozeno, 0 = buffer plny **/
uint32_t uart_send(char c);

/** Vloz retezec do odesilaciho bufferu. \return pocet vlozenych znaku **/
uint32_t uart_send_str(const char *s);

/** TX uloha: kdyz je volny TX registr a v bufferu neco je, odesle 1 znak.
  * Volat co nejcasteji (v kazdem pruchodu hlavni smycky). **/
void uart_tx_task(void);

#endif /* UART_H_HDR */
