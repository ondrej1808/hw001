/*
 * UART2 pres ST-LINK virtual COM port (USB): PA2 = TX, PA3 = RX, 115200 8N1.
 */
#include "uart.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"

#define UART_PCLK_HZ 16000000U   /* APB1 = HSI 16 MHz, bez delicky */

volatile bool run_rx_uart  = false;
volatile char uart_rx_char = 0;

/**
 * @brief  Nastavi PA2/PA3 na AF7 (USART2), USART2 na 115200 8N1, povoli RX preruseni.
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOA a USART2, povoli USART2_IRQn v NVIC.
 */
void uartInit(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  /* PA2 = TX, PA3 = RX, AF7 */
  GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2_Msk | GPIO_AFRL_AFSEL3_Msk);
  GPIOA->AFR[0] |= (GPIO_AF7_USART2 << GPIO_AFRL_AFSEL2_Pos) |
                   (GPIO_AF7_USART2 << GPIO_AFRL_AFSEL3_Pos);
  GPIOA->MODER  &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
  GPIOA->MODER  |= (GPIO_MODE_AF_PP << GPIO_MODER_MODE2_Pos) |
                   (GPIO_MODE_AF_PP << GPIO_MODER_MODE3_Pos);

  /* 16 MHz / 115200 = 138.9 -> BRR = 139 (oversampling 16) */
  USART2->BRR = (UART_PCLK_HZ + UART_BAUDRATE / 2U) / UART_BAUDRATE;
  USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

  NVIC_EnableIRQ(USART2_IRQn);
}

/**
 * @brief  Odesle jeden znak, ceka na volny datovy registr.
 * @param  c  Znak k odeslani.
 * @return Nic.
 */
void uart_putc(char c)
{
  while (!(USART2->SR & USART_SR_TXE)) { }
  USART2->DR = (uint8_t)c;
}

/**
 * @brief  Preruseni USART2: pri prijmu ulozi znak a nastavi flag run_rx_uart.
 * @param  None.
 * @return Nic. Cteni DR maze RXNE i pripadny overrun (ORE).
 */
void USART2_IRQHandler(void)
{
  uint32_t sr = USART2->SR;
  if (sr & (USART_SR_RXNE | USART_SR_ORE))
  {
    uart_rx_char = (char)USART2->DR;
    run_rx_uart  = true;
  }
}
