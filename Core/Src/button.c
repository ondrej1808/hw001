/*
 * Obsluha tlacitka SW1 (PA0, active low, externi pull-up 10k)
 */
#include "button.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio.h"

#define BUTTON_DEBOUNCE_SAMPLES 3U   /* stejny stav 3x po sobe = 30 ms */

static uint32_t btn_stable;   /* odruseny stav: 1 = stisknuto */
static uint32_t btn_cnt;      /* pocet vzorku odlisnych od btn_stable */

/**
 * @brief  Nastavi PA0 (SW1) jako vstup bez interniho pull (pull-up je na desce).
 * @param  None.
 * @return Nic. Side effect: zapne hodiny GPIOA.
 */
void button_init(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  GPIOA->MODER &= ~GPIO_MODER_MODE0_Msk;                          /* input */
  GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD0_Msk;
  GPIOA->PUPDR |= GPIO_NOPULL << GPIO_PUPDR_PUPD0_Pos;
  btn_stable = 0;
  btn_cnt    = 0;
}

/**
 * @brief  Precte SW1, odrusi zakmity a detekuje stisk.
 *         Volat periodicky kazdych BUTTON_TASK_PERIOD_MS.
 * @param  None.
 * @return 1 prave v okamziku stisku (po odruseni), jinak 0.
 */
uint32_t button_task(void)
{
  uint32_t raw = (GPIOA->IDR & GPIO_IDR_ID0) ? 0U : 1U;   /* active low */

  if (raw == btn_stable)
  {
    btn_cnt = 0;
    return 0;
  }

  if (++btn_cnt < BUTTON_DEBOUNCE_SAMPLES)
  {
    return 0;
  }

  btn_cnt    = 0;
  btn_stable = raw;
  return btn_stable;   /* 1 jen pri prechodu do stisknuto */
}
